#ifndef SMS_VDP_H
#define SMS_VDP_H

#include <util/types.h>

namespace Vdp {
    extern u8 vram[0x4000];
    void reset();
    void write_control(u8 value);
    void write_data(u8 value);
    void step(int cycles);
    bool interrupt_pending();
    u8 get_status();
}

#endif //SMS_VDP_H
