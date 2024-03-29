#include "art_root_io/detail/event_start.h"

#include "boost/algorithm/string.hpp"
#include "canvas/Utilities/Exception.h"

#include <regex>
#include <sstream>

namespace {

  // Numbers are allowed to have leading and trailing whitespace.
  auto
  group(std::string const& spec)
  {
    return "(" + spec + ")";
  }

  std::string const number{R"(\s*\d+\s*)"};
  std::regex const re_event_id{group(number) + ":" + group(number) + ":" +
                               group(number)};
  std::string const context{
    "An error was encountered while processing the 'skipToEvent' parameter.\n"};

  constexpr auto article [[gnu::unused]] (art::Level const L)
  {
    return (L == art::Level::InputFile || L == art::Level::Event) ? "an" : "a";
  }

  template <art::Level L>
  auto
  safe_conversion(std::string str_num)
  {
    boost::trim(str_num);
    auto const num = std::stoull(str_num);

    if (num > art::IDNumber<L>::max_valid() ||
        num < art::IDNumber<L>::first()) {
      // No need to provide exception message since it will be
      // included in the rethrown exception.
      throw art::Exception{art::errors::Configuration};
    }
    return static_cast<art::IDNumber_t<L>>(num);
  }

  template <art::Level L>
  auto
  range()
  {
    std::stringstream oss;
    oss << '[' << art::IDNumber<L>::first() << ", "
        << art::IDNumber<L>::invalid() << ')';
    return oss.str();
  }

  [[noreturn]] void
  throw_configuration_exception(std::string const& spec) noexcept(false)
  {
    throw art::Exception{art::errors::Configuration, context}
      << "The specification '" << spec << "' is not a valid EventID.\n"
      << "Please specify a value of the form '<run>:<subrun>:<event>' where:\n"
      << "  <run>    is in the range " << range<art::Level::Run>() << '\n'
      << "  <subrun> is in the range " << range<art::Level::SubRun>() << '\n'
      << "  <event>  is in the range " << range<art::Level::Event>() << '\n';
  }

  template <art::Level L>
  auto
  convert_or_throw(std::string const& field,
                   std::string const& event_spec) noexcept(false)
  try {
    return safe_conversion<L>(field);
  }
  catch (...) {
    throw_configuration_exception(event_spec);
  }
}

std::tuple<art::RunNumber_t, art::SubRunNumber_t, art::EventNumber_t>
art::detail::event_start_for_sampled_input(std::string const& event_spec)
{
  std::smatch parts;
  auto const success = std::regex_match(event_spec, parts, re_event_id);
  if (!success) {
    throw_configuration_exception(event_spec);
  }

  assert(parts.size() == 4ull);
  // A successful match will populate 'parts' with 4 elements.  For example:
  //
  //   +-------------+---------+
  //   | User spec.  | '1:0:3' |
  //   +-------------+---------+
  //   | parts[0] == | '1:0:3' |
  //   | parts[1] == |     '1' |
  //   | parts[2] == |     '0' |
  //   | parts[3] == |     '3' |
  //   +-------------+---------+

  auto const run = convert_or_throw<Level::Event>(parts[1], event_spec);
  auto const subrun = convert_or_throw<Level::Event>(parts[2], event_spec);
  auto const event = convert_or_throw<Level::Event>(parts[3], event_spec);
  return std::make_tuple(run, subrun, event);
}
