#include "rom.h"

#include <util/load_bin.h>
#include <util/log.h>

using std::filesystem::exists;

namespace Rom {
    Rom rom;

    unsigned int bank_offsets[3];

    void load(const char* path) {
        if (exists(path)) {
            rom.data = load_bin<u8>(path);
        } else {
            logfatal("%s not found!", path);
        }
    }

    u8 read(u16 address) {
        u16 in_slot_offset = address & 0x3FFF;
        switch (address) {
            case 0x0000 ... 0x3FFF: // Slot 0
                return rom.data[bank_offsets[0] + in_slot_offset];
            case 0x4000 ... 0x7FFF: // Slot 1
                return rom.data[bank_offsets[1] + in_slot_offset];
            case 0x8000 ... 0xBFFF: // Slot 2
                return rom.data[bank_offsets[2] + in_slot_offset];
        }
        return rom.data[address];
    }

    bool rom_write = true;
    bool ram_0 = false; // c000 - ffff
    bool ram_1 = false; // 8000 - bfff
    bool ram_bank_select = false;
    u8 bank_shift = 0;

    void mapper_ctrl_write(u16 address, u8 value) {
        unsigned int offset = value * 0x4000;
        switch (address) {
            case 0xFFFC: // Mapper control register
                rom_write = (value >> 7) & 1;
                ram_0 = (value >> 4) & 1;
                ram_1 = (value >> 3) & 1;
                ram_bank_select = (value >> 2) & 1;
                bank_shift = value & 3;
                unimplemented(!rom_write, "rom_write disabled");
                unimplemented(ram_0, "ram_0 enabled");
                unimplemented(ram_1, "ram_1 enabled");
                unimplemented(ram_bank_select, "ram_bank_select enabled");
                unimplemented(bank_shift != 0, "bank_shift != 0");
                break;
            case 0xFFFD: // Bank 0 offset
                bank_offsets[0] = offset;
                break;
            case 0xFFFE: // Bank 1 offset
                bank_offsets[1] = offset;
                break;
            case 0xFFFF: // Bank 2 offset
                bank_offsets[2] = offset;
                break;
            default:
                logfatal("Unknown sega mapper write %04X = %02X", address, value);
        }
    }
}