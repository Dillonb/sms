#include <iostream>
#include <cstring>

#include "z80/z80.h"
#include "util/load_bin.h"
#include "util/types.h"
#include "util/log.h"

using std::cout;
using std::endl;

u8 memory[0x10000];

u8 read_byte(u16 address) {
    return memory[address];
}

void write_byte(u16 address, u8 value) {
    memory[address] = value;
}

void load_rom(const char* path) {
    auto data = load_bin<u8>(path);
    for (unsigned int i = 0; i < data.size(); i++) {
        memory[0x100 + i] = data[i];
    }
}

bool check(const char* field, const std::string& expected, const u16 actual) {
    u16 expected_int = std::stoi(expected, nullptr, 16);
    if (actual != expected_int) {
        printf("%s Expected: %s Actual: %04X\n", field, expected.c_str(), actual);
        if ("AF" == std::string(field)) {
            printf("       SZ5H3P/VNC\n");
            Z80::FlagRegister expected_f;
            expected_f.set(expected_int & 0xFF);
            printf("EXPECT %d%d%d%d%d %d% d%d\n", expected_f.s, expected_f.z, expected_f.b5, expected_f.h, expected_f.b3, expected_f.p_v, expected_f.n, expected_f.c);
            printf("ACTUAL %d%d%d%d%d %d %d%d\n", Z80::z80.f.s, Z80::z80.f.z, Z80::z80.f.b5, Z80::z80.f.h, Z80::z80.f.b3, Z80::z80.f.p_v, Z80::z80.f.n, Z80::z80.f.c);
        }
        return true;
    }
    return false;
}
/*
PC: 0125, AF: C384, BC: 0009, DE: 1DDA, HL: 013B, SP: C900, IX: 0000, IY: 0000, I: 00, R: 16	(CA 2F 01 2B), cyc: 228
 */
void check_log(std::string& line) {
    bool any_bad = check("PC", line.substr(4, 4), Z80::z80.pc);
    u16 af = Z80::z80.a;
    af <<= 8;
    af |= Z80::z80.f.assemble();
    //std::cout << "checking: " << line << endl;
    any_bad |= check("AF", line.substr(14, 4), af);
    any_bad |= check("BC", line.substr(24, 4), Z80::z80.bc.raw);
    any_bad |= check("DE", line.substr(34, 4), Z80::z80.de.raw);
    any_bad |= check("HL", line.substr(44, 4), Z80::z80.hl.raw);
    any_bad |= check("SP", line.substr(54, 4), Z80::z80.sp);
    any_bad |= check("IX", line.substr(64, 4), Z80::z80.ix);
    any_bad |= check("IY", line.substr(74, 4), Z80::z80.iy);
    any_bad |= check("I",  line.substr(83, 2), Z80::z80.i);

    if (any_bad) {
        exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        cout << "Usage: " << argv[0] << " <test> <log>" << endl;
        exit(1);
    }
    bool log_loaded = false;
    std::ifstream f;
    if (argc == 3) {
        log_loaded = true;
        f = std::ifstream(argv[2]);
    }

    memset(memory, 0x00, 65535);

    memory[0x05] = 0xDB;
    memory[0x06] = 0x00;
    memory[0x07] = 0xC9;

    std::cout << "Loaded CPM test: " << argv[1] << std::endl;
    Z80::reset();
    Z80::set_bus_handlers(read_byte, write_byte);
    Z80::set_pc(0x100);
    load_rom(argv[1]);
    std::string line;
    while (true) {
        if (log_loaded) {
            getline(f, line);
            check_log(line);
        }
        Z80::step();
        // Syscall!
        if (Z80::z80.pc == 5) {
            u8 syscall = Z80::z80.bc[Z80::WideRegister::Lo];

            switch (syscall) {
                case 9: { // Print all characters until '$' is found
                    u16 addr = Z80::z80.de.raw;
                    for (char c = (char)memory[addr++]; c != '$'; c = (char)memory[addr++]) {
                        printf("%c", c);
                    }
                    break;
                }
                default:
                    logfatal("Unknown syscall %d!", syscall);
            }
        }
    }
}