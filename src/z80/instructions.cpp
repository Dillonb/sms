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
        Z, // Z flag is set
        NZ, // Z flag is reset
        C, // C flag is set
        NC, // C flag is reset
        M, // S flag is set
        P, // Set flag is reset
        PE, // P/V flag is set
        PO, // P/V flag is reset
    };
    enum class AddressingMode {
        Immediate,
        Indirect,
        HL,
        BC,
        IX,
        IXPlus,
        IY,
        IYPlus
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
            case Register::IX:
                return z80.ix;
            case Register::IY:
                return z80.iy;
            case Register::I:
                return z80.i;
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
                break;
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
                break;
            case Register::IX:
                z80.ix = value;
                break;
            case Register::IY:
                z80.iy = value;
                break;
            case Register::I:
                z80.i = value;
                break;
        }
    }

    template <Register a, Register b, typename aT = typename reg_type<a>::type, typename bT = typename reg_type<b>::type>
    void swap_registers() {
        static_assert(sizeof(aT) == sizeof(bT), "Types of swapped registers must be the same.");
        aT temp = get_register<a>();
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
            case Condition::C:
                return z80.f.c;
            case Condition::NC:
                return !z80.f.c;
            case Condition::M:
                return z80.f.s;
            case Condition::P:
                return !z80.f.s;
            case Condition::PE:
                return z80.f.p_v;
            case Condition::PO:
                return !z80.f.p_v;
        }
    }

    template <AddressingMode addressingMode>
    u16 get_address() {
        switch (addressingMode) {
            case AddressingMode::Indirect:
                return read_16_pc();
            case AddressingMode::HL:
                return z80.hl.raw;
            case AddressingMode::BC:
                return z80.bc.raw;
            case AddressingMode::IX:
                return z80.ix;
            case AddressingMode::IY:
                return z80.iy;
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
                if (std::is_same_v<T, u16>) {
                    return read_16(get_register<Register::HL>());
                } else if (std::is_same_v<T, u8>) {
                    return z80.read_byte(get_register<Register::HL>());
                }
            case AddressingMode::BC:
                return read_16(get_register<Register::BC>());
            case AddressingMode::IXPlus:
                if (std::is_same_v<T, u16>) {
                    return read_16(get_register<Register::IX>() + (s8)z80.read_byte(z80.pc++));
                } else if (std::is_same_v<T, u8>) {
                    return z80.read_byte(get_register<Register::IX>() + (s8)z80.read_byte(z80.pc++));
                }
            case AddressingMode::IYPlus:
                if (std::is_same_v<T, u16>) {
                    return read_16(get_register<Register::IY>() + (s8)z80.read_byte(z80.pc++));
                } else if (std::is_same_v<T, u8>) {
                    return z80.read_byte(get_register<Register::IY>() + (s8)z80.read_byte(z80.pc++));
                }
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

    template <u8 opc>
    int unimplemented_dd_instr() {
        printf("Unimplemented DD instruction %02X!\n", opc);
        exit(1);
    }

    template <u8 opc>
    int unimplemented_fd_instr() {
        printf("Unimplemented FD instruction %02X!\n", opc);
        exit(1);
    }

    template <Condition c, AddressingMode addressingMode>
    int instr_jp() {
        u16 address = get_address<addressingMode>();

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

    template <Condition c>
    int instr_jr() {
        if (check_condition<c>()) {
            s8 offset = z80.read_byte(z80.pc++);
            z80.pc += offset;
            return 12;
        }
        return 7;
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
            case sizeof(u8): {
                u8 m = get_register<reg>();
                u8 r = m + 1;
                z80.f.n = false;
                z80.f.p_v = m == 0x7F;
                z80.f.h = (m & 0xF) == 0xF;
                z80.f.b3 = (r >> 3) & 1;
                z80.f.b5 = (r >> 5) & 1;
                z80.f.z = r == 0;
                z80.f.s = ((s8)r) < 0;
                set_register<reg>(r);
                return 4;
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
        logfatal("Should not reach here, or time to implement 8 bit adds");
    }

    template <Register src, typename T = typename reg_type<src>::type>
    int instr_and() {
        static_assert(std::is_same_v<T, u8>, "Only defined for 8 bit regs");
        z80.a = z80.a & get_register<src>();
        z80.f.s = ((s8)z80.a) < 0;
        z80.f.z = z80.a == 0;
        z80.f.h = true;
        z80.f.p_v = parity(z80.a);
        z80.f.n = false;
        z80.f.c = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
    }

    template <AddressingMode src>
    int instr_and() {
        z80.a = z80.a & read_value<src, u8>();
        z80.f.s = ((s8)z80.a) < 0;
        z80.f.z = z80.a == 0;
        z80.f.h = true;
        z80.f.p_v = parity(z80.a);
        z80.f.n = false;
        z80.f.c = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
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
        z80.f.b3 = (s >> 3) & 1;
        z80.f.b5 = (s >> 5) & 1;

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

    int instr_in() {
        z80.a = z80.port_in(z80.read_byte(z80.pc++));
        return 4;
    }

    int instr_out() {
        z80.port_out(z80.read_byte(z80.pc++), z80.a);
        return 4;
    }

    int instr_dd() {
        return dd_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_ed() {
        return ed_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_fd() {
        return fd_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_nop() {
        return 4;
    }

    int instr_ldi() {
        u8 value = z80.read_byte(z80.hl.raw);
        z80.write_byte(z80.de.raw, value);
        z80.hl.raw++;
        z80.de.raw++;
        z80.bc.raw--;

        z80.f.n = false;
        z80.f.h = false;
        z80.f.p_v = z80.bc.raw > 0;

        u8 r = value + z80.a;

        z80.f.b3 = ((r >> 3) & 1) == 1;
        z80.f.b5 = ((r >> 1) & 1) == 1;

        return 16;
    }

    int instr_ldir() {
        instr_ldi();

        if (z80.bc.raw) {
            z80.pc -= 2; // Repeat the instruction until BC is zero
            return 21;
        }
        return 16;
    }

    int instr_rlca() {
        z80.a = std::rotl(z80.a, 1);
        z80.f.c = z80.a & 1;
        z80.f.n = false;
        z80.f.h = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
    }

    int instr_rrca() {
        z80.f.c = z80.a & 1;
        z80.a = std::rotr(z80.a, 1);
        z80.f.n = false;
        z80.f.h = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
    }

    int instr_djnz() {
        set_register<Register::B>(get_register<Register::B>() - 1);
        s8 offset = z80.read_byte(z80.pc++);
        if (get_register<Register::B>() != 0) {
            z80.pc += offset;
            return 13;
        }
        return 8;
    }

    const instruction instructions[0x100] = {
            /* 00 */ instr_nop,
            /* 01 */ instr_ld<Register::BC, AddressingMode::Immediate>,
            /* 02 */ instr_ld<AddressingMode::BC, Register::A>,
            /* 03 */ instr_inc<Register::BC>,
            /* 04 */ instr_inc<Register::B>,
            /* 05 */ instr_dec<Register::B>,
            /* 06 */ instr_ld<Register::B, AddressingMode::Immediate>,
            /* 07 */ instr_rlca,
            /* 08 */ instr_ex_af,
            /* 09 */ instr_add<Register::HL, Register::BC>,
            /* 0A */ instr_ld<Register::A, AddressingMode::BC>,
            /* 0B */ instr_dec<Register::BC>,
            /* 0C */ instr_inc<Register::C>,
            /* 0D */ instr_dec<Register::C>,
            /* 0E */ instr_ld<Register::C, AddressingMode::Immediate>,
            /* 0F */ instr_rrca,
            /* 10 */ instr_djnz,
            /* 11 */ instr_ld<Register::DE, AddressingMode::Immediate>,
            /* 12 */ unimplemented_instr<0x12>,
            /* 13 */ instr_inc<Register::DE>,
            /* 14 */ unimplemented_instr<0x14>,
            /* 15 */ unimplemented_instr<0x15>,
            /* 16 */ instr_ld<Register::D, AddressingMode::Immediate>,
            /* 17 */ unimplemented_instr<0x17>,
            /* 18 */ instr_jr<Condition::Always>,
            /* 19 */ instr_add<Register::HL, Register::DE>,
            /* 1A */ unimplemented_instr<0x1A>,
            /* 1B */ unimplemented_instr<0x1B>,
            /* 1C */ unimplemented_instr<0x1C>,
            /* 1D */ instr_dec<Register::E>,
            /* 1E */ unimplemented_instr<0x1E>,
            /* 1F */ unimplemented_instr<0x1F>,
            /* 20 */ instr_jr<Condition::NZ>,
            /* 21 */ instr_ld<Register::HL, AddressingMode::Immediate>,
            /* 22 */ unimplemented_instr<0x22>,
            /* 23 */ instr_inc<Register::HL>,
            /* 24 */ unimplemented_instr<0x24>,
            /* 25 */ unimplemented_instr<0x25>,
            /* 26 */ unimplemented_instr<0x26>,
            /* 27 */ unimplemented_instr<0x27>,
            /* 28 */ instr_jr<Condition::Z>,
            /* 29 */ unimplemented_instr<0x29>,
            /* 2A */ instr_ld<Register::HL, AddressingMode::Indirect>,
            /* 2B */ instr_dec<Register::HL>,
            /* 2C */ unimplemented_instr<0x2C>,
            /* 2D */ unimplemented_instr<0x2D>,
            /* 2E */ unimplemented_instr<0x2E>,
            /* 2F */ unimplemented_instr<0x2F>,
            /* 30 */ instr_jr<Condition::NC>,
            /* 31 */ instr_ld<Register::SP, AddressingMode::Immediate>,
            /* 32 */ instr_ld<AddressingMode::Indirect, Register::A>,
            /* 33 */ unimplemented_instr<0x33>,
            /* 34 */ unimplemented_instr<0x34>,
            /* 35 */ unimplemented_instr<0x35>,
            /* 36 */ instr_ld<AddressingMode::HL, AddressingMode::Immediate>,
            /* 37 */ unimplemented_instr<0x37>,
            /* 38 */ instr_jr<Condition::C>,
            /* 39 */ unimplemented_instr<0x39>,
            /* 3A */ instr_ld<Register::A, AddressingMode::Indirect>,
            /* 3B */ unimplemented_instr<0x3B>,
            /* 3C */ instr_inc<Register::A>,
            /* 3D */ unimplemented_instr<0x3D>,
            /* 3E */ instr_ld<Register::A, AddressingMode::Immediate>,
            /* 3F */ unimplemented_instr<0x3F>,
            /* 40 */ instr_ld<Register::B, Register::B>,
            /* 41 */ unimplemented_instr<0x41>,
            /* 42 */ unimplemented_instr<0x42>,
            /* 43 */ unimplemented_instr<0x43>,
            /* 44 */ unimplemented_instr<0x44>,
            /* 45 */ unimplemented_instr<0x45>,
            /* 46 */ unimplemented_instr<0x46>,
            /* 47 */ unimplemented_instr<0x47>,
            /* 48 */ unimplemented_instr<0x48>,
            /* 49 */ unimplemented_instr<0x49>,
            /* 4A */ unimplemented_instr<0x4A>,
            /* 4B */ unimplemented_instr<0x4B>,
            /* 4C */ unimplemented_instr<0x4C>,
            /* 4D */ unimplemented_instr<0x4D>,
            /* 4E */ unimplemented_instr<0x4E>,
            /* 4F */ unimplemented_instr<0x4F>,
            /* 50 */ unimplemented_instr<0x50>,
            /* 51 */ unimplemented_instr<0x51>,
            /* 52 */ unimplemented_instr<0x52>,
            /* 53 */ unimplemented_instr<0x53>,
            /* 54 */ instr_ld<Register::D, Register::H>,
            /* 55 */ unimplemented_instr<0x55>,
            /* 56 */ unimplemented_instr<0x56>,
            /* 57 */ unimplemented_instr<0x57>,
            /* 58 */ unimplemented_instr<0x58>,
            /* 59 */ unimplemented_instr<0x59>,
            /* 5A */ unimplemented_instr<0x5A>,
            /* 5B */ unimplemented_instr<0x5B>,
            /* 5C */ unimplemented_instr<0x5C>,
            /* 5D */ instr_ld<Register::E, Register::L>,
            /* 5E */ instr_ld<Register::E, AddressingMode::HL>,
            /* 5F */ unimplemented_instr<0x5F>,
            /* 60 */ unimplemented_instr<0x60>,
            /* 61 */ unimplemented_instr<0x61>,
            /* 62 */ instr_ld<Register::H, Register::D>,
            /* 63 */ unimplemented_instr<0x63>,
            /* 64 */ unimplemented_instr<0x64>,
            /* 65 */ unimplemented_instr<0x65>,
            /* 66 */ instr_ld<Register::H, AddressingMode::HL>,
            /* 67 */ unimplemented_instr<0x67>,
            /* 68 */ unimplemented_instr<0x68>,
            /* 69 */ unimplemented_instr<0x69>,
            /* 6A */ unimplemented_instr<0x6A>,
            /* 6B */ unimplemented_instr<0x6B>,
            /* 6C */ unimplemented_instr<0x6C>,
            /* 6D */ unimplemented_instr<0x6D>,
            /* 6E */ unimplemented_instr<0x6E>,
            /* 6F */ instr_ld<Register::L, Register::A>,
            /* 70 */ instr_ld<AddressingMode::HL, Register::B>,
            /* 71 */ instr_ld<AddressingMode::HL, Register::C>,
            /* 72 */ instr_ld<AddressingMode::HL, Register::D>,
            /* 73 */ instr_ld<AddressingMode::HL, Register::E>,
            /* 74 */ instr_ld<AddressingMode::HL, Register::H>,
            /* 75 */ instr_ld<AddressingMode::HL, Register::L>,
            /* 76 */ unimplemented_instr<0x76>,
            /* 77 */ instr_ld<AddressingMode::HL, Register::A>,
            /* 78 */ instr_ld<Register::A, Register::B>,
            /* 79 */ instr_ld<Register::A, Register::C>,
            /* 7A */ instr_ld<Register::A, Register::D>,
            /* 7B */ instr_ld<Register::A, Register::E>,
            /* 7C */ instr_ld<Register::A, Register::H>,
            /* 7D */ instr_ld<Register::A, Register::L>,
            /* 7E */ instr_ld<Register::A, AddressingMode::HL>,
            /* 7F */ instr_ld<Register::A, Register::A>,
            /* 80 */ unimplemented_instr<0x80>,
            /* 81 */ unimplemented_instr<0x81>,
            /* 82 */ unimplemented_instr<0x82>,
            /* 83 */ unimplemented_instr<0x83>,
            /* 84 */ unimplemented_instr<0x84>,
            /* 85 */ unimplemented_instr<0x85>,
            /* 86 */ unimplemented_instr<0x86>,
            /* 87 */ unimplemented_instr<0x87>,
            /* 88 */ unimplemented_instr<0x88>,
            /* 89 */ unimplemented_instr<0x89>,
            /* 8A */ unimplemented_instr<0x8A>,
            /* 8B */ unimplemented_instr<0x8B>,
            /* 8C */ unimplemented_instr<0x8C>,
            /* 8D */ unimplemented_instr<0x8D>,
            /* 8E */ unimplemented_instr<0x8E>,
            /* 8F */ unimplemented_instr<0x8F>,
            /* 90 */ unimplemented_instr<0x90>,
            /* 91 */ unimplemented_instr<0x91>,
            /* 92 */ unimplemented_instr<0x92>,
            /* 93 */ unimplemented_instr<0x93>,
            /* 94 */ unimplemented_instr<0x94>,
            /* 95 */ unimplemented_instr<0x95>,
            /* 96 */ unimplemented_instr<0x96>,
            /* 97 */ unimplemented_instr<0x97>,
            /* 98 */ unimplemented_instr<0x98>,
            /* 99 */ unimplemented_instr<0x99>,
            /* 9A */ unimplemented_instr<0x9A>,
            /* 9B */ unimplemented_instr<0x9B>,
            /* 9C */ unimplemented_instr<0x9C>,
            /* 9D */ unimplemented_instr<0x9D>,
            /* 9E */ unimplemented_instr<0x9E>,
            /* 9F */ unimplemented_instr<0x9F>,
            /* A0 */ unimplemented_instr<0xA0>,
            /* A1 */ instr_and<Register::C>,
            /* A2 */ unimplemented_instr<0xA2>,
            /* A3 */ unimplemented_instr<0xA3>,
            /* A4 */ unimplemented_instr<0xA4>,
            /* A5 */ unimplemented_instr<0xA5>,
            /* A6 */ unimplemented_instr<0xA6>,
            /* A7 */ unimplemented_instr<0xA7>,
            /* A8 */ unimplemented_instr<0xA8>,
            /* A9 */ unimplemented_instr<0xA9>,
            /* AA */ unimplemented_instr<0xAA>,
            /* AB */ unimplemented_instr<0xAB>,
            /* AC */ unimplemented_instr<0xAC>,
            /* AD */ unimplemented_instr<0xAD>,
            /* AE */ unimplemented_instr<0xAE>,
            /* AF */ unimplemented_instr<0xAF>,
            /* B0 */ unimplemented_instr<0xB0>,
            /* B1 */ unimplemented_instr<0xB1>,
            /* B2 */ unimplemented_instr<0xB2>,
            /* B3 */ unimplemented_instr<0xB3>,
            /* B4 */ unimplemented_instr<0xB4>,
            /* B5 */ unimplemented_instr<0xB5>,
            /* B6 */ instr_or<AddressingMode::HL>,
            /* B7 */ unimplemented_instr<0xB7>,
            /* B8 */ unimplemented_instr<0xB8>,
            /* B9 */ unimplemented_instr<0xB9>,
            /* BA */ unimplemented_instr<0xBA>,
            /* BB */ unimplemented_instr<0xBB>,
            /* BC */ unimplemented_instr<0xBC>,
            /* BD */ unimplemented_instr<0xBD>,
            /* BE */ unimplemented_instr<0xBE>,
            /* BF */ unimplemented_instr<0xBF>,
            /* C0 */ instr_ret<Condition::NZ>,
            /* C1 */ instr_pop<Register::BC>,
            /* C2 */ instr_jp<Condition::NZ, AddressingMode::Indirect>,
            /* C3 */ instr_jp<Condition::Always, AddressingMode::Indirect>,
            /* C4 */ instr_call<Condition::NZ>,
            /* C5 */ instr_push<Register::BC>,
            /* C6 */ unimplemented_instr<0xC6>,
            /* C7 */ unimplemented_instr<0xC7>,
            /* C8 */ instr_ret<Condition::Z>,
            /* C9 */ instr_ret<Condition::Always>,
            /* CA */ instr_jp<Condition::Z, AddressingMode::Indirect>,
            /* CB */ unimplemented_instr<0xCB>,
            /* CC */ instr_call<Condition::Z>,
            /* CD */ instr_call<Condition::Always>,
            /* CE */ unimplemented_instr<0xCE>,
            /* CF */ unimplemented_instr<0xCF>,
            /* D0 */ instr_ret<Condition::NC>,
            /* D1 */ instr_pop<Register::DE>,
            /* D2 */ instr_jp<Condition::NC, AddressingMode::Indirect>,
            /* D3 */ instr_out,
            /* D4 */ instr_call<Condition::NC>,
            /* D5 */ instr_push<Register::DE>,
            /* D6 */ unimplemented_instr<0xD6>,
            /* D7 */ unimplemented_instr<0xD7>,
            /* D8 */ instr_ret<Condition::C>,
            /* D9 */ instr_exx,
            /* DA */ instr_jp<Condition::C, AddressingMode::Indirect>,
            /* DB */ instr_in, //unimplemented_instr<0xDB>,
            /* DC */ instr_call<Condition::C>,
            /* DD */ instr_dd,
            /* DE */ unimplemented_instr<0xDE>,
            /* DF */ unimplemented_instr<0xDF>,
            /* E0 */ instr_ret<Condition::PO>,
            /* E1 */ instr_pop<Register::HL>,
            /* E2 */ instr_jp<Condition::PO, AddressingMode::Indirect>,
            /* E3 */ unimplemented_instr<0xE3>,
            /* E4 */ instr_call<Condition::PO>,
            /* E5 */ instr_push<Register::HL>,
            /* E6 */ instr_and<AddressingMode::Immediate>,
            /* E7 */ unimplemented_instr<0xE7>,
            /* E8 */ instr_ret<Condition::PE>,
            /* E9 */ instr_jp<Condition::Always, AddressingMode::HL>,
            /* EA */ instr_jp<Condition::PE, AddressingMode::Indirect>,
            /* EB */ instr_ex_de_hl,
            /* EC */ instr_call<Condition::PE>,
            /* ED */ instr_ed,
            /* EE */ unimplemented_instr<0xEE>,
            /* EF */ unimplemented_instr<0xEF>,
            /* F0 */ instr_ret<Condition::P>,
            /* F1 */ instr_pop<Register::AF>,
            /* F2 */ instr_jp<Condition::P, AddressingMode::Indirect>,
            /* F3 */ unimplemented_instr<0xF3>,
            /* F4 */ instr_call<Condition::P>,
            /* F5 */ instr_push<Register::AF>,
            /* F6 */ unimplemented_instr<0xF6>,
            /* F7 */ unimplemented_instr<0xF7>,
            /* F8 */ instr_ret<Condition::M>,
            /* F9 */ instr_ld<Register::SP, Register::HL>,
            /* FA */ instr_jp<Condition::M, AddressingMode::Indirect>,
            /* FB */ unimplemented_instr<0xFB>,
            /* FC */ instr_call<Condition::M>,
            /* FD */ instr_fd,
            /* FE */ instr_cp<AddressingMode::Immediate>,
            /* FF */ unimplemented_instr<0xFF>,
    };

    const instruction dd_instructions[0x100] = {
            /* DD 00 */ unimplemented_dd_instr<0x00>,
            /* DD 01 */ unimplemented_dd_instr<0x01>,
            /* DD 02 */ unimplemented_dd_instr<0x02>,
            /* DD 03 */ unimplemented_dd_instr<0x03>,
            /* DD 04 */ unimplemented_dd_instr<0x04>,
            /* DD 05 */ unimplemented_dd_instr<0x05>,
            /* DD 06 */ unimplemented_dd_instr<0x06>,
            /* DD 07 */ unimplemented_dd_instr<0x07>,
            /* DD 08 */ unimplemented_dd_instr<0x08>,
            /* DD 09 */ unimplemented_dd_instr<0x09>,
            /* DD 0A */ unimplemented_dd_instr<0x0A>,
            /* DD 0B */ unimplemented_dd_instr<0x0B>,
            /* DD 0C */ unimplemented_dd_instr<0x0C>,
            /* DD 0D */ unimplemented_dd_instr<0x0D>,
            /* DD 0E */ unimplemented_dd_instr<0x0E>,
            /* DD 0F */ unimplemented_dd_instr<0x0F>,
            /* DD 10 */ unimplemented_dd_instr<0x10>,
            /* DD 11 */ unimplemented_dd_instr<0x11>,
            /* DD 12 */ unimplemented_dd_instr<0x12>,
            /* DD 13 */ unimplemented_dd_instr<0x13>,
            /* DD 14 */ unimplemented_dd_instr<0x14>,
            /* DD 15 */ unimplemented_dd_instr<0x15>,
            /* DD 16 */ unimplemented_dd_instr<0x16>,
            /* DD 17 */ unimplemented_dd_instr<0x17>,
            /* DD 18 */ unimplemented_dd_instr<0x18>,
            /* DD 19 */ unimplemented_dd_instr<0x19>,
            /* DD 1A */ unimplemented_dd_instr<0x1A>,
            /* DD 1B */ unimplemented_dd_instr<0x1B>,
            /* DD 1C */ unimplemented_dd_instr<0x1C>,
            /* DD 1D */ unimplemented_dd_instr<0x1D>,
            /* DD 1E */ unimplemented_dd_instr<0x1E>,
            /* DD 1F */ unimplemented_dd_instr<0x1F>,
            /* DD 20 */ unimplemented_dd_instr<0x20>,
            /* DD 21 */ instr_ld<Register::IX, AddressingMode::Immediate>,
            /* DD 22 */ unimplemented_dd_instr<0x22>,
            /* DD 23 */ instr_inc<Register::IX>,
            /* DD 24 */ unimplemented_dd_instr<0x24>,
            /* DD 25 */ unimplemented_dd_instr<0x25>,
            /* DD 26 */ unimplemented_dd_instr<0x26>,
            /* DD 27 */ unimplemented_dd_instr<0x27>,
            /* DD 28 */ unimplemented_dd_instr<0x28>,
            /* DD 29 */ unimplemented_dd_instr<0x29>,
            /* DD 2A */ unimplemented_dd_instr<0x2A>,
            /* DD 2B */ unimplemented_dd_instr<0x2B>,
            /* DD 2C */ unimplemented_dd_instr<0x2C>,
            /* DD 2D */ unimplemented_dd_instr<0x2D>,
            /* DD 2E */ unimplemented_dd_instr<0x2E>,
            /* DD 2F */ unimplemented_dd_instr<0x2F>,
            /* DD 30 */ unimplemented_dd_instr<0x30>,
            /* DD 31 */ unimplemented_dd_instr<0x31>,
            /* DD 32 */ unimplemented_dd_instr<0x32>,
            /* DD 33 */ unimplemented_dd_instr<0x33>,
            /* DD 34 */ unimplemented_dd_instr<0x34>,
            /* DD 35 */ unimplemented_dd_instr<0x35>,
            /* DD 36 */ unimplemented_dd_instr<0x36>,
            /* DD 37 */ unimplemented_dd_instr<0x37>,
            /* DD 38 */ unimplemented_dd_instr<0x38>,
            /* DD 39 */ unimplemented_dd_instr<0x39>,
            /* DD 3A */ unimplemented_dd_instr<0x3A>,
            /* DD 3B */ unimplemented_dd_instr<0x3B>,
            /* DD 3C */ unimplemented_dd_instr<0x3C>,
            /* DD 3D */ unimplemented_dd_instr<0x3D>,
            /* DD 3E */ unimplemented_dd_instr<0x3E>,
            /* DD 3F */ unimplemented_dd_instr<0x3F>,
            /* DD 40 */ unimplemented_dd_instr<0x40>,
            /* DD 41 */ unimplemented_dd_instr<0x41>,
            /* DD 42 */ unimplemented_dd_instr<0x42>,
            /* DD 43 */ unimplemented_dd_instr<0x43>,
            /* DD 44 */ unimplemented_dd_instr<0x44>,
            /* DD 45 */ unimplemented_dd_instr<0x45>,
            /* DD 46 */ unimplemented_dd_instr<0x46>,
            /* DD 47 */ unimplemented_dd_instr<0x47>,
            /* DD 48 */ unimplemented_dd_instr<0x48>,
            /* DD 49 */ unimplemented_dd_instr<0x49>,
            /* DD 4A */ unimplemented_dd_instr<0x4A>,
            /* DD 4B */ unimplemented_dd_instr<0x4B>,
            /* DD 4C */ unimplemented_dd_instr<0x4C>,
            /* DD 4D */ unimplemented_dd_instr<0x4D>,
            /* DD 4E */ unimplemented_dd_instr<0x4E>,
            /* DD 4F */ unimplemented_dd_instr<0x4F>,
            /* DD 50 */ unimplemented_dd_instr<0x50>,
            /* DD 51 */ unimplemented_dd_instr<0x51>,
            /* DD 52 */ unimplemented_dd_instr<0x52>,
            /* DD 53 */ unimplemented_dd_instr<0x53>,
            /* DD 54 */ unimplemented_dd_instr<0x54>,
            /* DD 55 */ unimplemented_dd_instr<0x55>,
            /* DD 56 */ unimplemented_dd_instr<0x56>,
            /* DD 57 */ unimplemented_dd_instr<0x57>,
            /* DD 58 */ unimplemented_dd_instr<0x58>,
            /* DD 59 */ unimplemented_dd_instr<0x59>,
            /* DD 5A */ unimplemented_dd_instr<0x5A>,
            /* DD 5B */ unimplemented_dd_instr<0x5B>,
            /* DD 5C */ unimplemented_dd_instr<0x5C>,
            /* DD 5D */ unimplemented_dd_instr<0x5D>,
            /* DD 5E */ unimplemented_dd_instr<0x5E>,
            /* DD 5F */ unimplemented_dd_instr<0x5F>,
            /* DD 60 */ unimplemented_dd_instr<0x60>,
            /* DD 61 */ unimplemented_dd_instr<0x61>,
            /* DD 62 */ unimplemented_dd_instr<0x62>,
            /* DD 63 */ unimplemented_dd_instr<0x63>,
            /* DD 64 */ unimplemented_dd_instr<0x64>,
            /* DD 65 */ unimplemented_dd_instr<0x65>,
            /* DD 66 */ unimplemented_dd_instr<0x66>,
            /* DD 67 */ unimplemented_dd_instr<0x67>,
            /* DD 68 */ unimplemented_dd_instr<0x68>,
            /* DD 69 */ unimplemented_dd_instr<0x69>,
            /* DD 6A */ unimplemented_dd_instr<0x6A>,
            /* DD 6B */ unimplemented_dd_instr<0x6B>,
            /* DD 6C */ unimplemented_dd_instr<0x6C>,
            /* DD 6D */ unimplemented_dd_instr<0x6D>,
            /* DD 6E */ unimplemented_dd_instr<0x6E>,
            /* DD 6F */ unimplemented_dd_instr<0x6F>,
            /* DD 70 */ unimplemented_dd_instr<0x70>,
            /* DD 71 */ unimplemented_dd_instr<0x71>,
            /* DD 72 */ unimplemented_dd_instr<0x72>,
            /* DD 73 */ unimplemented_dd_instr<0x73>,
            /* DD 74 */ unimplemented_dd_instr<0x74>,
            /* DD 75 */ unimplemented_dd_instr<0x75>,
            /* DD 76 */ unimplemented_dd_instr<0x76>,
            /* DD 77 */ unimplemented_dd_instr<0x77>,
            /* DD 78 */ unimplemented_dd_instr<0x78>,
            /* DD 79 */ unimplemented_dd_instr<0x79>,
            /* DD 7A */ unimplemented_dd_instr<0x7A>,
            /* DD 7B */ unimplemented_dd_instr<0x7B>,
            /* DD 7C */ unimplemented_dd_instr<0x7C>,
            /* DD 7D */ unimplemented_dd_instr<0x7D>,
            /* DD 7E */ instr_ld<Register::A, AddressingMode::IXPlus>,
            /* DD 7F */ unimplemented_dd_instr<0x7F>,
            /* DD 80 */ unimplemented_dd_instr<0x80>,
            /* DD 81 */ unimplemented_dd_instr<0x81>,
            /* DD 82 */ unimplemented_dd_instr<0x82>,
            /* DD 83 */ unimplemented_dd_instr<0x83>,
            /* DD 84 */ unimplemented_dd_instr<0x84>,
            /* DD 85 */ unimplemented_dd_instr<0x85>,
            /* DD 86 */ unimplemented_dd_instr<0x86>,
            /* DD 87 */ unimplemented_dd_instr<0x87>,
            /* DD 88 */ unimplemented_dd_instr<0x88>,
            /* DD 89 */ unimplemented_dd_instr<0x89>,
            /* DD 8A */ unimplemented_dd_instr<0x8A>,
            /* DD 8B */ unimplemented_dd_instr<0x8B>,
            /* DD 8C */ unimplemented_dd_instr<0x8C>,
            /* DD 8D */ unimplemented_dd_instr<0x8D>,
            /* DD 8E */ unimplemented_dd_instr<0x8E>,
            /* DD 8F */ unimplemented_dd_instr<0x8F>,
            /* DD 90 */ unimplemented_dd_instr<0x90>,
            /* DD 91 */ unimplemented_dd_instr<0x91>,
            /* DD 92 */ unimplemented_dd_instr<0x92>,
            /* DD 93 */ unimplemented_dd_instr<0x93>,
            /* DD 94 */ unimplemented_dd_instr<0x94>,
            /* DD 95 */ unimplemented_dd_instr<0x95>,
            /* DD 96 */ unimplemented_dd_instr<0x96>,
            /* DD 97 */ unimplemented_dd_instr<0x97>,
            /* DD 98 */ unimplemented_dd_instr<0x98>,
            /* DD 99 */ unimplemented_dd_instr<0x99>,
            /* DD 9A */ unimplemented_dd_instr<0x9A>,
            /* DD 9B */ unimplemented_dd_instr<0x9B>,
            /* DD 9C */ unimplemented_dd_instr<0x9C>,
            /* DD 9D */ unimplemented_dd_instr<0x9D>,
            /* DD 9E */ unimplemented_dd_instr<0x9E>,
            /* DD 9F */ unimplemented_dd_instr<0x9F>,
            /* DD A0 */ unimplemented_dd_instr<0xA0>,
            /* DD A1 */ unimplemented_dd_instr<0xA1>,
            /* DD A2 */ unimplemented_dd_instr<0xA2>,
            /* DD A3 */ unimplemented_dd_instr<0xA3>,
            /* DD A4 */ unimplemented_dd_instr<0xA4>,
            /* DD A5 */ unimplemented_dd_instr<0xA5>,
            /* DD A6 */ unimplemented_dd_instr<0xA6>,
            /* DD A7 */ unimplemented_dd_instr<0xA7>,
            /* DD A8 */ unimplemented_dd_instr<0xA8>,
            /* DD A9 */ unimplemented_dd_instr<0xA9>,
            /* DD AA */ unimplemented_dd_instr<0xAA>,
            /* DD AB */ unimplemented_dd_instr<0xAB>,
            /* DD AC */ unimplemented_dd_instr<0xAC>,
            /* DD AD */ unimplemented_dd_instr<0xAD>,
            /* DD AE */ unimplemented_dd_instr<0xAE>,
            /* DD AF */ unimplemented_dd_instr<0xAF>,
            /* DD B0 */ unimplemented_dd_instr<0xB0>,
            /* DD B1 */ unimplemented_dd_instr<0xB1>,
            /* DD B2 */ unimplemented_dd_instr<0xB2>,
            /* DD B3 */ unimplemented_dd_instr<0xB3>,
            /* DD B4 */ unimplemented_dd_instr<0xB4>,
            /* DD B5 */ unimplemented_dd_instr<0xB5>,
            /* DD B6 */ unimplemented_dd_instr<0xB6>,
            /* DD B7 */ unimplemented_dd_instr<0xB7>,
            /* DD B8 */ unimplemented_dd_instr<0xB8>,
            /* DD B9 */ unimplemented_dd_instr<0xB9>,
            /* DD BA */ unimplemented_dd_instr<0xBA>,
            /* DD BB */ unimplemented_dd_instr<0xBB>,
            /* DD BC */ unimplemented_dd_instr<0xBC>,
            /* DD BD */ unimplemented_dd_instr<0xBD>,
            /* DD BE */ unimplemented_dd_instr<0xBE>,
            /* DD BF */ unimplemented_dd_instr<0xBF>,
            /* DD C0 */ unimplemented_dd_instr<0xC0>,
            /* DD C1 */ unimplemented_dd_instr<0xC1>,
            /* DD C2 */ unimplemented_dd_instr<0xC2>,
            /* DD C3 */ unimplemented_dd_instr<0xC3>,
            /* DD C4 */ unimplemented_dd_instr<0xC4>,
            /* DD C5 */ unimplemented_dd_instr<0xC5>,
            /* DD C6 */ unimplemented_dd_instr<0xC6>,
            /* DD C7 */ unimplemented_dd_instr<0xC7>,
            /* DD C8 */ unimplemented_dd_instr<0xC8>,
            /* DD C9 */ unimplemented_dd_instr<0xC9>,
            /* DD CA */ unimplemented_dd_instr<0xCA>,
            /* DD CB */ unimplemented_dd_instr<0xCB>,
            /* DD CC */ unimplemented_dd_instr<0xCC>,
            /* DD CD */ unimplemented_dd_instr<0xCD>,
            /* DD CE */ unimplemented_dd_instr<0xCE>,
            /* DD CF */ unimplemented_dd_instr<0xCF>,
            /* DD D0 */ unimplemented_dd_instr<0xD0>,
            /* DD D1 */ unimplemented_dd_instr<0xD1>,
            /* DD D2 */ unimplemented_dd_instr<0xD2>,
            /* DD D3 */ unimplemented_dd_instr<0xD3>,
            /* DD D4 */ unimplemented_dd_instr<0xD4>,
            /* DD D5 */ unimplemented_dd_instr<0xD5>,
            /* DD D6 */ unimplemented_dd_instr<0xD6>,
            /* DD D7 */ unimplemented_dd_instr<0xD7>,
            /* DD D8 */ unimplemented_dd_instr<0xD8>,
            /* DD D9 */ unimplemented_dd_instr<0xD9>,
            /* DD DA */ unimplemented_dd_instr<0xDA>,
            /* DD DB */ unimplemented_dd_instr<0xDB>,
            /* DD DC */ unimplemented_dd_instr<0xDC>,
            /* DD DD */ unimplemented_dd_instr<0xDD>,
            /* DD DE */ unimplemented_dd_instr<0xDE>,
            /* DD DF */ unimplemented_dd_instr<0xDF>,
            /* DD E0 */ unimplemented_dd_instr<0xE0>,
            /* DD E1 */ instr_pop<Register::IX>,
            /* DD E2 */ unimplemented_dd_instr<0xE2>,
            /* DD E3 */ unimplemented_dd_instr<0xE3>,
            /* DD E4 */ unimplemented_dd_instr<0xE4>,
            /* DD E5 */ instr_push<Register::IX>,
            /* DD E6 */ unimplemented_dd_instr<0xE6>,
            /* DD E7 */ unimplemented_dd_instr<0xE7>,
            /* DD E8 */ unimplemented_dd_instr<0xE8>,
            /* DD E9 */ instr_jp<Condition::Always, AddressingMode::IX>,
            /* DD EA */ unimplemented_dd_instr<0xEA>,
            /* DD EB */ unimplemented_dd_instr<0xEB>,
            /* DD EC */ unimplemented_dd_instr<0xEC>,
            /* DD ED */ unimplemented_dd_instr<0xED>,
            /* DD EE */ unimplemented_dd_instr<0xEE>,
            /* DD EF */ unimplemented_dd_instr<0xEF>,
            /* DD F0 */ unimplemented_dd_instr<0xF0>,
            /* DD F1 */ unimplemented_dd_instr<0xF1>,
            /* DD F2 */ unimplemented_dd_instr<0xF2>,
            /* DD F3 */ unimplemented_dd_instr<0xF3>,
            /* DD F4 */ unimplemented_dd_instr<0xF4>,
            /* DD F5 */ unimplemented_dd_instr<0xF5>,
            /* DD F6 */ unimplemented_dd_instr<0xF6>,
            /* DD F7 */ unimplemented_dd_instr<0xF7>,
            /* DD F8 */ unimplemented_dd_instr<0xF8>,
            /* DD F9 */ unimplemented_dd_instr<0xF9>,
            /* DD FA */ unimplemented_dd_instr<0xFA>,
            /* DD FB */ unimplemented_dd_instr<0xFB>,
            /* DD FC */ unimplemented_dd_instr<0xFC>,
            /* DD FD */ unimplemented_dd_instr<0xFD>,
            /* DD FE */ unimplemented_dd_instr<0xFE>,
            /* DD FF */ unimplemented_dd_instr<0xFF>,
    };

    const instruction ed_instructions[0xC0] = {
            /* ED 00 */ unimplemented_ed_instr<0x00>,
            /* ED 01 */ unimplemented_ed_instr<0x01>,
            /* ED 02 */ unimplemented_ed_instr<0x02>,
            /* ED 03 */ unimplemented_ed_instr<0x03>,
            /* ED 04 */ unimplemented_ed_instr<0x04>,
            /* ED 05 */ unimplemented_ed_instr<0x05>,
            /* ED 06 */ unimplemented_ed_instr<0x06>,
            /* ED 07 */ unimplemented_ed_instr<0x07>,
            /* ED 08 */ unimplemented_ed_instr<0x08>,
            /* ED 09 */ unimplemented_ed_instr<0x09>,
            /* ED 0A */ unimplemented_ed_instr<0x0A>,
            /* ED 0B */ unimplemented_ed_instr<0x0B>,
            /* ED 0C */ unimplemented_ed_instr<0x0C>,
            /* ED 0D */ unimplemented_ed_instr<0x0D>,
            /* ED 0E */ unimplemented_ed_instr<0x0E>,
            /* ED 0F */ unimplemented_ed_instr<0x0F>,
            /* ED 10 */ unimplemented_ed_instr<0x10>,
            /* ED 11 */ unimplemented_ed_instr<0x11>,
            /* ED 12 */ unimplemented_ed_instr<0x12>,
            /* ED 13 */ unimplemented_ed_instr<0x13>,
            /* ED 14 */ unimplemented_ed_instr<0x14>,
            /* ED 15 */ unimplemented_ed_instr<0x15>,
            /* ED 16 */ unimplemented_ed_instr<0x16>,
            /* ED 17 */ unimplemented_ed_instr<0x17>,
            /* ED 18 */ unimplemented_ed_instr<0x18>,
            /* ED 19 */ unimplemented_ed_instr<0x19>,
            /* ED 1A */ unimplemented_ed_instr<0x1A>,
            /* ED 1B */ unimplemented_ed_instr<0x1B>,
            /* ED 1C */ unimplemented_ed_instr<0x1C>,
            /* ED 1D */ unimplemented_ed_instr<0x1D>,
            /* ED 1E */ unimplemented_ed_instr<0x1E>,
            /* ED 1F */ unimplemented_ed_instr<0x1F>,
            /* ED 20 */ unimplemented_ed_instr<0x20>,
            /* ED 21 */ unimplemented_ed_instr<0x21>,
            /* ED 22 */ unimplemented_ed_instr<0x22>,
            /* ED 23 */ unimplemented_ed_instr<0x23>,
            /* ED 24 */ unimplemented_ed_instr<0x24>,
            /* ED 25 */ unimplemented_ed_instr<0x25>,
            /* ED 26 */ unimplemented_ed_instr<0x26>,
            /* ED 27 */ unimplemented_ed_instr<0x27>,
            /* ED 28 */ unimplemented_ed_instr<0x28>,
            /* ED 29 */ unimplemented_ed_instr<0x29>,
            /* ED 2A */ unimplemented_ed_instr<0x2A>,
            /* ED 2B */ unimplemented_ed_instr<0x2B>,
            /* ED 2C */ unimplemented_ed_instr<0x2C>,
            /* ED 2D */ unimplemented_ed_instr<0x2D>,
            /* ED 2E */ unimplemented_ed_instr<0x2E>,
            /* ED 2F */ unimplemented_ed_instr<0x2F>,
            /* ED 30 */ unimplemented_ed_instr<0x30>,
            /* ED 31 */ unimplemented_ed_instr<0x31>,
            /* ED 32 */ unimplemented_ed_instr<0x32>,
            /* ED 33 */ unimplemented_ed_instr<0x33>,
            /* ED 34 */ unimplemented_ed_instr<0x34>,
            /* ED 35 */ unimplemented_ed_instr<0x35>,
            /* ED 36 */ unimplemented_ed_instr<0x36>,
            /* ED 37 */ unimplemented_ed_instr<0x37>,
            /* ED 38 */ unimplemented_ed_instr<0x38>,
            /* ED 39 */ unimplemented_ed_instr<0x39>,
            /* ED 3A */ unimplemented_ed_instr<0x3A>,
            /* ED 3B */ unimplemented_ed_instr<0x3B>,
            /* ED 3C */ unimplemented_ed_instr<0x3C>,
            /* ED 3D */ unimplemented_ed_instr<0x3D>,
            /* ED 3E */ unimplemented_ed_instr<0x3E>,
            /* ED 3F */ unimplemented_ed_instr<0x3F>,
            /* ED 40 */ unimplemented_ed_instr<0x40>,
            /* ED 41 */ unimplemented_ed_instr<0x41>,
            /* ED 42 */ unimplemented_ed_instr<0x42>,
            /* ED 43 */ unimplemented_ed_instr<0x43>,
            /* ED 44 */ unimplemented_ed_instr<0x44>,
            /* ED 45 */ unimplemented_ed_instr<0x45>,
            /* ED 46 */ unimplemented_ed_instr<0x46>,
            /* ED 47 */ unimplemented_ed_instr<0x47>,
            /* ED 48 */ unimplemented_ed_instr<0x48>,
            /* ED 49 */ unimplemented_ed_instr<0x49>,
            /* ED 4A */ unimplemented_ed_instr<0x4A>,
            /* ED 4B */ unimplemented_ed_instr<0x4B>,
            /* ED 4C */ unimplemented_ed_instr<0x4C>,
            /* ED 4D */ unimplemented_ed_instr<0x4D>,
            /* ED 4E */ unimplemented_ed_instr<0x4E>,
            /* ED 4F */ unimplemented_ed_instr<0x4F>,
            /* ED 50 */ unimplemented_ed_instr<0x50>,
            /* ED 51 */ unimplemented_ed_instr<0x51>,
            /* ED 52 */ unimplemented_ed_instr<0x52>,
            /* ED 53 */ unimplemented_ed_instr<0x53>,
            /* ED 54 */ unimplemented_ed_instr<0x54>,
            /* ED 55 */ unimplemented_ed_instr<0x55>,
            /* ED 56 */ unimplemented_ed_instr<0x56>,
            /* ED 57 */ unimplemented_ed_instr<0x57>,
            /* ED 58 */ unimplemented_ed_instr<0x58>,
            /* ED 59 */ unimplemented_ed_instr<0x59>,
            /* ED 5A */ unimplemented_ed_instr<0x5A>,
            /* ED 5B */ unimplemented_ed_instr<0x5B>,
            /* ED 5C */ unimplemented_ed_instr<0x5C>,
            /* ED 5D */ unimplemented_ed_instr<0x5D>,
            /* ED 5E */ unimplemented_ed_instr<0x5E>,
            /* ED 5F */ unimplemented_ed_instr<0x5F>,
            /* ED 60 */ unimplemented_ed_instr<0x60>,
            /* ED 61 */ unimplemented_ed_instr<0x61>,
            /* ED 62 */ unimplemented_ed_instr<0x62>,
            /* ED 63 */ unimplemented_ed_instr<0x63>,
            /* ED 64 */ unimplemented_ed_instr<0x64>,
            /* ED 65 */ unimplemented_ed_instr<0x65>,
            /* ED 66 */ unimplemented_ed_instr<0x66>,
            /* ED 67 */ unimplemented_ed_instr<0x67>,
            /* ED 68 */ unimplemented_ed_instr<0x68>,
            /* ED 69 */ unimplemented_ed_instr<0x69>,
            /* ED 6A */ unimplemented_ed_instr<0x6A>,
            /* ED 6B */ unimplemented_ed_instr<0x6B>,
            /* ED 6C */ unimplemented_ed_instr<0x6C>,
            /* ED 6D */ unimplemented_ed_instr<0x6D>,
            /* ED 6E */ unimplemented_ed_instr<0x6E>,
            /* ED 6F */ unimplemented_ed_instr<0x6F>,
            /* ED 70 */ unimplemented_ed_instr<0x70>,
            /* ED 71 */ unimplemented_ed_instr<0x71>,
            /* ED 72 */ unimplemented_ed_instr<0x72>,
            /* ED 73 */ unimplemented_ed_instr<0x73>,
            /* ED 74 */ unimplemented_ed_instr<0x74>,
            /* ED 75 */ unimplemented_ed_instr<0x75>,
            /* ED 76 */ unimplemented_ed_instr<0x76>,
            /* ED 77 */ unimplemented_ed_instr<0x77>,
            /* ED 78 */ unimplemented_ed_instr<0x78>,
            /* ED 79 */ unimplemented_ed_instr<0x79>,
            /* ED 7A */ unimplemented_ed_instr<0x7A>,
            /* ED 7B */ unimplemented_ed_instr<0x7B>,
            /* ED 7C */ unimplemented_ed_instr<0x7C>,
            /* ED 7D */ unimplemented_ed_instr<0x7D>,
            /* ED 7E */ unimplemented_ed_instr<0x7E>,
            /* ED 7F */ unimplemented_ed_instr<0x7F>,
            /* ED 80 */ unimplemented_ed_instr<0x80>,
            /* ED 81 */ unimplemented_ed_instr<0x81>,
            /* ED 82 */ unimplemented_ed_instr<0x82>,
            /* ED 83 */ unimplemented_ed_instr<0x83>,
            /* ED 84 */ unimplemented_ed_instr<0x84>,
            /* ED 85 */ unimplemented_ed_instr<0x85>,
            /* ED 86 */ unimplemented_ed_instr<0x86>,
            /* ED 87 */ unimplemented_ed_instr<0x87>,
            /* ED 88 */ unimplemented_ed_instr<0x88>,
            /* ED 89 */ unimplemented_ed_instr<0x89>,
            /* ED 8A */ unimplemented_ed_instr<0x8A>,
            /* ED 8B */ unimplemented_ed_instr<0x8B>,
            /* ED 8C */ unimplemented_ed_instr<0x8C>,
            /* ED 8D */ unimplemented_ed_instr<0x8D>,
            /* ED 8E */ unimplemented_ed_instr<0x8E>,
            /* ED 8F */ unimplemented_ed_instr<0x8F>,
            /* ED 90 */ unimplemented_ed_instr<0x90>,
            /* ED 91 */ unimplemented_ed_instr<0x91>,
            /* ED 92 */ unimplemented_ed_instr<0x92>,
            /* ED 93 */ unimplemented_ed_instr<0x93>,
            /* ED 94 */ unimplemented_ed_instr<0x94>,
            /* ED 95 */ unimplemented_ed_instr<0x95>,
            /* ED 96 */ unimplemented_ed_instr<0x96>,
            /* ED 97 */ unimplemented_ed_instr<0x97>,
            /* ED 98 */ unimplemented_ed_instr<0x98>,
            /* ED 99 */ unimplemented_ed_instr<0x99>,
            /* ED 9A */ unimplemented_ed_instr<0x9A>,
            /* ED 9B */ unimplemented_ed_instr<0x9B>,
            /* ED 9C */ unimplemented_ed_instr<0x9C>,
            /* ED 9D */ unimplemented_ed_instr<0x9D>,
            /* ED 9E */ unimplemented_ed_instr<0x9E>,
            /* ED 9F */ unimplemented_ed_instr<0x9F>,
            /* ED A0 */ unimplemented_ed_instr<0xA0>,
            /* ED A1 */ unimplemented_ed_instr<0xA1>,
            /* ED A2 */ unimplemented_ed_instr<0xA2>,
            /* ED A3 */ unimplemented_ed_instr<0xA3>,
            /* ED A4 */ unimplemented_ed_instr<0xA4>,
            /* ED A5 */ unimplemented_ed_instr<0xA5>,
            /* ED A6 */ unimplemented_ed_instr<0xA6>,
            /* ED A7 */ unimplemented_ed_instr<0xA7>,
            /* ED A8 */ unimplemented_ed_instr<0xA8>,
            /* ED A9 */ unimplemented_ed_instr<0xA9>,
            /* ED AA */ unimplemented_ed_instr<0xAA>,
            /* ED AB */ unimplemented_ed_instr<0xAB>,
            /* ED AC */ unimplemented_ed_instr<0xAC>,
            /* ED AD */ unimplemented_ed_instr<0xAD>,
            /* ED AE */ unimplemented_ed_instr<0xAE>,
            /* ED AF */ unimplemented_ed_instr<0xAF>,
            /* ED B0 */ instr_ldir,
            /* ED B1 */ unimplemented_ed_instr<0xB1>,
            /* ED B2 */ unimplemented_ed_instr<0xB2>,
            /* ED B3 */ unimplemented_ed_instr<0xB3>,
            /* ED B4 */ unimplemented_ed_instr<0xB4>,
            /* ED B5 */ unimplemented_ed_instr<0xB5>,
            /* ED B6 */ unimplemented_ed_instr<0xB6>,
            /* ED B7 */ unimplemented_ed_instr<0xB7>,
            /* ED B8 */ unimplemented_ed_instr<0xB8>,
            /* ED B9 */ unimplemented_ed_instr<0xB9>,
            /* ED BA */ unimplemented_ed_instr<0xBA>,
            /* ED BB */ unimplemented_ed_instr<0xBB>,
            /* ED BC */ unimplemented_ed_instr<0xBC>,
            /* ED BD */ unimplemented_ed_instr<0xBD>,
            /* ED BE */ unimplemented_ed_instr<0xBE>,
            /* ED BF */ unimplemented_ed_instr<0xBF>,
    };

    const instruction fd_instructions[0x100] = {
            /* FD 00 */ unimplemented_fd_instr<0x00>,
            /* FD 01 */ unimplemented_fd_instr<0x01>,
            /* FD 02 */ unimplemented_fd_instr<0x02>,
            /* FD 03 */ unimplemented_fd_instr<0x03>,
            /* FD 04 */ unimplemented_fd_instr<0x04>,
            /* FD 05 */ unimplemented_fd_instr<0x05>,
            /* FD 06 */ unimplemented_fd_instr<0x06>,
            /* FD 07 */ unimplemented_fd_instr<0x07>,
            /* FD 08 */ unimplemented_fd_instr<0x08>,
            /* FD 09 */ unimplemented_fd_instr<0x09>,
            /* FD 0A */ unimplemented_fd_instr<0x0A>,
            /* FD 0B */ unimplemented_fd_instr<0x0B>,
            /* FD 0C */ unimplemented_fd_instr<0x0C>,
            /* FD 0D */ unimplemented_fd_instr<0x0D>,
            /* FD 0E */ unimplemented_fd_instr<0x0E>,
            /* FD 0F */ unimplemented_fd_instr<0x0F>,
            /* FD 10 */ unimplemented_fd_instr<0x10>,
            /* FD 11 */ unimplemented_fd_instr<0x11>,
            /* FD 12 */ unimplemented_fd_instr<0x12>,
            /* FD 13 */ unimplemented_fd_instr<0x13>,
            /* FD 14 */ unimplemented_fd_instr<0x14>,
            /* FD 15 */ unimplemented_fd_instr<0x15>,
            /* FD 16 */ unimplemented_fd_instr<0x16>,
            /* FD 17 */ unimplemented_fd_instr<0x17>,
            /* FD 18 */ unimplemented_fd_instr<0x18>,
            /* FD 19 */ unimplemented_fd_instr<0x19>,
            /* FD 1A */ unimplemented_fd_instr<0x1A>,
            /* FD 1B */ unimplemented_fd_instr<0x1B>,
            /* FD 1C */ unimplemented_fd_instr<0x1C>,
            /* FD 1D */ unimplemented_fd_instr<0x1D>,
            /* FD 1E */ unimplemented_fd_instr<0x1E>,
            /* FD 1F */ unimplemented_fd_instr<0x1F>,
            /* FD 20 */ unimplemented_fd_instr<0x20>,
            /* FD 21 */ instr_ld<Register::IY, AddressingMode::Immediate>,
            /* FD 22 */ unimplemented_fd_instr<0x22>,
            /* FD 23 */ instr_inc<Register::IY>,
            /* FD 24 */ unimplemented_fd_instr<0x24>,
            /* FD 25 */ unimplemented_fd_instr<0x25>,
            /* FD 26 */ unimplemented_fd_instr<0x26>,
            /* FD 27 */ unimplemented_fd_instr<0x27>,
            /* FD 28 */ unimplemented_fd_instr<0x28>,
            /* FD 29 */ unimplemented_fd_instr<0x29>,
            /* FD 2A */ unimplemented_fd_instr<0x2A>,
            /* FD 2B */ unimplemented_fd_instr<0x2B>,
            /* FD 2C */ unimplemented_fd_instr<0x2C>,
            /* FD 2D */ unimplemented_fd_instr<0x2D>,
            /* FD 2E */ unimplemented_fd_instr<0x2E>,
            /* FD 2F */ unimplemented_fd_instr<0x2F>,
            /* FD 30 */ unimplemented_fd_instr<0x30>,
            /* FD 31 */ unimplemented_fd_instr<0x31>,
            /* FD 32 */ unimplemented_fd_instr<0x32>,
            /* FD 33 */ unimplemented_fd_instr<0x33>,
            /* FD 34 */ unimplemented_fd_instr<0x34>,
            /* FD 35 */ unimplemented_fd_instr<0x35>,
            /* FD 36 */ unimplemented_fd_instr<0x36>,
            /* FD 37 */ unimplemented_fd_instr<0x37>,
            /* FD 38 */ unimplemented_fd_instr<0x38>,
            /* FD 39 */ unimplemented_fd_instr<0x39>,
            /* FD 3A */ unimplemented_fd_instr<0x3A>,
            /* FD 3B */ unimplemented_fd_instr<0x3B>,
            /* FD 3C */ unimplemented_fd_instr<0x3C>,
            /* FD 3D */ unimplemented_fd_instr<0x3D>,
            /* FD 3E */ unimplemented_fd_instr<0x3E>,
            /* FD 3F */ unimplemented_fd_instr<0x3F>,
            /* FD 40 */ unimplemented_fd_instr<0x40>,
            /* FD 41 */ unimplemented_fd_instr<0x41>,
            /* FD 42 */ unimplemented_fd_instr<0x42>,
            /* FD 43 */ unimplemented_fd_instr<0x43>,
            /* FD 44 */ unimplemented_fd_instr<0x44>,
            /* FD 45 */ unimplemented_fd_instr<0x45>,
            /* FD 46 */ unimplemented_fd_instr<0x46>,
            /* FD 47 */ unimplemented_fd_instr<0x47>,
            /* FD 48 */ unimplemented_fd_instr<0x48>,
            /* FD 49 */ unimplemented_fd_instr<0x49>,
            /* FD 4A */ unimplemented_fd_instr<0x4A>,
            /* FD 4B */ unimplemented_fd_instr<0x4B>,
            /* FD 4C */ unimplemented_fd_instr<0x4C>,
            /* FD 4D */ unimplemented_fd_instr<0x4D>,
            /* FD 4E */ unimplemented_fd_instr<0x4E>,
            /* FD 4F */ unimplemented_fd_instr<0x4F>,
            /* FD 50 */ unimplemented_fd_instr<0x50>,
            /* FD 51 */ unimplemented_fd_instr<0x51>,
            /* FD 52 */ unimplemented_fd_instr<0x52>,
            /* FD 53 */ unimplemented_fd_instr<0x53>,
            /* FD 54 */ unimplemented_fd_instr<0x54>,
            /* FD 55 */ unimplemented_fd_instr<0x55>,
            /* FD 56 */ unimplemented_fd_instr<0x56>,
            /* FD 57 */ unimplemented_fd_instr<0x57>,
            /* FD 58 */ unimplemented_fd_instr<0x58>,
            /* FD 59 */ unimplemented_fd_instr<0x59>,
            /* FD 5A */ unimplemented_fd_instr<0x5A>,
            /* FD 5B */ unimplemented_fd_instr<0x5B>,
            /* FD 5C */ unimplemented_fd_instr<0x5C>,
            /* FD 5D */ unimplemented_fd_instr<0x5D>,
            /* FD 5E */ unimplemented_fd_instr<0x5E>,
            /* FD 5F */ unimplemented_fd_instr<0x5F>,
            /* FD 60 */ unimplemented_fd_instr<0x60>,
            /* FD 61 */ unimplemented_fd_instr<0x61>,
            /* FD 62 */ unimplemented_fd_instr<0x62>,
            /* FD 63 */ unimplemented_fd_instr<0x63>,
            /* FD 64 */ unimplemented_fd_instr<0x64>,
            /* FD 65 */ unimplemented_fd_instr<0x65>,
            /* FD 66 */ unimplemented_fd_instr<0x66>,
            /* FD 67 */ unimplemented_fd_instr<0x67>,
            /* FD 68 */ unimplemented_fd_instr<0x68>,
            /* FD 69 */ unimplemented_fd_instr<0x69>,
            /* FD 6A */ unimplemented_fd_instr<0x6A>,
            /* FD 6B */ unimplemented_fd_instr<0x6B>,
            /* FD 6C */ unimplemented_fd_instr<0x6C>,
            /* FD 6D */ unimplemented_fd_instr<0x6D>,
            /* FD 6E */ unimplemented_fd_instr<0x6E>,
            /* FD 6F */ unimplemented_fd_instr<0x6F>,
            /* FD 70 */ unimplemented_fd_instr<0x70>,
            /* FD 71 */ unimplemented_fd_instr<0x71>,
            /* FD 72 */ unimplemented_fd_instr<0x72>,
            /* FD 73 */ unimplemented_fd_instr<0x73>,
            /* FD 74 */ unimplemented_fd_instr<0x74>,
            /* FD 75 */ unimplemented_fd_instr<0x75>,
            /* FD 76 */ unimplemented_fd_instr<0x76>,
            /* FD 77 */ unimplemented_fd_instr<0x77>,
            /* FD 78 */ unimplemented_fd_instr<0x78>,
            /* FD 79 */ unimplemented_fd_instr<0x79>,
            /* FD 7A */ unimplemented_fd_instr<0x7A>,
            /* FD 7B */ unimplemented_fd_instr<0x7B>,
            /* FD 7C */ unimplemented_fd_instr<0x7C>,
            /* FD 7D */ unimplemented_fd_instr<0x7D>,
            /* FD 7E */ instr_ld<Register::A, AddressingMode::IYPlus>,
            /* FD 7F */ unimplemented_fd_instr<0x7F>,
            /* FD 80 */ unimplemented_fd_instr<0x80>,
            /* FD 81 */ unimplemented_fd_instr<0x81>,
            /* FD 82 */ unimplemented_fd_instr<0x82>,
            /* FD 83 */ unimplemented_fd_instr<0x83>,
            /* FD 84 */ unimplemented_fd_instr<0x84>,
            /* FD 85 */ unimplemented_fd_instr<0x85>,
            /* FD 86 */ unimplemented_fd_instr<0x86>,
            /* FD 87 */ unimplemented_fd_instr<0x87>,
            /* FD 88 */ unimplemented_fd_instr<0x88>,
            /* FD 89 */ unimplemented_fd_instr<0x89>,
            /* FD 8A */ unimplemented_fd_instr<0x8A>,
            /* FD 8B */ unimplemented_fd_instr<0x8B>,
            /* FD 8C */ unimplemented_fd_instr<0x8C>,
            /* FD 8D */ unimplemented_fd_instr<0x8D>,
            /* FD 8E */ unimplemented_fd_instr<0x8E>,
            /* FD 8F */ unimplemented_fd_instr<0x8F>,
            /* FD 90 */ unimplemented_fd_instr<0x90>,
            /* FD 91 */ unimplemented_fd_instr<0x91>,
            /* FD 92 */ unimplemented_fd_instr<0x92>,
            /* FD 93 */ unimplemented_fd_instr<0x93>,
            /* FD 94 */ unimplemented_fd_instr<0x94>,
            /* FD 95 */ unimplemented_fd_instr<0x95>,
            /* FD 96 */ unimplemented_fd_instr<0x96>,
            /* FD 97 */ unimplemented_fd_instr<0x97>,
            /* FD 98 */ unimplemented_fd_instr<0x98>,
            /* FD 99 */ unimplemented_fd_instr<0x99>,
            /* FD 9A */ unimplemented_fd_instr<0x9A>,
            /* FD 9B */ unimplemented_fd_instr<0x9B>,
            /* FD 9C */ unimplemented_fd_instr<0x9C>,
            /* FD 9D */ unimplemented_fd_instr<0x9D>,
            /* FD 9E */ unimplemented_fd_instr<0x9E>,
            /* FD 9F */ unimplemented_fd_instr<0x9F>,
            /* FD A0 */ unimplemented_fd_instr<0xA0>,
            /* FD A1 */ unimplemented_fd_instr<0xA1>,
            /* FD A2 */ unimplemented_fd_instr<0xA2>,
            /* FD A3 */ unimplemented_fd_instr<0xA3>,
            /* FD A4 */ unimplemented_fd_instr<0xA4>,
            /* FD A5 */ unimplemented_fd_instr<0xA5>,
            /* FD A6 */ unimplemented_fd_instr<0xA6>,
            /* FD A7 */ unimplemented_fd_instr<0xA7>,
            /* FD A8 */ unimplemented_fd_instr<0xA8>,
            /* FD A9 */ unimplemented_fd_instr<0xA9>,
            /* FD AA */ unimplemented_fd_instr<0xAA>,
            /* FD AB */ unimplemented_fd_instr<0xAB>,
            /* FD AC */ unimplemented_fd_instr<0xAC>,
            /* FD AD */ unimplemented_fd_instr<0xAD>,
            /* FD AE */ unimplemented_fd_instr<0xAE>,
            /* FD AF */ unimplemented_fd_instr<0xAF>,
            /* FD B0 */ unimplemented_fd_instr<0xB0>,
            /* FD B1 */ unimplemented_fd_instr<0xB1>,
            /* FD B2 */ unimplemented_fd_instr<0xB2>,
            /* FD B3 */ unimplemented_fd_instr<0xB3>,
            /* FD B4 */ unimplemented_fd_instr<0xB4>,
            /* FD B5 */ unimplemented_fd_instr<0xB5>,
            /* FD B6 */ unimplemented_fd_instr<0xB6>,
            /* FD B7 */ unimplemented_fd_instr<0xB7>,
            /* FD B8 */ unimplemented_fd_instr<0xB8>,
            /* FD B9 */ unimplemented_fd_instr<0xB9>,
            /* FD BA */ unimplemented_fd_instr<0xBA>,
            /* FD BB */ unimplemented_fd_instr<0xBB>,
            /* FD BC */ unimplemented_fd_instr<0xBC>,
            /* FD BD */ unimplemented_fd_instr<0xBD>,
            /* FD BE */ unimplemented_fd_instr<0xBE>,
            /* FD BF */ unimplemented_fd_instr<0xBF>,
            /* FD C0 */ unimplemented_fd_instr<0xC0>,
            /* FD C1 */ unimplemented_fd_instr<0xC1>,
            /* FD C2 */ unimplemented_fd_instr<0xC2>,
            /* FD C3 */ unimplemented_fd_instr<0xC3>,
            /* FD C4 */ unimplemented_fd_instr<0xC4>,
            /* FD C5 */ unimplemented_fd_instr<0xC5>,
            /* FD C6 */ unimplemented_fd_instr<0xC6>,
            /* FD C7 */ unimplemented_fd_instr<0xC7>,
            /* FD C8 */ unimplemented_fd_instr<0xC8>,
            /* FD C9 */ unimplemented_fd_instr<0xC9>,
            /* FD CA */ unimplemented_fd_instr<0xCA>,
            /* FD CB */ unimplemented_fd_instr<0xCB>,
            /* FD CC */ unimplemented_fd_instr<0xCC>,
            /* FD CD */ unimplemented_fd_instr<0xCD>,
            /* FD CE */ unimplemented_fd_instr<0xCE>,
            /* FD CF */ unimplemented_fd_instr<0xCF>,
            /* FD D0 */ unimplemented_fd_instr<0xD0>,
            /* FD D1 */ unimplemented_fd_instr<0xD1>,
            /* FD D2 */ unimplemented_fd_instr<0xD2>,
            /* FD D3 */ unimplemented_fd_instr<0xD3>,
            /* FD D4 */ unimplemented_fd_instr<0xD4>,
            /* FD D5 */ unimplemented_fd_instr<0xD5>,
            /* FD D6 */ unimplemented_fd_instr<0xD6>,
            /* FD D7 */ unimplemented_fd_instr<0xD7>,
            /* FD D8 */ unimplemented_fd_instr<0xD8>,
            /* FD D9 */ unimplemented_fd_instr<0xD9>,
            /* FD DA */ unimplemented_fd_instr<0xDA>,
            /* FD DB */ unimplemented_fd_instr<0xDB>,
            /* FD DC */ unimplemented_fd_instr<0xDC>,
            /* FD DD */ unimplemented_fd_instr<0xDD>,
            /* FD DE */ unimplemented_fd_instr<0xDE>,
            /* FD DF */ unimplemented_fd_instr<0xDF>,
            /* FD E0 */ unimplemented_fd_instr<0xE0>,
            /* FD E1 */ instr_pop<Register::IY>,
            /* FD E2 */ unimplemented_fd_instr<0xE2>,
            /* FD E3 */ unimplemented_fd_instr<0xE3>,
            /* FD E4 */ unimplemented_fd_instr<0xE4>,
            /* FD E5 */ instr_push<Register::IY>,
            /* FD E6 */ unimplemented_fd_instr<0xE6>,
            /* FD E7 */ unimplemented_fd_instr<0xE7>,
            /* FD E8 */ unimplemented_fd_instr<0xE8>,
            /* FD E9 */ instr_jp<Condition::Always, AddressingMode::IY>,
            /* FD EA */ unimplemented_fd_instr<0xEA>,
            /* FD EB */ unimplemented_fd_instr<0xEB>,
            /* FD EC */ unimplemented_fd_instr<0xEC>,
            /* FD ED */ unimplemented_fd_instr<0xED>,
            /* FD EE */ unimplemented_fd_instr<0xEE>,
            /* FD EF */ unimplemented_fd_instr<0xEF>,
            /* FD F0 */ unimplemented_fd_instr<0xF0>,
            /* FD F1 */ unimplemented_fd_instr<0xF1>,
            /* FD F2 */ unimplemented_fd_instr<0xF2>,
            /* FD F3 */ unimplemented_fd_instr<0xF3>,
            /* FD F4 */ unimplemented_fd_instr<0xF4>,
            /* FD F5 */ unimplemented_fd_instr<0xF5>,
            /* FD F6 */ unimplemented_fd_instr<0xF6>,
            /* FD F7 */ unimplemented_fd_instr<0xF7>,
            /* FD F8 */ unimplemented_fd_instr<0xF8>,
            /* FD F9 */ unimplemented_fd_instr<0xF9>,
            /* FD FA */ unimplemented_fd_instr<0xFA>,
            /* FD FB */ unimplemented_fd_instr<0xFB>,
            /* FD FC */ unimplemented_fd_instr<0xFC>,
            /* FD FD */ unimplemented_fd_instr<0xFD>,
            /* FD FE */ unimplemented_fd_instr<0xFE>,
            /* FD FF */ unimplemented_fd_instr<0xFF>,
    };
}