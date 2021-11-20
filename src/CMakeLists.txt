SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
add_subdirectory(z80)
add_subdirectory(util)
add_executable(sms main.cpp util/bitfield.h
        mem/rom.cpp mem/rom.h
        mem/bus.cpp mem/bus.h
        mem/bios.cpp mem/bios.h
        mem/mem.cpp mem/mem.h
        vdp/vdp.cpp vdp/vdp.h
        vdp/vdp_register.cpp
        vdp/vdp_register.h
        vdp/sdl_render.cpp vdp/sdl_render.h


        ../test/superzazu_z80/z80.c
        ../test/superzazu_z80/z80.h
        )

target_link_libraries(sms SDL2)
target_link_libraries(sms z80 util)
