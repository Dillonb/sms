#include "instructions.h"

#include <cstdio>
#include <cstdlib>

#include "z80.h"
#include "util/types.h"
#include "util/log.h"

using Z80::z80;
using Z80::WideRegister;
using Z80::Register;
using Z80::reg_type;

namespace {
    enum class Condition {
        Always,
        Z,
        NZ,
    };
    enum class AddressingMode {
        Immediate,
        Indirect,
        HL
    };

    u16 read_16(u16 address) {
        u16 lo = z80.read_byte(address + 0);
        u16 hi = z80.read_byte(address + 1);
        return lo | (hi << 8);
    }

    u16 read_16_pc() {
        u16 value = read_16(z80.pc);
        z80.pc += 2;
        return value;
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    T get_register() {
        switch (reg) {
            case Register::A:
                return z80.a;
            case Register::F:
                return z80.f.assemble();
            case Register::AF:
                return ((u16)z80.a << 8) | z80.f.assemble();
            case Register::AF_:
                return z80.af_;
            case Register::B:
                return z80.bc[Z80::WideRegister::Hi];
            case Register::C:
                return z80.bc[Z80::WideRegister::Lo];
            case Register::BC:
                return z80.bc.raw;
            case Register::BC_:
                return z80.bc_;
            case Register::D:
                return z80.de[Z80::WideRegister::Hi];
            case Register::E:
                return z80.de[Z80::WideRegister::Lo];
            case Register::DE:
                return z80.de.raw;
            case Register::DE_:
                return z80.de_;
            case Register::H:
                return z80.hl[Z80::WideRegister::Hi];
            case Register::L:
                return z80.hl[Z80::WideRegister::Lo];
            case Register::HL:
                return z80.hl.raw;
            case Register::HL_:
                return z80.hl_;
            case Register::SP:
                return z80.sp;
        }
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    void set_register(T value) {
        switch (reg) {
            case Register::A:
                z80.a = value;
                break;
            case Register::F:
                z80.f.set(value);
                break;
            case Register::AF:
                z80.a = value >> 8;
                z80.f.set(value & 0xFF);
                break;
            case Register::AF_:
                z80.af_ = value;
            case Register::B:
                z80.bc(Z80::WideRegister::Hi) = value;
                break;
            case Register::C:
                z80.bc(Z80::WideRegister::Lo) = value;
                break;
            case Register::BC:
                z80.bc.raw = value;
                break;
            case Register::BC_:
                z80.bc_ = value;
                break;
            case Register::D:
                z80.de(Z80::WideRegister::Hi) = value;
                break;
            case Register::E:
                z80.de(Z80::WideRegister::Lo) = value;
                break;
            case Register::DE:
                z80.de.raw = value;
                break;
            case Register::DE_:
                z80.de_ = value;
                break;
            case Register::H:
                z80.hl(Z80::WideRegister::Hi) = value;
                break;
            case Register::L:
                z80.hl(Z80::WideRegister::Lo) = value;
                break;
            case Register::HL:
                z80.hl.raw = value;
                break;
            case Register::HL_:
                z80.hl_ = value;
                break;
            case Register::SP:
                z80.sp = value;
        }
    }

    template <Register a, Register b, typename aT = typename reg_type<a>::type, typename bT = typename reg_type<b>::type>
    void swap_registers() {
        static_assert(sizeof(aT) == sizeof(bT), "Types of swapped registers must be the same.");
        u16 temp = get_register<a>();
        set_register<a>(get_register<b>());
        set_register<b>(temp);
    }

    template <Condition c>
    bool check_condition() {
        switch (c) {
            case Condition::Always:
                return true;
            case Condition::Z:
                return z80.f.z;
            case Condition::NZ:
                return !z80.f.z;
        }
    }

    template <AddressingMode addressingMode>
    u16 get_address() {
        switch (addressingMode) {
            case AddressingMode::Indirect:
                return read_16_pc();
            case AddressingMode::HL:
                return z80.hl.raw;
        }
    }

    template <AddressingMode addressingMode, typename T>
    T read_value() {
        switch (addressingMode) {
            case AddressingMode::Immediate:
                switch (sizeof(T)) {
                    case sizeof(u16): {
                        return read_16_pc();
                    }
                    case sizeof(u8):
                        return z80.read_byte(z80.pc++);
                }
                break;
            case AddressingMode::Indirect: {
                u16 address = get_address<AddressingMode::Indirect>();
                if constexpr(std::is_same_v<T, u16>) {
                    return read_16(address);
                } else if constexpr(std::is_same_v<T, u8>) {
                    return z80.read_byte(address);
                }
            }
            case AddressingMode::HL:
                return read_16(get_register<Register::HL>());
        }
    }

    template <AddressingMode addressingMode, typename T>
    void write_value(T value) {
        static_assert(std::is_same_v<T, u8>, "only supported for u8");
        z80.write_byte(get_address<addressingMode>(), value);
    }

    template <typename T>
    void stack_push(T value) {
        if constexpr(std::is_same_v<T, u16>) {
            u8 hi = (value >> 8) & 0xFF;
            u8 lo = value & 0xFF;
            stack_push<u8>(hi);
            stack_push<u8>(lo);
        } else if constexpr(std::is_same_v<T, u8>) {
            z80.write_byte(--z80.sp, value);
        }
    }

    template <typename T>
    T stack_pop() {
        if constexpr(std::is_same_v<T, u16>) {
            u16 lo = stack_pop<u8>();
            u16 hi = stack_pop<u8>();
            return (hi << 8) | lo;
        } else if constexpr(std::is_same_v<T, u8>) {
            return z80.read_byte(z80.sp++);
        }
    }

    bool parity(u8 value) {
        int num_1_bits = 0;
        for (int i = 0; i < 8; i++) {
            num_1_bits += (value >> i) & 1;
        }
        return (num_1_bits % 2) == 0;
    }
}

namespace Z80 {
    template <u8 opc>
    int unimplemented_instr() {
        printf("Unimplemented instruction %02X!\n", opc);
        exit(1);
    }

    template <u8 opc>
    int unimplemented_ed_instr() {
        printf("Unimplemented ED instruction %02X!\n", opc);
        exit(1);
    }

    template <Condition c, AddressingMode addressingMode>
    int instr_jp() {
        u16 address = read_16_pc();

        if (check_condition<c>()) {
            z80.pc = address;
            logtrace("Jumped to %04X", z80.pc);
        }

        if (addressingMode == AddressingMode::Immediate) {
            return 10;
        } else {
            return 4;
        }
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    int instr_dec() {
        switch (sizeof(T)) {
            case sizeof(u8): {
                u8 m = get_register<reg>();
                u8 r = m - 1;
                set_register<reg>(r);

                z80.f.s = ((s8)r) < 0;
                z80.f.z = r == 0;
                z80.f.h = (r & 0xF) > (m & 0xF); // overflow on lower half of reg
                z80.f.p_v = m == 0x80;
                z80.f.n = true;

                return 4;
            }
            case sizeof(u16): {
                u16 m = get_register<reg>();
                u16 r = m - 1;
                set_register<reg>(r);
                return 6;
            }
        }
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    int instr_inc() {
        switch (sizeof(T)) {
            case sizeof(u16): {
                u16 m = get_register<reg>();
                u16 r = m + 1;
                set_register<reg>(r);
                return 6;
            }
        }
    }


    template <Register dst, Register src>
    int instr_ld() {
        set_register<dst>(get_register<src>());
        return 4;
    }


    template <Register dst, AddressingMode src, typename dstT = typename reg_type<dst>::type>
    int instr_ld() {
        set_register<dst>(read_value<src, dstT>());
        if (sizeof(dstT) == sizeof(u16)) {
            return 16;
        } else {
            return 7;
        }
    }

    template <AddressingMode dst, Register src, typename srcT = typename reg_type<src>::type>
    int instr_ld() {
        z80.write_byte(get_address<dst>(), get_register<src>());
        if (sizeof(srcT) == sizeof(u16)) {
            return 16;
        } else {
            return 13;
        }
    }

    template <AddressingMode dst, AddressingMode src>
    int instr_ld() {
        u8 val = read_value<src, u8>();
        write_value<dst, u8>(val);

        return 10;
    }

    template <Condition c>
    int instr_call() {
        // Read the address first so the return address is correct
        u16 address = get_address<AddressingMode::Indirect>();

        if (check_condition<c>()) {
            // Push return address
            stack_push<u16>(z80.pc);
            logwarn("Calling function at %04X - return address is %04X", address, z80.pc);
            z80.pc = address;
            return 17;
        }
        return 10;
    }

    template <Condition c>
    int instr_ret() {
        if (check_condition<c>()) {
            // Push return address
            z80.pc = stack_pop<u16>();
            logwarn("Returning to address %04X", z80.pc);
            if (c == Condition::Always) {
                return 10;
            } else {
                return 11;
            }
        }
        return 5;
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    int instr_push() {
        stack_push<T>(get_register<reg>());
        return 11;
    }

    template <Register reg, typename T = typename reg_type<reg>::type>
    int instr_pop() {
        set_register<reg>(stack_pop<T>());
        return 10;
    }

    template <AddressingMode addressingMode>
    int instr_or() {
        z80.a = z80.a | read_value<addressingMode, u8>();
        z80.f.s = ((s8)z80.a) < 0;
        z80.f.z = z80.a == 0;
        z80.f.h = false;
        z80.f.p_v = parity(z80.a);
        z80.f.n = false;
        z80.f.c = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 7;
    }

    template <Register dst, Register src>
    int instr_add() {
        if (get_register_size<dst>() == sizeof(u16) && get_register_size<src>() == sizeof(u16)) {
            u32 op1 = get_register<dst>();
            u32 op2 = get_register<src>();
            u32 res = op1 + op2;
            set_register<dst>(res & 0xFFFF);

            z80.f.h = ((op1 & 0xFFF) + (op2 & 0xFFF)) > 0xFFF; // Carry into bit 12
            z80.f.n = false;
            z80.f.c = res > 0xFFFF;
            return 11;
        }
        logfatal("aaa");
    }

    bool vflag(u8 a, u8 b, u8 r) {
        return ((a & 0x80) == (b & 0x80)) && ((a & 0x80) != (r & 0x80));
    }

    template <AddressingMode addressingMode>
    int instr_cp() {
        u8 s = read_value<addressingMode, u8>();
        u8 r = z80.a - s;

        z80.f.s = ((s8)r) < 0;
        z80.f.z = r == 0;
        z80.f.h = (s & 0xF) > (z80.a & 0xF); // overflow on lower half of reg
        z80.f.p_v = vflag(z80.a, ~s + 1, r);
        z80.f.n = true;
        z80.f.c = s > z80.a;
        z80.f.b3 = (r >> 3) & 1;
        z80.f.b5 = (r >> 5) & 1;

        return 7;
    }



    int instr_ex_af() {
        swap_registers<Register::AF, Register::AF_>();
        return 4;
    }

    int instr_exx() {
        swap_registers<Register::BC, Register::BC_>();
        swap_registers<Register::DE, Register::DE_>();
        swap_registers<Register::HL, Register::HL_>();
        return 4;
    }

    int instr_ex_de_hl() {
        swap_registers<Register::DE, Register::HL>();
        return 4;
    }

    int instr_out() {
        // stubbed for now
        z80.pc++;
        return 4;
    }

    int instr_ed() {
        return ed_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_nop() {
        return 4;
    }

    const instruction instructions[0x100] = {
            /* 0x00 */ instr_nop,
            /* 0x01 */ instr_ld<Register::BC, AddressingMode::Immediate>,
            /* 0x02 */ unimplemented_instr<0x02>,
            /* 0x03 */ unimplemented_instr<0x03>,
            /* 0x04 */ unimplemented_instr<0x04>,
            /* 0x05 */ unimplemented_instr<0x05>,
            /* 0x06 */ unimplemented_instr<0x06>,
            /* 0x07 */ unimplemented_instr<0x07>,
            /* 0x08 */ instr_ex_af,
            /* 0x09 */ instr_add<Register::HL, Register::BC>,
            /* 0x0A */ unimplemented_instr<0x0A>,
            /* 0x0B */ instr_dec<Register::BC>,
            /* 0x0C */ unimplemented_instr<0x0C>,
            /* 0x0D */ instr_dec<Register::C>,
            /* 0x0E */ instr_ld<Register::C, AddressingMode::Immediate>,
            /* 0x0F */ unimplemented_instr<0x0F>,
            /* 0x10 */ unimplemented_instr<0x10>,
            /* 0x11 */ instr_ld<Register::DE, AddressingMode::Immediate>,
            /* 0x12 */ unimplemented_instr<0x12>,
            /* 0x13 */ instr_inc<Register::DE>,
            /* 0x14 */ unimplemented_instr<0x14>,
            /* 0x15 */ unimplemented_instr<0x15>,
            /* 0x16 */ unimplemented_instr<0x16>,
            /* 0x17 */ unimplemented_instr<0x17>,
            /* 0x18 */ unimplemented_instr<0x18>,
            /* 0x19 */ instr_add<Register::HL, Register::DE>,
            /* 0x1A */ unimplemented_instr<0x1A>,
            /* 0x1B */ unimplemented_instr<0x1B>,
            /* 0x1C */ unimplemented_instr<0x1C>,
            /* 0x1D */ instr_dec<Register::E>,
            /* 0x1E */ unimplemented_instr<0x1E>,
            /* 0x1F */ unimplemented_instr<0x1F>,
            /* 0x20 */ unimplemented_instr<0x20>,
            /* 0x21 */ instr_ld<Register::HL, AddressingMode::Immediate>,
            /* 0x22 */ unimplemented_instr<0x22>,
            /* 0x23 */ instr_inc<Register::HL>,
            /* 0x24 */ unimplemented_instr<0x24>,
            /* 0x25 */ unimplemented_instr<0x25>,
            /* 0x26 */ unimplemented_instr<0x26>,
            /* 0x27 */ unimplemented_instr<0x27>,
            /* 0x28 */ unimplemented_instr<0x28>,
            /* 0x29 */ unimplemented_instr<0x29>,
            /* 0x2A */ instr_ld<Register::HL, AddressingMode::Indirect>,
            /* 0x2B */ instr_dec<Register::HL>,
            /* 0x2C */ unimplemented_instr<0x2C>,
            /* 0x2D */ unimplemented_instr<0x2D>,
            /* 0x2E */ unimplemented_instr<0x2E>,
            /* 0x2F */ unimplemented_instr<0x2F>,
            /* 0x30 */ unimplemented_instr<0x30>,
            /* 0x31 */ instr_ld<Register::SP, AddressingMode::Immediate>,
            /* 0x32 */ instr_ld<AddressingMode::Indirect, Register::A>,
            /* 0x33 */ unimplemented_instr<0x33>,
            /* 0x34 */ unimplemented_instr<0x34>,
            /* 0x35 */ unimplemented_instr<0x35>,
            /* 0x36 */ instr_ld<AddressingMode::HL, AddressingMode::Immediate>,
            /* 0x37 */ unimplemented_instr<0x37>,
            /* 0x38 */ unimplemented_instr<0x38>,
            /* 0x39 */ unimplemented_instr<0x39>,
            /* 0x3A */ unimplemented_instr<0x3A>,
            /* 0x3B */ unimplemented_instr<0x3B>,
            /* 0x3C */ unimplemented_instr<0x3C>,
            /* 0x3D */ unimplemented_instr<0x3D>,
            /* 0x3E */ instr_ld<Register::A, AddressingMode::Immediate>,
            /* 0x3F */ unimplemented_instr<0x3F>,
            /* 0x40 */ instr_ld<Register::B, Register::B>,
            /* 0x41 */ unimplemented_instr<0x41>,
            /* 0x42 */ unimplemented_instr<0x42>,
            /* 0x43 */ unimplemented_instr<0x43>,
            /* 0x44 */ unimplemented_instr<0x44>,
            /* 0x45 */ unimplemented_instr<0x45>,
            /* 0x46 */ unimplemented_instr<0x46>,
            /* 0x47 */ unimplemented_instr<0x47>,
            /* 0x48 */ unimplemented_instr<0x48>,
            /* 0x49 */ unimplemented_instr<0x49>,
            /* 0x4A */ unimplemented_instr<0x4A>,
            /* 0x4B */ unimplemented_instr<0x4B>,
            /* 0x4C */ unimplemented_instr<0x4C>,
            /* 0x4D */ unimplemented_instr<0x4D>,
            /* 0x4E */ unimplemented_instr<0x4E>,
            /* 0x4F */ unimplemented_instr<0x4F>,
            /* 0x50 */ unimplemented_instr<0x50>,
            /* 0x51 */ unimplemented_instr<0x51>,
            /* 0x52 */ unimplemented_instr<0x52>,
            /* 0x53 */ unimplemented_instr<0x53>,
            /* 0x54 */ instr_ld<Register::D, Register::H>,
            /* 0x55 */ unimplemented_instr<0x55>,
            /* 0x56 */ unimplemented_instr<0x56>,
            /* 0x57 */ unimplemented_instr<0x57>,
            /* 0x58 */ unimplemented_instr<0x58>,
            /* 0x59 */ unimplemented_instr<0x59>,
            /* 0x5A */ unimplemented_instr<0x5A>,
            /* 0x5B */ unimplemented_instr<0x5B>,
            /* 0x5C */ unimplemented_instr<0x5C>,
            /* 0x5D */ instr_ld<Register::E, Register::L>,
            /* 0x5E */ unimplemented_instr<0x5E>,
            /* 0x5F */ unimplemented_instr<0x5F>,
            /* 0x60 */ unimplemented_instr<0x60>,
            /* 0x61 */ unimplemented_instr<0x61>,
            /* 0x62 */ instr_ld<Register::H, Register::D>,
            /* 0x63 */ unimplemented_instr<0x63>,
            /* 0x64 */ unimplemented_instr<0x64>,
            /* 0x65 */ unimplemented_instr<0x65>,
            /* 0x66 */ instr_ld<Register::H, AddressingMode::HL>,
            /* 0x67 */ unimplemented_instr<0x67>,
            /* 0x68 */ unimplemented_instr<0x68>,
            /* 0x69 */ unimplemented_instr<0x69>,
            /* 0x6A */ unimplemented_instr<0x6A>,
            /* 0x6B */ unimplemented_instr<0x6B>,
            /* 0x6C */ unimplemented_instr<0x6C>,
            /* 0x6D */ unimplemented_instr<0x6D>,
            /* 0x6E */ unimplemented_instr<0x6E>,
            /* 0x6F */ instr_ld<Register::L, Register::A>,
            /* 0x70 */ unimplemented_instr<0x70>,
            /* 0x71 */ unimplemented_instr<0x71>,
            /* 0x72 */ unimplemented_instr<0x72>,
            /* 0x73 */ unimplemented_instr<0x73>,
            /* 0x74 */ unimplemented_instr<0x74>,
            /* 0x75 */ unimplemented_instr<0x75>,
            /* 0x76 */ unimplemented_instr<0x76>,
            /* 0x77 */ unimplemented_instr<0x77>,
            /* 0x78 */ unimplemented_instr<0x78>,
            /* 0x79 */ unimplemented_instr<0x79>,
            /* 0x7A */ unimplemented_instr<0x7A>,
            /* 0x7B */ unimplemented_instr<0x7B>,
            /* 0x7C */ instr_ld<Register::A, Register::H>,
            /* 0x7D */ instr_ld<Register::A, Register::L>,
            /* 0x7E */ instr_ld<Register::A, AddressingMode::HL>,
            /* 0x7F */ unimplemented_instr<0x7F>,
            /* 0x80 */ unimplemented_instr<0x80>,
            /* 0x81 */ unimplemented_instr<0x81>,
            /* 0x82 */ unimplemented_instr<0x82>,
            /* 0x83 */ unimplemented_instr<0x83>,
            /* 0x84 */ unimplemented_instr<0x84>,
            /* 0x85 */ unimplemented_instr<0x85>,
            /* 0x86 */ unimplemented_instr<0x86>,
            /* 0x87 */ unimplemented_instr<0x87>,
            /* 0x88 */ unimplemented_instr<0x88>,
            /* 0x89 */ unimplemented_instr<0x89>,
            /* 0x8A */ unimplemented_instr<0x8A>,
            /* 0x8B */ unimplemented_instr<0x8B>,
            /* 0x8C */ unimplemented_instr<0x8C>,
            /* 0x8D */ unimplemented_instr<0x8D>,
            /* 0x8E */ unimplemented_instr<0x8E>,
            /* 0x8F */ unimplemented_instr<0x8F>,
            /* 0x90 */ unimplemented_instr<0x90>,
            /* 0x91 */ unimplemented_instr<0x91>,
            /* 0x92 */ unimplemented_instr<0x92>,
            /* 0x93 */ unimplemented_instr<0x93>,
            /* 0x94 */ unimplemented_instr<0x94>,
            /* 0x95 */ unimplemented_instr<0x95>,
            /* 0x96 */ unimplemented_instr<0x96>,
            /* 0x97 */ unimplemented_instr<0x97>,
            /* 0x98 */ unimplemented_instr<0x98>,
            /* 0x99 */ unimplemented_instr<0x99>,
            /* 0x9A */ unimplemented_instr<0x9A>,
            /* 0x9B */ unimplemented_instr<0x9B>,
            /* 0x9C */ unimplemented_instr<0x9C>,
            /* 0x9D */ unimplemented_instr<0x9D>,
            /* 0x9E */ unimplemented_instr<0x9E>,
            /* 0x9F */ unimplemented_instr<0x9F>,
            /* 0xA0 */ unimplemented_instr<0xA0>,
            /* 0xA1 */ unimplemented_instr<0xA1>,
            /* 0xA2 */ unimplemented_instr<0xA2>,
            /* 0xA3 */ unimplemented_instr<0xA3>,
            /* 0xA4 */ unimplemented_instr<0xA4>,
            /* 0xA5 */ unimplemented_instr<0xA5>,
            /* 0xA6 */ unimplemented_instr<0xA6>,
            /* 0xA7 */ unimplemented_instr<0xA7>,
            /* 0xA8 */ unimplemented_instr<0xA8>,
            /* 0xA9 */ unimplemented_instr<0xA9>,
            /* 0xAA */ unimplemented_instr<0xAA>,
            /* 0xAB */ unimplemented_instr<0xAB>,
            /* 0xAC */ unimplemented_instr<0xAC>,
            /* 0xAD */ unimplemented_instr<0xAD>,
            /* 0xAE */ unimplemented_instr<0xAE>,
            /* 0xAF */ unimplemented_instr<0xAF>,
            /* 0xB0 */ unimplemented_instr<0xB0>,
            /* 0xB1 */ unimplemented_instr<0xB1>,
            /* 0xB2 */ unimplemented_instr<0xB2>,
            /* 0xB3 */ unimplemented_instr<0xB3>,
            /* 0xB4 */ unimplemented_instr<0xB4>,
            /* 0xB5 */ unimplemented_instr<0xB5>,
            /* 0xB6 */ instr_or<AddressingMode::HL>,
            /* 0xB7 */ unimplemented_instr<0xB7>,
            /* 0xB8 */ unimplemented_instr<0xB8>,
            /* 0xB9 */ unimplemented_instr<0xB9>,
            /* 0xBA */ unimplemented_instr<0xBA>,
            /* 0xBB */ unimplemented_instr<0xBB>,
            /* 0xBC */ unimplemented_instr<0xBC>,
            /* 0xBD */ unimplemented_instr<0xBD>,
            /* 0xBE */ unimplemented_instr<0xBE>,
            /* 0xBF */ unimplemented_instr<0xBF>,
            /* 0xC0 */ unimplemented_instr<0xC0>,
            /* 0xC1 */ instr_pop<Register::BC>,
            /* 0xC2 */ instr_jp<Condition::NZ, AddressingMode::Immediate>,
            /* 0xC3 */ instr_jp<Condition::Always, AddressingMode::Immediate>,
            /* 0xC4 */ unimplemented_instr<0xC4>,
            /* 0xC5 */ instr_push<Register::BC>,
            /* 0xC6 */ unimplemented_instr<0xC6>,
            /* 0xC7 */ unimplemented_instr<0xC7>,
            /* 0xC8 */ unimplemented_instr<0xC8>,
            /* 0xC9 */ instr_ret<Condition::Always>,
            /* 0xCA */ instr_jp<Condition::Z, AddressingMode::Immediate>,
            /* 0xCB */ unimplemented_instr<0xCB>,
            /* 0xCC */ unimplemented_instr<0xCC>,
            /* 0xCD */ instr_call<Condition::Always>,
            /* 0xCE */ unimplemented_instr<0xCE>,
            /* 0xCF */ unimplemented_instr<0xCF>,
            /* 0xD0 */ unimplemented_instr<0xD0>,
            /* 0xD1 */ instr_pop<Register::DE>,
            /* 0xD2 */ unimplemented_instr<0xD2>,
            /* 0xD3 */ unimplemented_instr<0xD3>,
            /* 0xD4 */ unimplemented_instr<0xD4>,
            /* 0xD5 */ instr_push<Register::DE>,
            /* 0xD6 */ unimplemented_instr<0xD6>,
            /* 0xD7 */ unimplemented_instr<0xD7>,
            /* 0xD8 */ unimplemented_instr<0xD8>,
            /* 0xD9 */ instr_exx,
            /* 0xDA */ unimplemented_instr<0xDA>,
            /* 0xDB */ instr_out, //unimplemented_instr<0xDB>,
            /* 0xDC */ unimplemented_instr<0xDC>,
            /* 0xDD */ unimplemented_instr<0xDD>,
            /* 0xDE */ unimplemented_instr<0xDE>,
            /* 0xDF */ unimplemented_instr<0xDF>,
            /* 0xE0 */ unimplemented_instr<0xE0>,
            /* 0xE1 */ instr_pop<Register::HL>,
            /* 0xE2 */ unimplemented_instr<0xE2>,
            /* 0xE3 */ unimplemented_instr<0xE3>,
            /* 0xE4 */ unimplemented_instr<0xE4>,
            /* 0xE5 */ instr_push<Register::HL>,
            /* 0xE6 */ unimplemented_instr<0xE6>,
            /* 0xE7 */ unimplemented_instr<0xE7>,
            /* 0xE8 */ unimplemented_instr<0xE8>,
            /* 0xE9 */ unimplemented_instr<0xE9>,
            /* 0xEA */ unimplemented_instr<0xEA>,
            /* 0xEB */ instr_ex_de_hl,
            /* 0xEC */ unimplemented_instr<0xEC>,
            /* 0xED */ instr_ed,
            /* 0xEE */ unimplemented_instr<0xEE>,
            /* 0xEF */ unimplemented_instr<0xEF>,
            /* 0xF0 */ unimplemented_instr<0xF0>,
            /* 0xF1 */ instr_pop<Register::AF>,
            /* 0xF2 */ unimplemented_instr<0xF2>,
            /* 0xF3 */ unimplemented_instr<0xF3>,
            /* 0xF4 */ unimplemented_instr<0xF4>,
            /* 0xF5 */ instr_push<Register::AF>,
            /* 0xF6 */ unimplemented_instr<0xF6>,
            /* 0xF7 */ unimplemented_instr<0xF7>,
            /* 0xF8 */ unimplemented_instr<0xF8>,
            /* 0xF9 */ instr_ld<Register::SP, Register::HL>,
            /* 0xFA */ unimplemented_instr<0xFA>,
            /* 0xFB */ unimplemented_instr<0xFB>,
            /* 0xFC */ unimplemented_instr<0xFC>,
            /* 0xFD */ unimplemented_instr<0xFD>,
            /* 0xFE */ instr_cp<AddressingMode::Immediate>,
            /* 0xFF */ unimplemented_instr<0xFF>,
    };

    const instruction ed_instructions[0xC0] = {
            /* 0x00 */ unimplemented_ed_instr<0x00>,
            /* 0x01 */ unimplemented_ed_instr<0x01>,
            /* 0x02 */ unimplemented_ed_instr<0x02>,
            /* 0x03 */ unimplemented_ed_instr<0x03>,
            /* 0x04 */ unimplemented_ed_instr<0x04>,
            /* 0x05 */ unimplemented_ed_instr<0x05>,
            /* 0x06 */ unimplemented_ed_instr<0x06>,
            /* 0x07 */ unimplemented_ed_instr<0x07>,
            /* 0x08 */ unimplemented_ed_instr<0x08>,
            /* 0x09 */ unimplemented_ed_instr<0x09>,
            /* 0x0A */ unimplemented_ed_instr<0x0A>,
            /* 0x0B */ unimplemented_ed_instr<0x0B>,
            /* 0x0C */ unimplemented_ed_instr<0x0C>,
            /* 0x0D */ unimplemented_ed_instr<0x0D>,
            /* 0x0E */ unimplemented_ed_instr<0x0E>,
            /* 0x0F */ unimplemented_ed_instr<0x0F>,
            /* 0x10 */ unimplemented_ed_instr<0x10>,
            /* 0x11 */ unimplemented_ed_instr<0x11>,
            /* 0x12 */ unimplemented_ed_instr<0x12>,
            /* 0x13 */ unimplemented_ed_instr<0x13>,
            /* 0x14 */ unimplemented_ed_instr<0x14>,
            /* 0x15 */ unimplemented_ed_instr<0x15>,
            /* 0x16 */ unimplemented_ed_instr<0x16>,
            /* 0x17 */ unimplemented_ed_instr<0x17>,
            /* 0x18 */ unimplemented_ed_instr<0x18>,
            /* 0x19 */ unimplemented_ed_instr<0x19>,
            /* 0x1A */ unimplemented_ed_instr<0x1A>,
            /* 0x1B */ unimplemented_ed_instr<0x1B>,
            /* 0x1C */ unimplemented_ed_instr<0x1C>,
            /* 0x1D */ unimplemented_ed_instr<0x1D>,
            /* 0x1E */ unimplemented_ed_instr<0x1E>,
            /* 0x1F */ unimplemented_ed_instr<0x1F>,
            /* 0x20 */ unimplemented_ed_instr<0x20>,
            /* 0x21 */ unimplemented_ed_instr<0x21>,
            /* 0x22 */ unimplemented_ed_instr<0x22>,
            /* 0x23 */ unimplemented_ed_instr<0x23>,
            /* 0x24 */ unimplemented_ed_instr<0x24>,
            /* 0x25 */ unimplemented_ed_instr<0x25>,
            /* 0x26 */ unimplemented_ed_instr<0x26>,
            /* 0x27 */ unimplemented_ed_instr<0x27>,
            /* 0x28 */ unimplemented_ed_instr<0x28>,
            /* 0x29 */ unimplemented_ed_instr<0x29>,
            /* 0x2A */ unimplemented_ed_instr<0x2A>,
            /* 0x2B */ unimplemented_ed_instr<0x2B>,
            /* 0x2C */ unimplemented_ed_instr<0x2C>,
            /* 0x2D */ unimplemented_ed_instr<0x2D>,
            /* 0x2E */ unimplemented_ed_instr<0x2E>,
            /* 0x2F */ unimplemented_ed_instr<0x2F>,
            /* 0x30 */ unimplemented_ed_instr<0x30>,
            /* 0x31 */ unimplemented_ed_instr<0x31>,
            /* 0x32 */ unimplemented_ed_instr<0x32>,
            /* 0x33 */ unimplemented_ed_instr<0x33>,
            /* 0x34 */ unimplemented_ed_instr<0x34>,
            /* 0x35 */ unimplemented_ed_instr<0x35>,
            /* 0x36 */ unimplemented_ed_instr<0x36>,
            /* 0x37 */ unimplemented_ed_instr<0x37>,
            /* 0x38 */ unimplemented_ed_instr<0x38>,
            /* 0x39 */ unimplemented_ed_instr<0x39>,
            /* 0x3A */ unimplemented_ed_instr<0x3A>,
            /* 0x3B */ unimplemented_ed_instr<0x3B>,
            /* 0x3C */ unimplemented_ed_instr<0x3C>,
            /* 0x3D */ unimplemented_ed_instr<0x3D>,
            /* 0x3E */ unimplemented_ed_instr<0x3E>,
            /* 0x3F */ unimplemented_ed_instr<0x3F>,
            /* 0x40 */ unimplemented_ed_instr<0x40>,
            /* 0x41 */ unimplemented_ed_instr<0x41>,
            /* 0x42 */ unimplemented_ed_instr<0x42>,
            /* 0x43 */ unimplemented_ed_instr<0x43>,
            /* 0x44 */ unimplemented_ed_instr<0x44>,
            /* 0x45 */ unimplemented_ed_instr<0x45>,
            /* 0x46 */ unimplemented_ed_instr<0x46>,
            /* 0x47 */ unimplemented_ed_instr<0x47>,
            /* 0x48 */ unimplemented_ed_instr<0x48>,
            /* 0x49 */ unimplemented_ed_instr<0x49>,
            /* 0x4A */ unimplemented_ed_instr<0x4A>,
            /* 0x4B */ unimplemented_ed_instr<0x4B>,
            /* 0x4C */ unimplemented_ed_instr<0x4C>,
            /* 0x4D */ unimplemented_ed_instr<0x4D>,
            /* 0x4E */ unimplemented_ed_instr<0x4E>,
            /* 0x4F */ unimplemented_ed_instr<0x4F>,
            /* 0x50 */ unimplemented_ed_instr<0x50>,
            /* 0x51 */ unimplemented_ed_instr<0x51>,
            /* 0x52 */ unimplemented_ed_instr<0x52>,
            /* 0x53 */ unimplemented_ed_instr<0x53>,
            /* 0x54 */ unimplemented_ed_instr<0x54>,
            /* 0x55 */ unimplemented_ed_instr<0x55>,
            /* 0x56 */ unimplemented_ed_instr<0x56>,
            /* 0x57 */ unimplemented_ed_instr<0x57>,
            /* 0x58 */ unimplemented_ed_instr<0x58>,
            /* 0x59 */ unimplemented_ed_instr<0x59>,
            /* 0x5A */ unimplemented_ed_instr<0x5A>,
            /* 0x5B */ unimplemented_ed_instr<0x5B>,
            /* 0x5C */ unimplemented_ed_instr<0x5C>,
            /* 0x5D */ unimplemented_ed_instr<0x5D>,
            /* 0x5E */ unimplemented_ed_instr<0x5E>,
            /* 0x5F */ unimplemented_ed_instr<0x5F>,
            /* 0x60 */ unimplemented_ed_instr<0x60>,
            /* 0x61 */ unimplemented_ed_instr<0x61>,
            /* 0x62 */ unimplemented_ed_instr<0x62>,
            /* 0x63 */ unimplemented_ed_instr<0x63>,
            /* 0x64 */ unimplemented_ed_instr<0x64>,
            /* 0x65 */ unimplemented_ed_instr<0x65>,
            /* 0x66 */ unimplemented_ed_instr<0x66>,
            /* 0x67 */ unimplemented_ed_instr<0x67>,
            /* 0x68 */ unimplemented_ed_instr<0x68>,
            /* 0x69 */ unimplemented_ed_instr<0x69>,
            /* 0x6A */ unimplemented_ed_instr<0x6A>,
            /* 0x6B */ unimplemented_ed_instr<0x6B>,
            /* 0x6C */ unimplemented_ed_instr<0x6C>,
            /* 0x6D */ unimplemented_ed_instr<0x6D>,
            /* 0x6E */ unimplemented_ed_instr<0x6E>,
            /* 0x6F */ unimplemented_ed_instr<0x6F>,
            /* 0x70 */ unimplemented_ed_instr<0x70>,
            /* 0x71 */ unimplemented_ed_instr<0x71>,
            /* 0x72 */ unimplemented_ed_instr<0x72>,
            /* 0x73 */ unimplemented_ed_instr<0x73>,
            /* 0x74 */ unimplemented_ed_instr<0x74>,
            /* 0x75 */ unimplemented_ed_instr<0x75>,
            /* 0x76 */ unimplemented_ed_instr<0x76>,
            /* 0x77 */ unimplemented_ed_instr<0x77>,
            /* 0x78 */ unimplemented_ed_instr<0x78>,
            /* 0x79 */ unimplemented_ed_instr<0x79>,
            /* 0x7A */ unimplemented_ed_instr<0x7A>,
            /* 0x7B */ unimplemented_ed_instr<0x7B>,
            /* 0x7C */ unimplemented_ed_instr<0x7C>,
            /* 0x7D */ unimplemented_ed_instr<0x7D>,
            /* 0x7E */ unimplemented_ed_instr<0x7E>,
            /* 0x7F */ unimplemented_ed_instr<0x7F>,
            /* 0x80 */ unimplemented_ed_instr<0x80>,
            /* 0x81 */ unimplemented_ed_instr<0x81>,
            /* 0x82 */ unimplemented_ed_instr<0x82>,
            /* 0x83 */ unimplemented_ed_instr<0x83>,
            /* 0x84 */ unimplemented_ed_instr<0x84>,
            /* 0x85 */ unimplemented_ed_instr<0x85>,
            /* 0x86 */ unimplemented_ed_instr<0x86>,
            /* 0x87 */ unimplemented_ed_instr<0x87>,
            /* 0x88 */ unimplemented_ed_instr<0x88>,
            /* 0x89 */ unimplemented_ed_instr<0x89>,
            /* 0x8A */ unimplemented_ed_instr<0x8A>,
            /* 0x8B */ unimplemented_ed_instr<0x8B>,
            /* 0x8C */ unimplemented_ed_instr<0x8C>,
            /* 0x8D */ unimplemented_ed_instr<0x8D>,
            /* 0x8E */ unimplemented_ed_instr<0x8E>,
            /* 0x8F */ unimplemented_ed_instr<0x8F>,
            /* 0x90 */ unimplemented_ed_instr<0x90>,
            /* 0x91 */ unimplemented_ed_instr<0x91>,
            /* 0x92 */ unimplemented_ed_instr<0x92>,
            /* 0x93 */ unimplemented_ed_instr<0x93>,
            /* 0x94 */ unimplemented_ed_instr<0x94>,
            /* 0x95 */ unimplemented_ed_instr<0x95>,
            /* 0x96 */ unimplemented_ed_instr<0x96>,
            /* 0x97 */ unimplemented_ed_instr<0x97>,
            /* 0x98 */ unimplemented_ed_instr<0x98>,
            /* 0x99 */ unimplemented_ed_instr<0x99>,
            /* 0x9A */ unimplemented_ed_instr<0x9A>,
            /* 0x9B */ unimplemented_ed_instr<0x9B>,
            /* 0x9C */ unimplemented_ed_instr<0x9C>,
            /* 0x9D */ unimplemented_ed_instr<0x9D>,
            /* 0x9E */ unimplemented_ed_instr<0x9E>,
            /* 0x9F */ unimplemented_ed_instr<0x9F>,
            /* 0xA0 */ unimplemented_ed_instr<0xA0>,
            /* 0xA1 */ unimplemented_ed_instr<0xA1>,
            /* 0xA2 */ unimplemented_ed_instr<0xA2>,
            /* 0xA3 */ unimplemented_ed_instr<0xA3>,
            /* 0xA4 */ unimplemented_ed_instr<0xA4>,
            /* 0xA5 */ unimplemented_ed_instr<0xA5>,
            /* 0xA6 */ unimplemented_ed_instr<0xA6>,
            /* 0xA7 */ unimplemented_ed_instr<0xA7>,
            /* 0xA8 */ unimplemented_ed_instr<0xA8>,
            /* 0xA9 */ unimplemented_ed_instr<0xA9>,
            /* 0xAA */ unimplemented_ed_instr<0xAA>,
            /* 0xAB */ unimplemented_ed_instr<0xAB>,
            /* 0xAC */ unimplemented_ed_instr<0xAC>,
            /* 0xAD */ unimplemented_ed_instr<0xAD>,
            /* 0xAE */ unimplemented_ed_instr<0xAE>,
            /* 0xAF */ unimplemented_ed_instr<0xAF>,
            /* 0xB0 */ unimplemented_ed_instr<0xB0>,
            /* 0xB1 */ unimplemented_ed_instr<0xB1>,
            /* 0xB2 */ unimplemented_ed_instr<0xB2>,
            /* 0xB3 */ unimplemented_ed_instr<0xB3>,
            /* 0xB4 */ unimplemented_ed_instr<0xB4>,
            /* 0xB5 */ unimplemented_ed_instr<0xB5>,
            /* 0xB6 */ unimplemented_ed_instr<0xB6>,
            /* 0xB7 */ unimplemented_ed_instr<0xB7>,
            /* 0xB8 */ unimplemented_ed_instr<0xB8>,
            /* 0xB9 */ unimplemented_ed_instr<0xB9>,
            /* 0xBA */ unimplemented_ed_instr<0xBA>,
            /* 0xBB */ unimplemented_ed_instr<0xBB>,
            /* 0xBC */ unimplemented_ed_instr<0xBC>,
            /* 0xBD */ unimplemented_ed_instr<0xBD>,
            /* 0xBE */ unimplemented_ed_instr<0xBE>,
            /* 0xBF */ unimplemented_ed_instr<0xBF>,
    };
}