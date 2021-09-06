#ifndef SMS_Z80_H
#define SMS_Z80_H

#include <cstdint>

#include "util/bitfield.h"
#include "util/types.h"

#include "registers.h"

namespace Z80 {
    typedef u8 (*read_byte_handler)(u16 address);
    typedef void (*write_byte_handler)(u16 address, u8 value);
    typedef u8 (*port_in_handler)(u8 port);
    typedef void (*port_out_handler)(u8 port, u8 value);

    typedef struct z80 {
        read_byte_handler read_byte;
        write_byte_handler write_byte;
        port_in_handler port_in;
        port_out_handler port_out;

        int interrupt_mode;

        bool interrupts_enabled;
        bool next_interrupts_enabled;

        u8 a;
        FlagRegister f;

        Util::Bitfield<WideRegister> bc;
        Util::Bitfield<WideRegister> de;
        Util::Bitfield<WideRegister> hl;
        u16 pc;
        u16 sp;
        u8 i;
        Util::Bitfield<WideRegister> ix;
        Util::Bitfield<WideRegister> iy;

        u8 r;

        // Shadow registers
        u16 af_, bc_, de_, hl_;

        // for DDCB and FDCB
        s8 prev_immediate;

        long instructions;
    } z80_t;

    extern z80_t z80;

    void reset();
    void set_bus_handlers(read_byte_handler read_handler, write_byte_handler write_handler);
    void set_port_handlers(port_in_handler in_handler, port_out_handler out_handler);

    void set_pc(u16 address);

    void raise_interrupt();

    int step();
}

#endif //SMS_Z80_H
