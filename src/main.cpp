#include "mem/rom.h"
#include "mem/bus.h"
#include "mem/bios.h"
#include "z80/z80.h"
#include "util/log.h"
#include "vdp/vdp.h"

int main(int argc, char** argv) {
    Rom::load(argv[1]);

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

    while (1) {
        if (Vdp::interrupt_pending()) {
            Z80::raise_interrupt();
        }
        int cycles = Z80::step();
        Vdp::step(cycles);
    }

    return 0;
}
