#ifndef SMS_Z80_H
#define SMS_Z80_H

#include <cstdint>

#include "util/bitfield.h"
#include "util/types.h"

#include "registers.h"

namespace Z80 {
    typedef u8 (*read_byte_handler)(u16 address);
    typedef void (*write_byte_handler)(u16 address, u8 value);

    typedef struct z80 {
        read_byte_handler read_byte;
        write_byte_handler write_byte;

        u8 a;
        FlagRegister f;

        Util::Bitfield<WideRegister> bc;
        Util::Bitfield<WideRegister> de;
        Util::Bitfield<WideRegister> hl;
        u16 pc;
        u16 sp;
        u8 i;
        u16 ix;
        u16 iy;

        // Shadow registers
        u16 af_, bc_, de_, hl_;
    } z80_t;

    extern z80_t z80;

    void reset();
    void set_bus_handlers(read_byte_handler read_handler, write_byte_handler write_handler);

    void set_pc(u16 address);

    int step();
}

#endif //SMS_Z80_H
