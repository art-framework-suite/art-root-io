find_package(Boost COMPONENTS filesystem REQUIRED PUBLIC)
find_package(Boost PRIVATE COMPONENTS date_time headers program_options REQUIRED)
find_package(ROOT COMPONENTS Tree RIO Core REQUIRED PUBLIC)
find_package(ROOT PRIVATE COMPONENTS Hist REQUIRED)
find_package(SQLite3 PRIVATE REQUIRED)
find_package(art REQUIRED PUBLIC)
find_package(canvas REQUIRED PUBLIC)
find_package(canvas_root_io PRIVATE REQUIRED)
find_package(cetlib REQUIRED PUBLIC)
find_package(cetlib_except PRIVATE REQUIRED)
find_package(fhiclcpp REQUIRED PUBLIC)
find_package(hep_concurrency REQUIRED PUBLIC)
find_package(messagefacility REQUIRED PUBLIC)

include(ArtDictionary)
include(BuildPlugins)

art_dictionary(DICTIONARY_LIBRARIES PRIVATE art::Framework_IO_ProductMix)

add_subdirectory(RootDB)

cet_make_library(LIBRARY_NAME art_root_io_detail
  SOURCE
    detail/RangeSetInfo.cc
    detail/RootErrorClassifier.cc
    detail/dropBranch.cc
    detail/getEntry.cc
    detail/getObjectRequireDict.cc
    detail/rangeSetFromFileIndex.cc
    detail/resolveRangeSet.cc
    detail/rootFileSizeTools.cc
    detail/rootOutputConfigurationTools.cc
  LIBRARIES
  PUBLIC
    art::Framework_Core
    art::Framework_Principal
    art::Persistency_Common
    canvas::canvas
    cetlib::sqlite
    cetlib::cetlib
    ROOT::Core
    ROOT::RIO
    ROOT::Tree
  PRIVATE
    canvas_root_io::canvas_root_io
    art::Framework_Services_Registry
    messagefacility::MF_MessageLogger
    fhiclcpp::types
    fhiclcpp::fhiclcpp
    cetlib::container_algorithms
    hep_concurrency::hep_concurrency
  )

cet_make_library(HEADERS_TARGET
  SOURCE
    DropMetaData.cc
    DummyProductCache.cc
    DuplicateChecker.cc
    FastCloningInfoProvider.cc
    GetFileFormatEra.cc
    GetFileFormatVersion.cc
    RootBranchInfo.cc
    RootBranchInfoList.cc
    RootDelayedReader.cc
    RootIOPolicy.cc
    RootInputFile.cc
    RootInputFileSequence.cc
    RootOutputFile.cc
    RootOutputTree.cc
    checkDictionaries.cc
    setup.cc
  LIBRARIES
  PUBLIC
    art::Framework_IO_Catalog
    art::Framework_IO
    art::Framework_Core
    art::Framework_Principal
    art::Persistency_Common
    art::Persistency_Provenance
    canvas::canvas
    fhiclcpp::types
    cetlib::container_algorithms
    cetlib::sqlite
    cetlib::cetlib
    ROOT::Tree
    ROOT::RIO
    ROOT::Core
    Boost::filesystem
  PRIVATE
    art_root_io::detail
    art_root_io::RootDB
    art::Framework_IO_detail
    art::Framework_Services_Registry
    art::Framework_Services_System_FileCatalogMetadata_service
    art::Utilities
    canvas_root_io::canvas_root_io
    messagefacility::MF_MessageLogger
    fhiclcpp::fhiclcpp
    hep_concurrency::macros
    Boost::date_time
    ROOT::Hist
  )

cet_make_library(LIBRARY_NAME art_root_io_tfile_support
  SOURCE TFileDirectory.cc detail/RootDirectorySentry.cc
  LIBRARIES
  PUBLIC ROOT::Core
  PRIVATE
    art_root_io::art_root_io
    canvas::canvas
    ROOT::Hist
    ROOT::RIO
  )

