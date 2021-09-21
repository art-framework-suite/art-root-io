set(CCV_DEFAULT_RECURSIVE FALSE)

cet_build_plugin(BadAssnsProducer art::Producer ALLOW_UNDERSCORES NO_INSTALL
  LIBRARIES PRIVATE messagefacility::MF_MessageLogger)

art_dictionary(DICTIONARY_LIBRARIES art_test::TestObjects NO_INSTALL)

cet_test(BadAssns_t HANDBUILT
  TEST_EXEC art
  TEST_ARGS --rethrow-all -c BadAssns_t.fcl -o out.root -n 4
  DATAFILES fcl/BadAssns_t.fcl
  TEST_PROPERTIES
  PASS_REGULAR_EXPRESSION "No dictionary found for the following classes:\n.*art::Wrapper<art::Assns<arttest::DummyProduct,arttest::DummyA,void> >\n.*Most likely they were never"
)
