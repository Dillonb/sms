add_executable(cpm_test cpm_test.cpp)
target_link_libraries(cpm_test z80 util)

foreach (test zexall zexdoc prelim)
    configure_file(data/${test}.com ${test}.com COPYONLY)
    add_test(NAME cpm_${test} COMMAND cpm_test ${test}.com)
    message("Test: ${test}")
endforeach(test)
