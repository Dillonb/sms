#include "z80.h"

#include <cstdio>
#include <cstring>

#include "util/log.h"

#include "instructions.h"
#include "util.h"

namespace Z80 {
    z80_t z80;

    void reset() {
        memset(&z80, 0, sizeof(z80_t));
        z80.a = 0xFF;
        z80.f.set(0xFF);
        z80.sp = 0xFFFF;
    }

    void set_bus_handlers(read_byte_handler read_handler, write_byte_handler write_handler) {
        z80.read_byte = read_handler;
        z80.write_byte = write_handler;
    }

    void set_port_handlers(port_in_handler in_handler, port_out_handler out_handler) {
        z80.port_in = in_handler;
        z80.port_out = out_handler;
    }

    void set_pc(u16 address) {
        z80.pc = address;
    }

    void service_interrupt() {
        z80.interrupts_enabled = false;
        z80.next_interrupts_enabled = false;
        z80.interrupt_pending = false;
        switch (z80.interrupt_mode) {
            case 1:
                stack_push<u16>(z80.pc);
                z80.pc = 0x0038;
                break;
            default:
                logfatal("Interrupt raised. Mode: %d", z80.interrupt_mode);
        }
    }

    int step() {
        z80.interrupts_enabled = z80.next_interrupts_enabled;

        u16 address = z80.pc;
        u8 opcode = z80.read_byte(z80.pc++);

        logdebug("[%04X] %02X %02X %02X %02X", address, opcode, z80.read_byte(z80.pc), z80.read_byte(z80.pc + 1), z80.read_byte(z80.pc + 2));
        logtrace("AF: %02X%02X BC: %04X DE: %04X HL: %04X", z80.a, z80.f.assemble(), z80.bc.raw, z80.de.raw, z80.hl.raw);
        logtrace("SZ5H3PVNC");
        logtrace("%d%d%d%d%d %d%d%d", z80.f.s, z80.f.z, z80.f.b5, z80.f.h, z80.f.b3, z80.f.p_v, z80.f.n, z80.f.c);

        z80.instructions++;

        u8 r_hi = z80.r & 0x80;
        z80.r = r_hi | ((z80.r + 1) & 0x7F);

        int cycles = instructions[opcode]();

        if (z80.interrupts_enabled && z80.interrupt_pending) {
            service_interrupt();
        }

        return cycles;
    }

    void raise_interrupt() {
        z80.interrupt_pending = true;
    }
}