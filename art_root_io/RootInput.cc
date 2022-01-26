#include "art_root_io/RootInput.h"

#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/InputSourceDescription.h"
#include "art/Framework/Core/ProcessingLimits.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/RangeSetHandler.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art_root_io/FastCloningInfoProvider.h"
#include "art_root_io/RootInputFileSequence.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Utilities/Exception.h"

#include <memory>

using namespace art;
using namespace std;

RootInput::AccessState::~AccessState() = default;
RootInput::AccessState::AccessState() = default;

RootInput::AccessState::State
RootInput::AccessState::state() const
{
  return state_;
}

void
RootInput::AccessState::resetState()
{
  state_ = SEQUENTIAL;
}

EventID const&
RootInput::AccessState::lastReadEventID() const
{
  return lastReadEventID_;
}

EventID const&
RootInput::AccessState::wantedEventID() const
{
  return wantedEventID_;
}

shared_ptr<RootInputFile>
RootInput::AccessState::rootFileForLastReadEvent() const
{
  return rootFileForLastReadEvent_;
}

void
RootInput::AccessState::setState(State state)
{
  state_ = state;
}

void
RootInput::AccessState::setLastReadEventID(EventID const& eid)
{
  lastReadEventID_ = eid;
}

void
RootInput::AccessState::setWantedEventID(EventID const& eid)
{
  wantedEventID_ = eid;
}

void
RootInput::AccessState::setRootFileForLastReadEvent(
  shared_ptr<RootInputFile> const& ptr)
{
  rootFileForLastReadEvent_ = ptr;
}

RootInput::~RootInput() = default;

RootInput::RootInput(Parameters const& config, InputSourceDescription& desc)
  : InputSource{desc.moduleDescription}
  , limits_{config().limits_config()}
  , catalog_{config().ifc_config}
  , primaryFileSequence_{
      config().rifs_config,
      catalog_,
      FastCloningInfoProvider{cet::make_exempt_ptr(&limits_)},
      limits_.processingMode(),
      desc.productRegistry,
      processConfiguration()}
{}

void
RootInput::doEndJob()
{
  primaryFileSequence_.endJob();
}

void
RootInput::closeFile()
{
  primaryFileSequence_.closeFile_();
}

input::ItemType
RootInput::getNextItemType()
{
  if (limits_.atLimit()) {
    return input::IsStop;
  }

  auto next_item_type = primaryFileSequence_.getNextItemType();
  while (not limits_.itemTypeAllowed(next_item_type)) {
    next_item_type = primaryFileSequence_.getNextItemType();
  }

  return next_item_type;
}

input::ItemType
RootInput::nextItemType()
{
  switch (accessState_.state()) {
  case AccessState::SEQUENTIAL:
    return getNextItemType();
  case AccessState::SEEKING_FILE:
    return input::IsFile;
  case AccessState::SEEKING_RUN:
    primaryFileSequence_.readIt(accessState_.wantedEventID().runID());
    return input::IsRun;
  case AccessState::SEEKING_SUBRUN:
    primaryFileSequence_.readIt(accessState_.wantedEventID().subRunID());
    return input::IsSubRun;
  case AccessState::SEEKING_EVENT: {
    primaryFileSequence_.readIt(accessState_.wantedEventID(), true);
    accessState_.setLastReadEventID(accessState_.wantedEventID());
    accessState_.setRootFileForLastReadEvent(
      primaryFileSequence_.rootFileForLastReadEvent());
    return input::IsEvent;
  }
  }
  throw Exception{errors::LogicError} << "Unreachable.\n";
}

unique_ptr<FileBlock>
RootInput::readFile()
{
  switch (accessState_.state()) {
  case AccessState::SEQUENTIAL:
    return primaryFileSequence_.readFile_();
  case AccessState::SEEKING_FILE:
    accessState_.setState(AccessState::SEEKING_RUN);
    return primaryFileSequence_.readFile_();
  default:
    throw Exception(errors::LogicError) << "RootInputSource::readFile "
                                           "encountered an unknown or "
                                           "inappropriate AccessState.\n";
  }
}

unique_ptr<RunPrincipal>
RootInput::readRun()
{
  switch (accessState_.state()) {
  case AccessState::SEQUENTIAL:
    return primaryFileSequence_.readRun_();
  case AccessState::SEEKING_RUN:
    accessState_.setState(AccessState::SEEKING_SUBRUN);
    return primaryFileSequence_.readRun_();
  default:
    throw Exception(errors::LogicError) << "RootInputSource::readRun "
                                           "encountered an unknown or "
                                           "inappropriate AccessState.\n";
  }
}

unique_ptr<RangeSetHandler>
RootInput::runRangeSetHandler()
{
  return primaryFileSequence_.runRangeSetHandler();
}

unique_ptr<SubRunPrincipal>
RootInput::readSubRun(cet::exempt_ptr<RunPrincipal const> rp)
{
  unique_ptr<SubRunPrincipal> result;
  switch (accessState_.state()) {
  case AccessState::SEQUENTIAL:
    result = readSubRun_(rp);
    break;
  case AccessState::SEEKING_SUBRUN:
    accessState_.setState(AccessState::SEEKING_EVENT);
    result = readSubRun_(rp);
    break;
  default:
    throw Exception(errors::LogicError) << "RootInputSource::readSubRun "
                                           "encountered an unknown or "
                                           "inappropriate AccessState.\n";
  }
  limits_.update(result->subRunID());
  return result;
}

unique_ptr<SubRunPrincipal>
RootInput::readSubRun_(cet::exempt_ptr<RunPrincipal const> rp)
{
  auto result = primaryFileSequence_.readSubRun_();
  result->setRunPrincipal(rp);
  return result;
}

unique_ptr<RangeSetHandler>
RootInput::subRunRangeSetHandler()
{
  return primaryFileSequence_.subRunRangeSetHandler();
}

unique_ptr<EventPrincipal>
RootInput::readEvent(cet::exempt_ptr<SubRunPrincipal const> srp)
{
  unique_ptr<EventPrincipal> result;
  switch (accessState_.state()) {
  case AccessState::SEQUENTIAL:
    result = readEvent_(srp);
    break;
  case AccessState::SEEKING_EVENT:
    accessState_.resetState();
    result = readEvent_(srp);
    break;
  default:
    throw Exception(errors::LogicError) << "RootInputSource::readEvent "
                                           "encountered an unknown or "
                                           "inappropriate AccessState.\n";
  }
  limits_.update(result->eventID());
  return result;
}

unique_ptr<EventPrincipal>
RootInput::readEvent_(cet::exempt_ptr<SubRunPrincipal const> srp)
{
  auto result = primaryFileSequence_.readEvent_();
  if (result) {
    accessState_.setLastReadEventID(result->eventID());
    accessState_.setRootFileForLastReadEvent(
      primaryFileSequence_.rootFileForLastReadEvent());
  }
  result->setSubRunPrincipal(srp);
  return result;
}