build_plugin(TFileService "service" ALLOW_UNDERSCORES LIBRARIES
  PUBLIC
    art_root_io::tfile_support
    art::Framework_IO
    art::Framework_Core
    fhiclcpp::types
    hep_concurrency::hep_concurrency
  PRIVATE
    art::Framework_IO_detail
    art::Framework_Principal
    art::Framework_Services_Registry
    art::Utilities
    fhiclcpp::fhiclcpp
    ROOT::RIO
    ROOT::Core
  )

build_plugin(RootInput "source" BASE DRISISource ALLOW_UNDERSCORES
  LIBRARIES
  PUBLIC
    art_root_io::art_root_io
    art::Framework_IO_Catalog
    canvas::canvas
    messagefacility::MF_MessageLogger
    fhiclcpp::types
    ROOT::Tree
  PRIVATE
    art::Framework_Principal
    canvas::canvas
  )

build_plugin(RootOutput "module" ALLOW_UNDERSCORES
  LIBRARIES REG
    art_root_io::detail
    art_root_io::art_root_io
    art::Framework_IO
    art::Framework_IO_detail
    art::Framework_Core
    art::Framework_Principal
    art::Utilities
    canvas::canvas
    messagefacility::MF_MessageLogger
    fhiclcpp::types
    fhiclcpp::fhiclcpp
    hep_concurrency::hep_concurrency
  )

build_plugin(SamplingInput "source" BASE InputSource ALLOW_UNDERSCORES
  REG_SOURCE
    SamplingInput_source.cc
    detail/DataSetBroker.cc
    detail/DataSetSampler.cc
    detail/SamplingDelayedReader.cc
    detail/SamplingInputFile.cc
    detail/event_start.cc
  LIBRARIES REG
    SQLite::SQLite3
    art_root_io::RootDB
    art_root_io::art_root_io
    art_root_io::detail
    canvas_root_io::canvas_root_io
    art::Framework_Core
    art::Framework_Principal
    art::Persistency_Provenance
    canvas::canvas
    messagefacility::MF_MessageLogger
    fhiclcpp::types
)

cet_make_exec(NAME product_sizes_dumper
  SOURCE product_sizes_dumper.cc RootSizeOnDisk.cc
  LIBRARIES PRIVATE
    art_root_io::art_root_io
    art_root_io::detail
    cetlib::container_algorithms
    cetlib::parsed_program_options
    Boost::program_options
    ROOT::RIO
    ROOT::Core
)

cet_make_exec(NAME config_dumper LIBRARIES PRIVATE
  art_root_io::RootDB
  art_root_io::art_root_io
  canvas::canvas
  fhiclcpp::fhiclcpp
  cetlib::container_algorithms
  cetlib::parsed_program_options
  cetlib::cetlib
  Boost::program_options
  ROOT::Tree
  ROOT::RIO
)

cet_make_exec(NAME sam_metadata_dumper LIBRARIES PRIVATE
  art::Utilities
  art_root_io::art_root_io
  art_root_io::RootDB
  cetlib::parsed_program_options
  Boost::program_options
  ROOT::RIO
  ROOT::Core
)

cet_make_exec(NAME count_events LIBRARIES PRIVATE
  canvas::canvas
  cetlib::parsed_program_options
  Boost::program_options
  ROOT::Tree
  ROOT::RIO
  ROOT::Core
)

cet_make_exec(NAME file_info_dumper
  SOURCE file_info_dumper.cc detail/InfoDumperInputFile.cc
  LIBRARIES PRIVATE
    art_root_io::RootDB
    art_root_io::detail
    art_root_io::art_root_io
    canvas::canvas
    cetlib::container_algorithms
    cetlib::parsed_program_options
    cetlib::cetlib
    Boost::program_options
)

include(CetMakeCompletions)
cet_make_completions(product_sizes_dumper)
cet_make_completions(config_dumper)
cet_make_completions(sam_metadata_dumper)
cet_make_completions(count_events)
cet_make_completions(file_info_dumper)

install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)

add_subdirectory(test)
