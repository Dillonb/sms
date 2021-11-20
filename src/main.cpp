#include <vdp/sdl_render.h>
#include "mem/rom.h"
#include "mem/bus.h"
#include "mem/bios.h"
#include "z80/z80.h"
#include "util/log.h"
#include "vdp/vdp.h"
#include "../test/superzazu_z80/z80.h"


static z80 z;

u8 sz_read_byte(void*, u16 address) {
    return Bus::read_byte(address);
}

void sz_write_byte(void*, u16 address, u8 value) {
    Bus::write_byte(address, value);
}

u8 sz_port_in(z80*, u8 port) {
    return Bus::port_in(port);
}

void sz_port_out(z80*, u8 port, u8 value) {
    Bus::port_out(port, value);
}

void run_sz() {
    z80_init(&z);
    z.write_byte = sz_write_byte;
    z.read_byte = sz_read_byte;
    z.port_in = sz_port_in;
    z.port_out = sz_port_out;
    Vdp::reset();
    if (Bios::try_load()) {
        logalways("Found a bios!");
    } else {
        logalways("No bios found.");
    }

    Vdp::render_init();

    while (1) {
        if (Vdp::interrupt_pending()) {
            z.int_pending = true;
        }
        z80_step(&z);
        Vdp::step(z.cyc);
        z.cyc = 0;
    }
}

void run_mine() {
    Z80::reset();
    Vdp::reset();
    Z80::set_bus_handlers(Bus::read_byte, Bus::write_byte);
    Z80::set_port_handlers(Bus::port_in, Bus::port_out);
    if (Bios::try_load()) {
        logalways("Found a bios!");
    } else {
        logalways("No bios found.");
    }
    Z80::set_pc(0);

    Vdp::render_init();

    while (1) {
        if (Vdp::interrupt_pending()) {
            Z80::raise_interrupt();
        }
        int cycles = Z80::step();
        Vdp::step(cycles);
    }
}

int main(int argc, char** argv) {
    Rom::load(argv[1]);

    run_mine();

    return 0;
}
