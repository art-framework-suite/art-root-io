art_dictionary(NO_INSTALL)

basic_plugin(ClonedProdAnalyzer "module" ALLOW_UNDERSCORES NO_INSTALL
  LIBRARIES PRIVATE art::Framework_Core)

set_target_properties(
  art_root_io_test_fastclonefail_v11_dict
  art_root_io_test_fastclonefail_v11_ClonedProdAnalyzer_module
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Read data product in a way that requires schema evolution, which
# requires fast-cloning to fail (see comment in
# v10/test_fastclonefail_w.sh).
cet_test(test_fastclonefail_r.sh PREBUILT
  TEST_ARGS $<TARGET_FILE:art>
            test_fastclone_fail_v11.fcl
            ../../v10/test_fastclonefail_w.sh.d/out.root
  TEST_PROPERTIES DEPENDS test_fastclonefail_w.sh
  PASS_REGULAR_EXPRESSION "Unable to fast clone tree"
  DATAFILES test_fastclone_fail_v11.fcl
  REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/../v10/test_fastclonefail_w.sh.d/out.root)
