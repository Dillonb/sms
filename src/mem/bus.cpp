#include <util/log.h>
#include "bus.h"
#include "bios.h"
#include "mem.h"

namespace Bus {
    u8 read_byte(u16 address) {
        switch (address) {
            case 0x0000 ... 0xBFFF:
                return Bios::data[address];
            case 0xC000 ... 0xDFFF:
            case 0xE000 ... 0xFFFF:
                return Mem::ram[address & 0x1FFF];
            default:
                logfatal("Should never get here.");
        }

    }

    void write_byte(u16 address, u8 value) {
        switch (address) {
            case 0x0000 ... 0xBFFF:
                logfatal("Cart mapper");
                break;
            case 0xC000 ... 0xDFFF:
                Mem::ram[address - 0xC000] = value;
                break;
            case 0xE000 ... 0xFFFF:
                logfatal("Mirror of system RAM");
                break;
            default:
                logfatal("Should never get here.");
        }
    }

    void port_out(u8 port, u8 value) {
        switch (port) {
            case 0x40 ... 0x7F:
                logwarn("PSG port written, ignoring!");
                break;
            case 0x80 ... 0xBF:
                logwarn("VDP register written, ignoring!");
                break;
            default:
                logfatal("Unsupported port: 0x%02X", port);
        }
    }

    u8 port_in(u8 port) {
        switch (port) {
            case 0x40 ... 0x7F:
                logfatal("Read from either VCounter or HCounter!");
            default:
                logfatal("Unsupported port: 0x%02X", port);
        }
    }
}