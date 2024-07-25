add_test( LRUKReplacerTest.SampleTest /home/chen/bustub/build/test/lru_k_replacer_test [==[--gtest_filter=LRUKReplacerTest.SampleTest]==] --gtest_also_run_disabled_tests [==[--gtest_color=auto]==] [==[--gtest_output=xml:/home/chen/bustub/build/test/lru_k_replacer_test.xml]==] [==[--gtest_catch_exceptions=0]==])
set_tests_properties( LRUKReplacerTest.SampleTest PROPERTIES WORKING_DIRECTORY /home/chen/bustub/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==] TIMEOUT 120)
set( lru_k_replacer_test_TESTS LRUKReplacerTest.SampleTest)
