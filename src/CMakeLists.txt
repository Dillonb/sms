add_subdirectory(z80)
add_subdirectory(util)
add_executable(sms main.cpp util/bitfield.h)
target_link_libraries(sms z80 util)
