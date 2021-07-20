#ifndef SMS_VDP_H
#define SMS_VDP_H

#include <util/types.h>

namespace Vdp {
    extern u8 vram[0x4000];
    extern u8 cram[32];
    extern u8 screen[256][256];

    extern int vcounter;
    extern u8 read_buffer;

    constexpr int SMS_SCREEN_X = 256;
    constexpr int SMS_SCREEN_Y = 256;

    void reset();
    void write_control(u8 value);
    void write_data(u8 value);
    void step(unsigned int cycles);
    bool interrupt_pending();
    u8 get_status();
}

#endif //SMS_VDP_H
