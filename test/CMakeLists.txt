add_executable(cpm_test cpm_test.cpp)
target_link_libraries(cpm_test z80 util)

configure_file(data/zexall.com zexall.com COPYONLY)
configure_file(data/zexall.log zexall.log COPYONLY)
configure_file(data/zexdoc.com zexdoc.com COPYONLY)
configure_file(data/prelim.com prelim.com COPYONLY)

#add_test(NAME cpm_prelim COMMAND cpm_test prelim.com)
add_test(NAME cpm_zexall COMMAND cpm_test zexall.com zexall.log)
#add_test(NAME cpm_zexdoc COMMAND cpm_test zexdoc.com)
