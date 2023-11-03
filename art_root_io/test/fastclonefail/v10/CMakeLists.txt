art_dictionary(NO_INSTALL)

basic_plugin(ClonedProdProducer "module" ALLOW_UNDERSCORES NO_INSTALL
  LIBRARIES PRIVATE art::Framework_Core
 )

set_target_properties(
  art_root_io_test_fastclonefail_v10_dict
  art_root_io_test_fastclonefail_v10_ClonedProdProducer_module
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR})

# Write data to an output file using a private dictionary from this
# directory. The intention is that the data members of the data
# product class will be changed in such a way that the later test that
# reads this file will need to use schema evolution which will cause
# fast cloning to fail.
#
# NB: Split level must be high enough for ROOT's I/O system to detect
#     a change in the type of the persisted product.
cet_test(test_fastclonefail_w.sh PREBUILT
  TEST_ARGS $<TARGET_FILE:art> test_fastclone_fail_v10.fcl
  DATAFILES test_fastclone_fail_v10.fcl)
