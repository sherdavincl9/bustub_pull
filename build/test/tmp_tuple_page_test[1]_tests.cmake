add_test( TmpTuplePageTest.BasicTest /home/chen/bustub/build/test/tmp_tuple_page_test [==[--gtest_filter=TmpTuplePageTest.DISABLED_BasicTest]==] --gtest_also_run_disabled_tests [==[--gtest_color=auto]==] [==[--gtest_output=xml:/home/chen/bustub/build/test/tmp_tuple_page_test.xml]==] [==[--gtest_catch_exceptions=0]==])
set_tests_properties( TmpTuplePageTest.BasicTest PROPERTIES DISABLED TRUE)
set_tests_properties( TmpTuplePageTest.BasicTest PROPERTIES WORKING_DIRECTORY /home/chen/bustub/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==] TIMEOUT 120)
set( tmp_tuple_page_test_TESTS TmpTuplePageTest.BasicTest)
