#include <iostream>
#include <cstring>
#include <unordered_set>

#include "z80/z80.h"
#include "util/load_bin.h"
#include "util/types.h"
#include "util/log.h"

#include "superzazu_z80/z80.h"

using std::cout;
using std::endl;

z80 REFERENCE;
u8 memory[0x10000];
u8 REFERENCE_memory[0x10000];
bool should_quit = false;

long steps = 0;
long start_checking_at = 5'680'750'400;
//long start_checking_at = 5'348'006'623;
//long start_checking_at = 0;

std::unordered_set<u16> addresses_written(2);

u8 read_byte(u16 address) {
    if (memory[address] != REFERENCE_memory[address]) {
        logfatal("Address: %04X expected (reference) %02X but is (mine) %02X", address, REFERENCE_memory[address], memory[address]);
    }
    return memory[address];
}

void write_byte(u16 address, u8 value) {
    addresses_written.insert(address);
    memory[address] = value;
}

u8 REFERENCE_read_byte(void* ud, u16 address) {
    return REFERENCE_memory[address];
}

void REFERENCE_write_byte(void* ud, u16 address, u8 value) {
    addresses_written.insert(address);
    REFERENCE_memory[address] = value;
}

void load_rom(const char* path) {
    auto data = load_bin<u8>(path);
    for (unsigned int i = 0; i < data.size(); i++) {
        memory[0x100 + i] = data[i];
        REFERENCE_memory[0x100 + i] = data[i];
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

u8 REFERENCE_port_in(z80* z, u8 port) { return 0xFF; }
void REFERENCE_port_out(z80* z, u8 port, u8 value) {}

void REFERENCE_init() {
    z80_init(&REFERENCE);
    REFERENCE.read_byte = REFERENCE_read_byte;
    REFERENCE.write_byte = REFERENCE_write_byte;
    REFERENCE.port_in = REFERENCE_port_in;
    REFERENCE.port_out = REFERENCE_port_out;
}

void REFERENCE_step() {
    z80_step(&REFERENCE);
}

template <typename T>
bool check(const char* name, T reference, T mine) {
    if (reference != mine) {
        if (std::is_same_v<T, u8>) {
            logalways("%s mismatch: reference: %02X != mine: %02X!", name, reference, mine);
        } else if (std::is_same_v<T, u16>) {
            logalways("%s mismatch: reference: %04X != mine: %04X!", name, reference, mine);
        } else if (std::is_same_v<T, bool>) {
            logalways("%s mismatch: reference: %s != mine: %s!", name, reference ? "true" : "false", mine ? "true" : "false");
        } else {
            logalways("%s: mismatched values, unsure how to display!", name);
        }
        return true;
    } else {
        return false;
    }
}

void REFERENCE_check(u16 old_pc, u8 old_instr[4]) {
    bool any_bad = false;

    any_bad |= check("PC", REFERENCE.pc, Z80::z80.pc);
    any_bad |= check("SP", REFERENCE.sp, Z80::z80.sp);

    any_bad |= check("flag S", REFERENCE.sf, Z80::z80.f.s);
    any_bad |= check("flag Z", REFERENCE.zf, Z80::z80.f.z);
    any_bad |= check("flag Y(b5)", REFERENCE.yf, Z80::z80.f.b5);
    any_bad |= check("flag H", REFERENCE.hf, Z80::z80.f.h);
    any_bad |= check("flag X(b3)", REFERENCE.xf, Z80::z80.f.b3);
    any_bad |= check("flag P/V", REFERENCE.pf, Z80::z80.f.p_v);
    any_bad |= check("flag N", REFERENCE.nf, Z80::z80.f.n);
    any_bad |= check("flag C", REFERENCE.cf, Z80::z80.f.c);

    any_bad |= check("A", REFERENCE.a, Z80::z80.a);

    any_bad |= check("B", REFERENCE.b, (u8)Z80::z80.bc[Z80::WideRegister::Hi]);
    any_bad |= check("C", REFERENCE.c, (u8)Z80::z80.bc[Z80::WideRegister::Lo]);

    any_bad |= check("D", REFERENCE.d, (u8)Z80::z80.de[Z80::WideRegister::Hi]);
    any_bad |= check("E", REFERENCE.e, (u8)Z80::z80.de[Z80::WideRegister::Lo]);

    any_bad |= check("H", REFERENCE.h, (u8)Z80::z80.hl[Z80::WideRegister::Hi]);
    any_bad |= check("L", REFERENCE.l, (u8)Z80::z80.hl[Z80::WideRegister::Lo]);

    any_bad |= check("IX", REFERENCE.ix, Z80::z80.ix.raw);
    any_bad |= check("IY", REFERENCE.iy, Z80::z80.iy.raw);
    any_bad |= check("I", REFERENCE.i, Z80::z80.i);
    //any_bad |= check("R", REFERENCE.r, Z80::z80.r);

    for (const u16 &address : addresses_written) {
        if (memory[address] != REFERENCE_memory[address]) {
            logalways("written: (%04X) should be (reference) %02X but is (mine) %02X!", address, REFERENCE_memory[address], memory[address]);
            any_bad = true;
        }
    }

    if (any_bad) {
        logalways("Steps: %ld", steps);
        logalways("Last PC: %04X", old_pc);
        logalways("Last instruction: %02X %02X %02X %02X", old_instr[0], old_instr[1], old_instr[2], old_instr[3]);

        logalways("Checking memory...");
        bool any_mem_bad = false;
        for (int i = 0; i < 0x10000; i++) {
            if (memory[i] != REFERENCE_memory[i]) {
                logalways("(%04X) should be (reference) %02X but is (mine) %02X!", i, REFERENCE_memory[i], memory[i]);
                any_mem_bad = true;
            }
        }

        if (!any_mem_bad) {
            logalways("All memory looks good.");
        }

        exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <test>" << endl;
        exit(1);
    }

    memset(memory, 0x00, 65535);
    memset(REFERENCE_memory, 0x00, 65535);

    memory[0x00] = 0xD3;
    memory[0x01] = 0x00;
    memory[0x05] = 0xDB;
    memory[0x06] = 0x00;
    memory[0x07] = 0xC9;

    REFERENCE_memory[0x00] = 0xD3;
    REFERENCE_memory[0x01] = 0x00;
    REFERENCE_memory[0x05] = 0xDB;
    REFERENCE_memory[0x06] = 0x00;
    REFERENCE_memory[0x07] = 0xC9;

    REFERENCE_init();

    logalways("Loaded CPM test: %s", argv[1]);
    Z80::reset();
    Z80::set_bus_handlers(read_byte, write_byte);
    Z80::set_port_handlers(port_in, port_out);
    Z80::set_pc(0x100);
    REFERENCE.pc = 0x100;
    load_rom(argv[1]);
    while (!should_quit) {
        if (!addresses_written.empty()) {
            addresses_written.clear();
        }

        u16 old_pc = Z80::z80.pc;
        u8 old_instr[] = {memory[old_pc + 0], memory[old_pc + 1], memory[old_pc + 2], memory[old_pc + 3]};
        Z80::step();
        REFERENCE_step();
        if (++steps >= start_checking_at) {
            REFERENCE_check(old_pc, old_instr);
        }
    }
    exit(0);
}