#include "art_root_io/GetFileFormatEra.h"
#include "art_root_io/RootDB/SQLite3Wrapper.h"
#include "art_root_io/RootDB/tkeyvfs.h"
#include "canvas/Persistency/Provenance/FileFormatVersion.h"
#include "canvas/Persistency/Provenance/ParameterSetBlob.h"
#include "canvas/Persistency/Provenance/ParameterSetMap.h"
#include "canvas/Persistency/Provenance/rootNames.h"
#include "cetlib/container_algorithms.h"
#include "cetlib/exempt_ptr.h"
#include "cetlib/parsed_program_options.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/ParameterSetRegistry.h"

#include "boost/program_options.hpp"
#include "range/v3/view.hpp"

#include "TFile.h"
#include "TTree.h"

#include "sqlite3.h"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace bpo = boost::program_options;

using art::ParameterSetBlob;
using art::ParameterSetMap;
using fhicl::ParameterSet;
using std::back_inserter;
using std::cerr;
using std::cout;
using std::endl;
using std::ostream;
using std::string;
using std::vector;

using stringvec = vector<string>;

enum class PsetType { MODULE, SERVICE, PROCESS };

size_t
db_size(sqlite3* db)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, "PRAGMA page_size;", -1, &stmt, nullptr);
  sqlite3_step(stmt);
  size_t page_size = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db, "PRAGMA page_count;", -1, &stmt, nullptr);
  sqlite3_step(stmt);
  size_t page_count = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return page_size * page_count;
}

std::string
db_size_hr(sqlite3* db)
{
  std::string result;
  double size = db_size(db);
  std::vector<std::string> units = {"b", "KiB", "MiB", "GiB", "TiB"};
  auto unit = units.cbegin(), end = units.cend();
  while (size > 1024.0 && unit != end) {
    size /= 1024.0;
    ++unit;
  }
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(1) << size << " " << *unit;
  result = ss.str();
  return result;
}

std::string
want_pset(ParameterSet const& ps, stringvec const& filters, PsetType mode)
{
  string label;
  switch (mode) {
  case PsetType::MODULE:
    ps.get_if_present<string>("module_label", label);
    break;
  case PsetType::SERVICE:
    ps.get_if_present<string>("service_provider", label) ||
      ps.get_if_present<string>("service_type", label);
    break;
  case PsetType::PROCESS: {
    fhicl::ParameterSet dummy;
    if (ps.get_if_present("source", dummy)) {
      ps.get_if_present<string>("process_name", label);
    }
  } break;
  default:
    throw std::string("INTERNAL ERROR: unknown mode ")
      .append(std::to_string(int(mode)))
      .append(".");
  }
  return (filters.empty() || label.empty() || cet::search_all(filters, label)) ?
           label :
           std::string();
}

ParameterSet
strip_pset(ParameterSet const& ps, PsetType mode)
{
  ParameterSet result(ps);
  switch (mode) {
  case PsetType::MODULE:
    result.erase("module_label");
    break;
  case PsetType::SERVICE:
    result.erase("service_type");
    result.erase("service_provider");
    break;
  case PsetType::PROCESS:
    result.erase("process_name");
    break;
  default:
    throw std::string("INTERNAL ERROR: unknown mode ")
      .append(std::to_string(int(mode)))
      .append(".");
  }
  return result;
}

// Read all the ParameterSets stored in 'file'. Write any error messages
// to errors.  Return false on failure, and true on success.
bool
read_all_parameter_sets(TFile& file, ostream& errors)
{
  ParameterSetMap psm;
  ParameterSetMap* psm_address = &psm;
  // Find the TTree that holds this data.
  std::unique_ptr<TTree> metadata_tree{
    file.Get<TTree>(art::rootNames::metaDataTreeName().c_str())};
  if (!metadata_tree) {
    errors << "Unable to find the metadata tree in file '" << file.GetName()
           << "';\nthis may not be an ART event data file.\n";
    return false;
  }
  if (metadata_tree->GetBranch(
        art::rootNames::metaBranchRootName<ParameterSetMap>())) {
    metadata_tree->SetBranchAddress(
      art::rootNames::metaBranchRootName<ParameterSetMap>(), &psm_address);
  }
  art::FileFormatVersion ffv;
  art::FileFormatVersion* ffv_address = &ffv;
  metadata_tree->SetBranchAddress(
    art::rootNames::metaBranchRootName<art::FileFormatVersion>(), &ffv_address);
  long bytes_read = metadata_tree->GetEntry(0);
  if (bytes_read < 0) {
    errors << "Unable to read the metadata tree in file '" << file.GetName()
           << ";\nthis file appears to be corrupted.\n";
    return false;
  }
  // Check version
  std::string const expected_era = art::getFileFormatEra();
  if (ffv.era_ != expected_era) {
    errors << "Can only read files written during the \"" << expected_era
           << "\" era: "
           << "Era of "
           << "\"" << file.GetName() << "\" was "
           << (ffv.era_.empty() ? "not set" : ("set to \"" + ffv.era_ + "\" "))
           << ".\n";
    return false;
  }
  for (auto const& pr : psm) {
    // Read the next ParameterSet directly into the output vector.
    fhicl::ParameterSetRegistry::put(
      fhicl::ParameterSet::make(pr.second.pset_));
  }
  if (ffv.value_ >= 5) { // Should have metadata DB.
    // Open the DB
    art::SQLite3Wrapper sqliteDB(&file, "RootFileDB");
    std::cout << "# Read SQLiteDB from file, total size: "
              << db_size_hr(sqliteDB) << ".\n"
              << std::endl;
    fhicl::ParameterSetRegistry::importFrom(sqliteDB);
    fhicl::ParameterSetRegistry::stageIn();
  }
  return true;
}

