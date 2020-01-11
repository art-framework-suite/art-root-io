#include "art_root_io/setup.h"

#include "TError.h"

#include <catch2/catch.hpp>
#include <string>

#define exception_is_required "An exception must be thrown"
#define job_should_continue "The job should continue without incident"

SCENARIO("Some ROOT messages are fatal errors")
{
  art::root::setup();
  WHEN("A tree is being filled but the branch address is not set")
  {
    THEN(exception_is_required)
    {
      REQUIRE_THROWS(Error(
        "Fill", "attempt to fill branch %s while addresss is not set", "foo"));
    }
  }
  WHEN("Two branches of the same tree have different numbers of entries")
  {
    THEN(exception_is_required)
    {
      REQUIRE_THROWS(Warning(
        "SetEntries", "Tree branches have different numbers of entries"));
    }
  }
  WHEN("XRootD is attempting to retry file opens")
  {
    AND_WHEN("TNetXNGFile::Open results in a fatal error")
    {
      THEN(exception_is_required)
      {
        REQUIRE_THROWS(Error("TNetXNGFile::Open", "[FATAL]"));
      }
    }
  }
}

SCENARIO("Some ROOT messages are informational")
{
  art::root::setup();
  WHEN("The input file contains obsolete art objects that are not read")
  {
    THEN(job_should_continue)
    {
      for (char const* name :
           {"art::Transient<art::ProductRegistry::Transients>",
            "art::DoNotRecordParents"}) {
        REQUIRE_NOTHROW(Warning(
          "TClass::TClass", "no dictionary for class %s is available", name));
      }
    }
  }
  WHEN("Some dictionary errors are emitted")
  {
    AND_WHEN("The message has the word 'dictionary' in it")
    {
      THEN(job_should_continue)
      {
        REQUIRE_NOTHROW(Error("TBranchElement::Bronch",
                              "Cannot find dictionary for class foo"));
      }
    }
    AND_WHEN("The class is already in the TClassTable")
    {
      THEN(job_should_continue)
      {
        REQUIRE_NOTHROW(Warning(
          "TClassTable::Add", "class %s already in TClassTable", "foo"));
      }
    }
  }
  WHEN("ROOT has declared a fatal error, but it is due to a pending signal")
  {
    THEN("No exception should be thrown.")
    {
      REQUIRE_NOTHROW(Break("TUnixSystem::DispatchSignals", "%s", "SIGUSR2"));
    }
  }
  WHEN("The DISPLAY environment variable is not set")
  {
    THEN(job_should_continue)
    {
      REQUIRE_NOTHROW(Warning("TUnixSystem::SetDisplay", "DISPLAY not set"));
    }
  }
  WHEN("XRootD is attempting to retry file opens and authentications")
  {
    AND_WHEN("The TUnixSystem::GetHostByName function is called")
    {
      THEN(job_should_continue)
      {
        REQUIRE_NOTHROW(Error("TUnixSystem::GetHostByName",
                              "getaddrinfo failed for '%s': %s",
                              "http://fndca1.fnal.gov/",
                              "Temporary failure in name resolution"));
      }
    }
    AND_WHEN("TNetXNGFile::Open results in a non-fatal error")
    {
      THEN(job_should_continue)
      {
        REQUIRE_NOTHROW(Error("TNetXNGFile::Open", "[INFO]"));
      }
    }
  }
  WHEN("Any errors in a 'Fit' location are emitted")
  {
    THEN(job_should_continue)
    {
      REQUIRE_NOTHROW(Error("TF1::SetFitResult", "Invalid Fit result passed"));
    }
  }
  WHEN("A Cholesky decomposition error is emitted")
  {
    THEN(job_should_continue)
    {
      REQUIRE_NOTHROW(Error("TDecompChol::Solve", "matrix is singular"));
    }
  }
  WHEN("Any THistPainter::PaintInit errors are emitted")
  {
    THEN(job_should_continue)
    {
      REQUIRE_NOTHROW(
        Error("THistPainter::PaintInit", "cannot set x axis to log scale"));
    }
  }
  WHEN("Any TGClient::GetFontByName errors are emitted")
  {
    THEN(job_should_continue)
    {
      REQUIRE_NOTHROW(
        Warning("TGClient::GetFontByName", "couldn't retrieve font"));
    }
  }
}

#undef job_should_continue
#undef exception_is_required
