#include <iostream>
#include <cstring>

#include "z80/z80.h"
#include "util/load_bin.h"
#include "util/types.h"
#include "util/log.h"

using std::cout;
using std::endl;

u8 memory[0x10000];
bool should_quit = false;

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

u8 port_in(u8 port) {
    u8 syscall = Z80::z80.bc[Z80::WideRegister::Lo];

    switch (syscall) {
        case 9: { // Print all characters until '$' is found
            u16 addr = Z80::z80.de.raw;
            for (char c = (char)memory[addr++]; c != '$'; c = (char)memory[addr++]) {
                printf("%c", c);
            }
            break;
        }
        case 2: {
            printf("%c", Z80::z80.de[Z80::WideRegister::Lo]);
            break;
        }
        default:
            logfatal("Unknown syscall %d!", syscall);
    }

    return 0xFF;
}

void port_out(u8 port, u8 value) {
    should_quit = true; // Success!
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <test>" << endl;
        exit(1);
    }

    memset(memory, 0x00, 65535);

    memory[0x00] = 0xD3;
    memory[0x01] = 0x00;
    memory[0x05] = 0xDB;
    memory[0x06] = 0x00;
    memory[0x07] = 0xC9;

    std::cout << "Loaded CPM test: " << argv[1] << std::endl;
    Z80::reset();
    Z80::set_bus_handlers(read_byte, write_byte);
    Z80::set_port_handlers(port_in, port_out);
    Z80::set_pc(0x100);
    load_rom(argv[1]);
    while (!should_quit) {
        Z80::step();
    }
    exit(0);
}