// Extract all the requested module configuration ParameterSets (for
// modules with the given labels, run as part of processes of the given
// names) from the given TFIle. An empty list of process names means
// select all process names; an empty list of module labels means select
// all modules. The ParameterSets are written to the stream output, and
// error messages are written to the stream errors.
//
// Returns 0 to indicate success, and 1 on failure.
// Precondition: file.IsZombie() == false

// Caution: We pass 'file' by non-const reference because the TFile interface
// does not declare the functions we use to be const, even though they do not
// modify the underlying file.
int
print_pset_from_file(TFile& file,
                     stringvec const& filters,
                     PsetType const mode,
                     ostream& output,
                     ostream& errors)
{
  if (!read_all_parameter_sets(file, errors)) {
    errors << "Unable to to read parameter sets.\n";
    return 1;
  }

  // Print ParameterSets alphabetically according to the top-level
  // name.  There is an additional request to sort according to
  // chronology--i.e. presumably first according to process name, and
  // then alphabetically within.
  auto const& collection = fhicl::ParameterSetRegistry::get();

  // Cache pointers to the ParameterSets to avoid exorbitant copying.
  std::map<std::string, cet::exempt_ptr<fhicl::ParameterSet const>> sorted_pses;
  for (auto const& pset : collection | ranges::views::values) {
    std::string const label{want_pset(pset, filters, mode)};
    if (label.empty())
      continue;

    sorted_pses.emplace(label, &pset);
  }

  for (auto const& [key, pset_ptr] : sorted_pses) {
    output << key << ": {\n";
    output << strip_pset(*pset_ptr, mode).to_indented_string(1);
    output << "}\n\n";
  }

  return 0;
}

// Extract all the requested module configuration ParameterSets (for
// modules with the given labels, run as part of processes of the given
// names) from the named files. An empty list of process names means
// select all process names; an empty list of module labels means select
// all modules. The ParameterSets are written to the stream output, and
// error messages are written to the stream errors.
//
// The return value is the number of files in which errors were
// encountered, and is thus 0 to indicate success.
int
print_psets_from_files(stringvec const& file_names,
                       stringvec const& filters,
                       PsetType const mode,
                       ostream& output,
                       ostream& errors)
{
  int rc{0};
  for (auto const& file_name : file_names) {
    std::unique_ptr<TFile> current_file{TFile::Open(file_name.c_str(), "READ")};
    if (!current_file || current_file->IsZombie()) {
      ++rc;
      errors << "Unable to open file '" << file_name << "' for reading."
             << "\nSkipping to next file.\n";
    } else {
      std::cout << "=============================================\n";
      std::cout << "Processing file: " << file_name << std::endl;
      rc += print_pset_from_file(*current_file, filters, mode, output, errors);
    }
  }
  return rc;
}

int
main(int argc, char** argv)
{
  std::ostringstream descstr;
  descstr << argv[0] << " <PsetType> <options> [<source-file>]+\nOptions";
  bpo::options_description desc(descstr.str());
  // clang-format off
  desc.add_options()
    ("filter,f",
       bpo::value<stringvec>()->composing(),
       "Only entities whose identifier (label (M), service type (S) "
       "or process name (P)) match (multiple OK).")
    ("help,h", "this help message.")
    ("modules,M", "PsetType: print module configurations (default).")
    ("source,s",
       bpo::value<stringvec>()->composing(), "source data file (multiple OK).")
    ("services,S", "PsetType: print service configurations.")
    ("process,P", "PsetType: print process configurations.");
  // clang-format on

  bpo::options_description all_opts("All Options.");
  all_opts.add(desc);

  // Each non-option argument is interpreted as the name of a file to be
  // processed. Any number of filenames is allowed.
  bpo::positional_options_description pd;
  pd.add("source", -1);

  auto const vm = cet::parsed_program_options(argc, argv, all_opts, pd);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  // Process the mode information.
  PsetType mode;
  if (vm.count("services")) {
    mode = PsetType::SERVICE;
  } else if (vm.count("process")) {
    mode = PsetType::PROCESS;
  } else {
    mode = PsetType::MODULE;
  }

  // Obtain any filtering names to limit what we show.
  stringvec filters;
  if (vm.count("filter")) {
    cet::copy_all(vm["filter"].as<stringvec>(), std::back_inserter(filters));
  }

  // Get the names of the files we will process.
  stringvec file_names;
  size_t file_count = vm.count("source");
  if (file_count < 1) {
    cerr << "ERROR: One or more input files must be specified;"
         << " supply filenames as program arguments\n"
         << "For usage and options list, please do 'config_dumper --help'.\n";
    return 3;
  }
  file_names.reserve(file_count);
  cet::copy_all(vm["source"].as<stringvec>(), std::back_inserter(file_names));

  // Register the tkey VFS with sqlite:
  tkeyvfs_init();

  // Do the work.
  return print_psets_from_files(file_names, filters, mode, cout, cerr);
}
