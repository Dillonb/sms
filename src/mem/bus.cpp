#include <util/log.h>
#include <vdp/vdp.h>
#include "bus.h"
#include "bios.h"
#include "mem.h"
#include "rom.h"

namespace Bus {

    bool enable_joysticks = true;
    bool enable_bios = true;
    bool enable_ram = true;
    bool enable_card_rom = false;
    bool enable_cart_rom = false;
    bool enable_ext_port = false;

    void update_memory_enables(u8 value) {
        enable_joysticks = (value >> 2) & 1;
        enable_bios = (value >> 3) & 1;
        enable_ram = (value >> 4) & 1;
        enable_card_rom = (value >> 5) & 1;
        enable_cart_rom = (value >> 6) & 1;
        enable_ext_port = (value >> 7) & 1;

        //logalways("joysticks %d bios %d ram %d card_rom %d cart_rom %d ext_port %d", enable_joysticks, enable_bios, enable_ram, enable_card_rom, enable_cart_rom, enable_ext_port);
    }

    u8 read_byte(u16 address) {
        switch (address) {
            case 0x0000 ... 0xBFFF: {
                // TODO optimize
                u8 value = 0xFF;
                if (enable_bios) {
                    value &= Bios::data[address & 0x1FFF];
                }
                if (enable_cart_rom) {
                    value &= Rom::read(address);
                }
                return value;
            }
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
                // Ignore these writes for now, I guess
                break;
            case 0xC000 ... 0xDFFF:
                Mem::ram[address & 0x1FFF] = value;
                break;
            case 0xE000 ... 0xFFFF:
                if (address >= 0xFFFC) {
                    Rom::mapper_ctrl_write(address, value);
                }
                // Values matching the above if statement are also written to RAM.
                Mem::ram[address & 0x1FFF] = value;
                break;
            default:
                logfatal("Should never get here.");
        }
    }

    void port_out(u8 port, u8 value) {
        switch (port) {
            case 0x40 ... 0x7F: // PSG ports, ignored for now
                break;
            case 0xBE:
                Vdp::write_data(value);
                break;
            case 0xBF:
                Vdp::write_control(value);
                break;
            case 0x3E:
                update_memory_enables(value);
                break;
            default:
                logfatal("Unsupported port: 0x%02X = %02X", port, value);
        }
    }

    u8 port_in(u8 port) {
        switch (port) {
            case 0x40 ... 0x7F:
                if (port & 1) {
                    logfatal("HCounter read (oh no)");
                } else {
                    return Vdp::vcounter;
                    logfatal("VCounter read");
                }
                logfatal("Read from either VCounter or HCounter!");
            case 0x80 ... 0xBF:
                if (port & 1) { // Odd port - VDP status
                    return Vdp::get_status();
                } else { // Even port - VDP data port
                    return Vdp::read_buffer;
                    logfatal("Unsupported port: 0x%02X (VDP data port)", port);
                }
            case 0xDC: // controller data A
                return 0xFF;
            case 0xDD: // controller data B / misc
                return 0xFF;
            default:
                logfatal("Unsupported port: 0x%02X", port);
        }
    }
}