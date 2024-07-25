add_test( ClockReplacerTest.SampleTest /home/chen/bustub/build/test/clock_replacer_test [==[--gtest_filter=ClockReplacerTest.DISABLED_SampleTest]==] --gtest_also_run_disabled_tests [==[--gtest_color=auto]==] [==[--gtest_output=xml:/home/chen/bustub/build/test/clock_replacer_test.xml]==] [==[--gtest_catch_exceptions=0]==])
set_tests_properties( ClockReplacerTest.SampleTest PROPERTIES DISABLED TRUE)
set_tests_properties( ClockReplacerTest.SampleTest PROPERTIES WORKING_DIRECTORY /home/chen/bustub/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==] TIMEOUT 120)
set( clock_replacer_test_TESTS ClockReplacerTest.SampleTest)
