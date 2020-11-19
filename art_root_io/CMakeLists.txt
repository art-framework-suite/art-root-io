include(ArtDictionary)

art_dictionary(DICTIONARY_LIBRARIES PRIVATE art::art_Framework_IO_ProductMix)

add_subdirectory(RootDB)

art_make_library(
  LIBRARY_NAME
    art_root_io_detail
  SOURCE
    detail/DummyProductCache.cc
    detail/RangeSetInfo.cc
    detail/RootErrorClassifier.cc
    detail/dropBranch.cc
    detail/getEntry.cc
    detail/getObjectRequireDict.cc
    detail/rangeSetFromFileIndex.cc
    detail/resolveRangeSet.cc
    detail/rootFileSizeTools.cc
    detail/rootOutputConfigurationTools.cc
  LIBRARIES PRIVATE
    art::art_Framework_Core
    art_root_io::art_root_io_RootDB
    canvas_root_io::canvas_root_io
    messagefacility::MF_MessageLogger
    ROOT::Core
    ROOT::RIO
    ROOT::Tree
  )

art_make_library(
  LIBRARY_NAME
    art_root_io
  SOURCE
    checkDictionaries.cc
    DropMetaData.cc
    DuplicateChecker.cc
    FastCloningInfoProvider.cc
    GetFileFormatEra.cc
    GetFileFormatVersion.cc
    RootBranchInfo.cc
    RootBranchInfoList.cc
    RootDelayedReader.cc
    RootInputFile.cc
    RootInputFileSequence.cc
    RootIOPolicy.cc
    RootOutputFile.cc
    RootOutputTree.cc
    setup.cc
  LIBRARIES
    art::art_Framework_Core
    art::art_Framework_Principal
    art::art_Framework_IO
    art::art_Framework_IO_Catalog
    art::art_Framework_IO_detail
    art::art_Framework_Services_Registry
    art::art_Framework_Services_System_FileCatalogMetadata_service
    art_root_io::art_root_io_detail
    canvas_root_io::canvas_root_io
    ROOT::Tree
  PRIVATE
    art_root_io::art_root_io_RootDB
    messagefacility::MF_MessageLogger
    Boost::date_time
    ROOT::Hist
    ROOT::RIO
  )

art_make_library(
  LIBRARY_NAME
    art_root_io_tfile_support
  SOURCE
    TFileDirectory.cc
    detail/RootDirectorySentry.cc
  LIBRARIES
    art_root_io::art_root_io
    ROOT::Hist
    ROOT::Core
  )

simple_plugin(TFileService "service"
  ALLOW_UNDERSCORES
  LIBRARIES
    art_root_io::art_root_io_tfile_support
  PRIVATE
    art::art_Framework_IO
    art::art_Framework_IO_detail
    art::art_Framework_Principal
    ROOT::RIO
    ROOT::Thread
    ROOT::Tree
  )

simple_plugin(RootInput "source"
  ALLOW_UNDERSCORES
  LIBRARIES INTERFACE
    messagefacility::MF_MessageLogger
  PRIVATE
    art::art_Framework_IO_Catalog
    art_root_io::art_root_io
    messagefacility::MF_MessageLogger
  )

simple_plugin(RootOutput "module"
  ALLOW_UNDERSCORES
  LIBRARIES PRIVATE
    art::art_Framework_Core
    art_root_io::art_root_io
    messagefacility::MF_MessageLogger
  )

simple_plugin(SamplingInput "source"
  LIBRARIES PRIVATE
    messagefacility::MF_MessageLogger
    art_root_io::art_root_io
    art_root_io::art_root_io_RootDB
    ROOT::RIO
    ROOT::Tree
  SOURCE
    SamplingInput_source.cc
    detail/DataSetBroker.cc
    detail/DataSetSampler.cc
    detail/SamplingDelayedReader.cc
    detail/SamplingInputFile.cc
    detail/event_start.cc
  ALLOW_UNDERSCORES)


cet_make_exec(NAME product_sizes_dumper
  SOURCE product_sizes_dumper.cc RootSizeOnDisk.cc
  LIBRARIES PRIVATE
  art_root_io::art_root_io
  Boost::program_options
  ROOT::RIO)

cet_make_exec(NAME config_dumper
  LIBRARIES PRIVATE
  art::art_Utilities
  art_root_io::art_root_io
  art_root_io::art_root_io_RootDB
  Boost::program_options
  ROOT::Tree
  ROOT::RIO)

cet_make_exec(NAME sam_metadata_dumper
  LIBRARIES PRIVATE
  art::art_Utilities
  art_root_io::art_root_io
  art_root_io::art_root_io_RootDB
  Boost::program_options
  ROOT::RIO)

cet_make_exec(NAME count_events
  LIBRARIES PRIVATE
  art::art_Utilities
  art_root_io::art_root_io
  Boost::program_options
  ROOT::Tree
  ROOT::RIO)

cet_make_exec(NAME file_info_dumper
  SOURCE file_info_dumper.cc detail/InfoDumperInputFile.cc
  LIBRARIES PRIVATE
  art_root_io::art_root_io
  art_root_io::art_root_io_RootDB
  Boost::program_options)

include(CetMakeCompletions)
cet_make_completions(product_sizes_dumper)
cet_make_completions(config_dumper)
cet_make_completions(sam_metadata_dumper)
cet_make_completions(count_events)
cet_make_completions(file_info_dumper)

install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)

add_subdirectory(test)
