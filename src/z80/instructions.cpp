#include "instructions.h"

#include <cstdio>
#include <cstdlib>

#include "z80.h"
#include "util/types.h"
#include "util/log.h"
#include "util.h"

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
        DE,
        IX,
        IXPlus,
        IXPlusPrevious,
        IY,
        IYPlus,
        IYPlusPrevious
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
                return z80.ix.raw;
            case Register::IXH:
                return z80.ix[Z80::WideRegister::Hi];
            case Register::IXL:
                return z80.ix[Z80::WideRegister::Lo];
            case Register::IY:
                return z80.iy.raw;
            case Register::IYH:
                return z80.iy[Z80::WideRegister::Hi];
            case Register::IYL:
                return z80.iy[Z80::WideRegister::Lo];
            case Register::I:
                return z80.i;
            case Register::R:
                return z80.r;
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
            case Register::IXH:
                z80.ix(Z80::WideRegister::Hi) = value;
                break;
            case Register::IXL:
                z80.ix(Z80::WideRegister::Lo) = value;
                break;
            case Register::IY:
                z80.iy = value;
                break;
            case Register::IYH:
                z80.iy(Z80::WideRegister::Hi) = value;
                break;
            case Register::IYL:
                z80.iy(Z80::WideRegister::Lo) = value;
                break;
            case Register::I:
                z80.i = value;
                break;
            case Register::R:
                z80.r = value;
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
            case AddressingMode::Immediate:
                logfatal("get_address() should not be used with the immediate addressing mode!");
            case AddressingMode::Indirect:
                return read_16_pc();
            case AddressingMode::HL:
                return z80.hl.raw;
            case AddressingMode::BC:
                return z80.bc.raw;
            case AddressingMode::DE:
                return z80.de.raw;
            case AddressingMode::IX:
                return z80.ix.raw;
            case AddressingMode::IY:
                return z80.iy.raw;
            case AddressingMode::IXPlus:
                return get_register<Register::IX>() + (s8)z80.read_byte(z80.pc++);
            case AddressingMode::IXPlusPrevious:
                return get_register<Register::IX>() + z80.prev_immediate;
            case AddressingMode::IYPlus:
                return get_register<Register::IY>() + (s8)z80.read_byte(z80.pc++);
            case AddressingMode::IYPlusPrevious:
                return get_register<Register::IY>() + z80.prev_immediate;
        }
    }

    template <AddressingMode addressingMode, typename T>
    T read_value() {
        if (addressingMode == AddressingMode::Immediate) {
            switch (sizeof(T)) {
                case sizeof(u16): {
                    return read_16_pc();
                }
                case sizeof(u8):
                    return z80.read_byte(z80.pc++);
            }
        } else {
            u16 address = get_address<addressingMode>();
            if constexpr(std::is_same_v<T, u16>) {
                return read_16(address);
            } else if constexpr(std::is_same_v<T, u8>) {
                return z80.read_byte(address);
            }
        }
    }

    template <AddressingMode addressingMode, typename T>
    void write_value(T value) {
        static_assert(std::is_same_v<T, u8>, "only supported for u8");
        z80.write_byte(get_address<addressingMode>(), value);
    }

    constexpr bool parity(u8 value) {
        int num_1_bits = 0;
        for (int i = 0; i < 8; i++) {
            num_1_bits += (value >> i) & 1;
        }
        return (num_1_bits % 2) == 0;
    }

    constexpr bool vflag(u8 a, u8 b, u8 r) {
        return ((a & 0x80) == (b & 0x80)) && ((a & 0x80) != (r & 0x80));
    }

    constexpr bool vflag_16(u16 a, u16 b, u16 r) {
        return ((a & 0x8000) == (b & 0x8000)) && ((a & 0x8000) != (r & 0x8000));
    }

    constexpr bool carry(int bit, u16 a, u16 b, bool c) {
        u32 result = a + b + c;
        u32 carry = result ^ a ^ b;
        return carry & (1 << bit);
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
        s8 offset = z80.read_byte(z80.pc++);
        if (check_condition<c>()) {
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
                z80.f.b3 = (r >> 3) & 1;
                z80.f.b5 = (r >> 5) & 1;

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

    template <AddressingMode src>
    int instr_dec() {
        u16 address = get_address<src>();
        u8 m = z80.read_byte(address);
        u8 r = m - 1;
        z80.write_byte(address, r);

        z80.f.s = ((s8)r) < 0;
        z80.f.z = r == 0;
        z80.f.h = (r & 0xF) > (m & 0xF); // overflow on lower half of reg
        z80.f.p_v = m == 0x80;
        z80.f.n = true;
        z80.f.b3 = (r >> 3) & 1;
        z80.f.b5 = (r >> 5) & 1;

        return 4;
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

    template <AddressingMode src>
    int instr_inc() {
        u16 address = get_address<src>();
        u8 m = z80.read_byte(address);
        u8 r = m + 1;
        z80.f.n = false;
        z80.f.p_v = m == 0x7F;
        z80.f.h = (m & 0xF) == 0xF;
        z80.f.b3 = (r >> 3) & 1;
        z80.f.b5 = (r >> 5) & 1;
        z80.f.z = r == 0;
        z80.f.s = ((s8)r) < 0;
        z80.write_byte(address, r);
        return 11;
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
        u16 address = get_address<dst>();

        if (sizeof(srcT) == sizeof(u16)) {
            u16 value = get_register<src>();
            z80.write_byte(address + 0, value & 0xFF);
            z80.write_byte(address + 1, (value >> 8) & 0xFF);
            return 16;
        } else {
            z80.write_byte(address, get_register<src>());
            return 13;
        }
    }

    template <AddressingMode dst, AddressingMode src>
    int instr_ld() {
        u16 dst_addr = get_address<dst>(); // Need to get the dst address first, to handle cases like ld (ix+*),*
        u8 val = read_value<src, u8>();
        z80.write_byte(dst_addr, val);

        return 10;
    }

    template <Condition c>
    int instr_call() {
        // Read the address first so the return address is correct
        u16 address = get_address<AddressingMode::Indirect>();

        if (check_condition<c>()) {
            // Push return address
            stack_push<u16>(z80.pc);
            logtrace("Calling function at %04X - return address is %04X", address, z80.pc);
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
            logtrace("Returning to address %04X", z80.pc);
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

    template <Register src>
    int instr_or() {
        z80.a = z80.a | get_register<src>();
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

    template <AddressingMode addressingMode>
    int instr_xor() {
        z80.a = z80.a ^ read_value<addressingMode, u8>();
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

    template <Register src>
    int instr_xor() {
        z80.a = z80.a ^ get_register<src>();
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
        static_assert(get_register_size<src>() == get_register_size<dst>(), "src and dst must be the same type");
        if (get_register_size<dst>() == sizeof(u8)) {
            u16 op1 = get_register<dst>();
            u16 op2 = get_register<src>();
            u16 res = op1 + op2;
            set_register<dst>(res & 0xFF);

            z80.f.z = (res & 0xFF) == 0;
            z80.f.s = ((s8)(res & 0xFF)) < 0;
            z80.f.p_v = vflag(op1, op2, res);
            z80.f.h = carry(4, op1, op2, false);
            z80.f.n = false;
            z80.f.c = carry(8, op1, op2, false);
            z80.f.b3 = (res >> 3) & 1;
            z80.f.b5 = (res >> 5) & 1;
            return 4;
        } else if (get_register_size<dst>() == sizeof(u16)) {
            u32 op1 = get_register<dst>();
            u32 op2 = get_register<src>();
            u32 res = op1 + op2;
            set_register<dst>(res & 0xFFFF);

            z80.f.h = carry(12, op1, op2, false);
            z80.f.n = false;
            z80.f.c = carry(16, op1, op2, false);
            z80.f.b3 = (res >> 11) & 1;
            z80.f.b5 = (res >> 13) & 1;
            return 11;
        }
        logfatal("Should not reach here");
    }

    template <Register dst, AddressingMode src>
    int instr_add() {
        static_assert(get_register_size<dst>() == sizeof(u8));
        u16 op1 = get_register<dst>();
        u16 op2 = read_value<src, u8>();
        u16 res = op1 + op2;
        set_register<dst>(res & 0xFF);

        z80.f.z = (res & 0xFF) == 0;
        z80.f.s = ((s8)(res & 0xFF)) < 0;
        z80.f.p_v = vflag(op1, op2, res);
        z80.f.h = carry(4, op1, op2, false);
        z80.f.n = false;
        z80.f.c = carry(8, op1, op2, false);
        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 7;
    }

    template <Register dst, Register src>
    int instr_adc() {
        static_assert(get_register_size<src>() == get_register_size<dst>(), "src and dst must be the same type");
        if (get_register_size<dst>() == sizeof(u8)) {
            u16 op1 = get_register<dst>();
            u16 op2 = get_register<src>();
            u16 res = op1 + op2 + (z80.f.c ? 1 : 0);
            set_register<dst>(res & 0xFF);

            z80.f.z = (res & 0xFF) == 0;
            z80.f.s = ((s8)(res & 0xFF)) < 0;
            z80.f.p_v = vflag(op1, op2, res);
            z80.f.h = carry(4, op1, op2, z80.f.c);
            z80.f.n = false;
            z80.f.c = carry(8, op1, op2, z80.f.c);
            z80.f.b3 = (res >> 3) & 1;
            z80.f.b5 = (res >> 5) & 1;
            return 4;
        } else if (get_register_size<dst>() == sizeof(u16)) {
            u32 op1 = get_register<dst>();
            u32 op2 = get_register<src>();
            u32 res = op1 + op2 + (z80.f.c ? 1 : 0);
            set_register<dst>(res & 0xFFFF);

            z80.f.z = res == 0;
            z80.f.s = ((s16)(res & 0xFFFF)) < 0;
            z80.f.p_v = vflag_16(op1, op2, res);
            z80.f.h = carry(12, op1, op2, z80.f.c);
            z80.f.n = false;
            z80.f.c = carry(16, op1, op2, z80.f.c);
            z80.f.b3 = (res >> 11) & 1;
            z80.f.b5 = (res >> 13) & 1;
            return 11;
        }
        logfatal("Should not reach here");
    }

    template <Register dst, AddressingMode src>
    int instr_adc() {
        static_assert(get_register_size<dst>() == sizeof(u8));
        u16 op1 = get_register<dst>();
        u16 op2 = read_value<src, u8>();
        u16 res = op1 + op2 + (z80.f.c ? 1 : 0);
        set_register<dst>(res & 0xFF);

        z80.f.z = (res & 0xFF) == 0;
        z80.f.s = ((s8)(res & 0xFF)) < 0;
        z80.f.p_v = vflag(op1, op2, res);
        z80.f.h = carry(4, op1, op2, z80.f.c);
        z80.f.n = false;
        z80.f.c = carry(8, op1, op2, z80.f.c);
        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 11;
    }

    template <Register src>
    int instr_sub() {
        u16 op1 = z80.a;
        // this is subtraction, convert to negative and add
        u8 value = get_register<src>();
        u16 op2 = (u8)((~value) + 1);
        u16 res = op1 + op2;
        z80.a = res & 0xFF;

        z80.f.z = (res & 0xFF) == 0;
        z80.f.s = ((s8)(res & 0xFF)) < 0;
        z80.f.p_v = vflag(op1, ~value, res);

        // These three flags are set the opposite way they would be in normal addition
        z80.f.h = (op1 & 0xF) < (value & 0xF);
        z80.f.n = true;
        z80.f.c = value > op1;

        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 4;
    }

    template <AddressingMode src>
    int instr_sub() {
        u16 op1 = z80.a;
        // this is subtraction, convert to negative and add
        u8 value = read_value<src, u8>();
        u16 op2 = (u8)((~value) + 1);
        u16 res = op1 + op2;
        z80.a = res & 0xFF;

        z80.f.z = (res & 0xFF) == 0;
        z80.f.s = ((s8)(res & 0xFF)) < 0;
        z80.f.p_v = vflag(op1, ~value, res);

        // These three flags are set the opposite way they would be in normal addition
        z80.f.h = (op1 & 0xF) < (value & 0xF);
        z80.f.n = true;
        z80.f.c = value > op1;

        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 7;
    }

    int instr_neg() {
        // "neg is the same as subtracting a from 0"
        u16 op1 = 0;
        // this is subtraction, convert to negative and add
        u8 value = z80.a;
        u16 op2 = (u8)((~value) + 1);
        u16 res = op1 + op2;
        z80.a = res & 0xFF;

        z80.f.z = (res & 0xFF) == 0;
        z80.f.s = ((s8)(res & 0xFF)) < 0;
        z80.f.p_v = vflag(op1, ~value, res);

        // These three flags are set the opposite way they would be in normal addition
        z80.f.h = (op1 & 0xF) < (value & 0xF);
        z80.f.n = true;
        z80.f.c = value > op1;

        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 7;
    }

    template <Register dst, Register src, typename dstT = typename reg_type<dst>::type, typename srcT = typename reg_type<src>::type>
    int instr_sbc() {
        static_assert(std::is_same_v<dstT, srcT>, "SBC only valid when dst and src are the same size");

        if (std::is_same_v<dstT, u16>) {
            u16 minuend = get_register<dst>();
            u32 subtrahend = get_register<src>() + (z80.f.c ? 1 : 0);
            u16 result = minuend - subtrahend;
            set_register<dst>(result);
            z80.f.c = subtrahend > minuend;
            z80.f.n = true;
            z80.f.p_v = vflag_16(minuend, ~subtrahend + 1, result);
            z80.f.h = (minuend & 0xFFF) < (subtrahend & 0xFFF);
            z80.f.b3 = (result >> 11) & 1;
            z80.f.b5 = (result >> 13) & 1;
            z80.f.z = result == 0;
            z80.f.s = ((s16)result) < 0;

            return 15;
        } else if (std::is_same_v<dstT, u8>) {
            u8 minuend = get_register<dst>();
            u8 value = get_register<src>();
            int carry = z80.f.c ? 1 : 0;
            u16 subtrahend = value + carry;
            u8 result = minuend - subtrahend;
            set_register<dst>(result);
            z80.f.c = subtrahend > minuend;
            z80.f.n = true;
            z80.f.p_v = vflag(minuend, ~value, result);
            z80.f.h = (value & 0xF) + carry > (minuend & 0xF);
            z80.f.b3 = (result >> 3) & 1;
            z80.f.b5 = (result >> 5) & 1;
            z80.f.z = result == 0;
            z80.f.s = ((s8)result) < 0;
            return 4;
        }
    }

    template <Register dst, AddressingMode src, typename dstT = typename reg_type<dst>::type>
    int instr_sbc() {
        static_assert(std::is_same_v<dstT, u8>, "sbc mem only valid for 8 bit registers");

        int carry = z80.f.c ? 1 : 0;

        u8 minuend = get_register<dst>();
        u8 value = read_value<src, u8>();
        u16 subtrahend = value + carry;
        u8 result = minuend - subtrahend;
        set_register<dst>(result);
        z80.f.c = subtrahend > minuend;
        z80.f.n = true;
        z80.f.p_v = vflag(minuend, ~value, result);
        z80.f.h = (value & 0xF) + carry > (minuend & 0xF);
        z80.f.b3 = (result >> 3) & 1;
        z80.f.b5 = (result >> 5) & 1;
        z80.f.z = result == 0;
        z80.f.s = ((s8)result) < 0;

        return 7;
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

    template <AddressingMode addressingMode>
    int instr_cp() {
        u8 s = read_value<addressingMode, u8>();
        u8 r = z80.a - s;

        z80.f.s = ((s8)r) < 0;
        z80.f.z = r == 0;
        z80.f.h = (s & 0xF) > (z80.a & 0xF); // overflow on lower half of reg
        z80.f.p_v = vflag(z80.a, ~s, r);
        z80.f.n = true;
        z80.f.c = s > z80.a;
        z80.f.b3 = (s >> 3) & 1;
        z80.f.b5 = (s >> 5) & 1;

        return 7;
    }

    template <Register src>
    int instr_cp() {
        u8 s = get_register<src>();
        u8 r = z80.a - s;

        z80.f.s = ((s8)r) < 0;
        z80.f.z = r == 0;
        z80.f.h = (s & 0xF) > (z80.a & 0xF); // overflow on lower half of reg
        z80.f.p_v = vflag(z80.a, ~s, r);
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

    template<int hl_increment>
    inline int instr_cpd_cpi() {
        u8 s = read_value<AddressingMode::HL, u8>();
        u8 r = z80.a - s;

        z80.f.s = ((s8)r) < 0;
        z80.f.z = r == 0;
        z80.f.h = (s & 0xF) > (z80.a & 0xF); // overflow on lower half of reg
        z80.f.n = true;

        z80.f.b3 = ((r - z80.f.h) >> 3) & 1;
        z80.f.b5 = ((r - z80.f.h) >> 1) & 1;

        z80.hl.raw += hl_increment;
        z80.bc.raw--;

        z80.f.p_v = z80.bc.raw != 0;

        return 16;
    }

    template<int hl_increment>
    int instr_cpdr_cpir() {
        int cycles = instr_cpd_cpi<hl_increment>();
        if (get_register<Register::BC>() != 0 && !z80.f.z) {
            z80.pc -= 2;
            cycles += 5;
        }
        return cycles;
    }

    template <AddressingMode port, Register value>
    int instr_out() {
        z80.port_out(read_value<port, u8>(), get_register<value>());
        return 4;
    }

    template <Register port, Register value>
    int instr_out() {
        z80.port_out(get_register<port>(), get_register<value>());
        return 4;
    }

    int instr_outi() {
        u8 b = read_value<AddressingMode::HL, u8>();
        u8 port = get_register<Register::C>();
        z80.port_out(port, b);

        z80.hl.raw++;
        u8 reg_b = get_register<Register::B>() - 1;
        set_register<Register::B>(reg_b);

        return 16;
    }

    int instr_otir() {
        int cycles = instr_outi();

        if (get_register<Register::B>() != 0) {
            z80.pc -= 2; // repeat
            cycles += 5;
        }
        return cycles;
    }

    int instr_cb() {
        return cb_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_dd() {
        return dd_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_ddcb() {
        z80.prev_immediate = z80.read_byte(z80.pc++);
        return ddcb_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_ed() {
        return ed_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_fd() {
        return fd_instructions[z80.read_byte(z80.pc++)]();
    }

    int instr_fdcb() {
        z80.prev_immediate = z80.read_byte(z80.pc++);
        return fdcb_instructions[z80.read_byte(z80.pc++)]();
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


    int instr_ldd() {
        u8 value = z80.read_byte(z80.hl.raw);
        z80.write_byte(z80.de.raw, value);
        z80.hl.raw--;
        z80.de.raw--;
        z80.bc.raw--;

        z80.f.n = false;
        z80.f.h = false;
        z80.f.p_v = z80.bc.raw > 0;

        u8 r = value + z80.a;

        z80.f.b3 = ((r >> 3) & 1) == 1;
        z80.f.b5 = ((r >> 1) & 1) == 1;

        return 16;
    }

    int instr_lddr() {
        instr_ldd();

        if (z80.bc.raw) {
            z80.pc -= 2; // Repeat the instruction until BC is zero
            return 21;
        }
        return 16;
    }

    int instr_rla() {
        bool new_carry = (z80.a >> 7) & 1;
        z80.a <<= 1;
        z80.a |= (z80.f.c ? 1 : 0);
        z80.f.c = new_carry;
        return 4;
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

    int instr_di() {
        z80.interrupts_enabled = false;
        z80.next_interrupts_enabled = false;
        return 4;
    }

    int instr_ei() {
        z80.next_interrupts_enabled = true;
        return 4;
    }

    template<AddressingMode src, Register dst>
    int instr_rlc() {
        u16 address = get_address<src>();
        u8 value = z80.read_byte(address);
        u8 res = std::rotl(value, 1);

        set_register<dst>(res);
        z80.write_byte(address, res);
        z80.f.s = res & 1;
        z80.f.z = res == 0;
        z80.f.p_v = parity(res);
        z80.f.n = false;
        z80.f.h = false;
        z80.f.c = res & 1;
        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 23;
    }

    template<Register src>
    int instr_rlc() {
        u8 value = get_register<src>();
        u8 res = std::rotl(value, 1);
        set_register<src>(res);
        z80.f.s = res & 1;
        z80.f.z = res == 0;
        z80.f.p_v = parity(res);
        z80.f.n = false;
        z80.f.h = false;
        z80.f.c = res & 1;
        z80.f.b3 = (res >> 3) & 1;
        z80.f.b5 = (res >> 5) & 1;
        return 8;
    }

    template<AddressingMode src>
    int instr_rlc() {
        logfatal("rlc");
    }

    template<AddressingMode src, Register dst>
    int instr_rrc() {
        logfatal("rrc");
    }

    template<AddressingMode src>
    int instr_rrc() {
        logfatal("rrc");
    }

    template<Register src>
    int instr_rrc() {
        logfatal("rrc");
    }

    template<AddressingMode src, Register dst>
    int instr_rl() {
        logfatal("rl");
    }

    template<AddressingMode src>
    int instr_rl() {
        logfatal("rl");
    }

    template<Register src>
    int instr_rl() {
        logfatal("rl");
    }

    template<AddressingMode src, Register dst>
    int instr_rr() {
        logfatal("rr");
    }

    template<AddressingMode src>
    int instr_rr() {
        logfatal("rr");
    }

    template<Register src>
    int instr_rr() {
        logfatal("rr");
    }

    template<AddressingMode src, Register dst>
    int instr_sla() {
        logfatal("instr_sla");
    }

    template<AddressingMode src>
    int instr_sla() {
        logfatal("instr_sla");
    }

    template<Register src>
    int instr_sla() {
        logfatal("instr_sla");
    }

    template<AddressingMode src, Register dst>
    int instr_sra() {
        logfatal("instr_sra");
    }

    template<AddressingMode src>
    int instr_sra() {
        logfatal("instr_sra");
    }

    template<Register src>
    int instr_sra() {
        logfatal("instr_sra");
    }

    template<AddressingMode src, Register dst>
    int instr_sll() {
        logfatal("instr_sll");
    }

    template<AddressingMode src>
    int instr_sll() {
        logfatal("instr_sll");
    }

    template<Register src>
    int instr_sll() {
        logfatal("instr_sll");
    }

    template<AddressingMode src, Register dst>
    int instr_srl() {
        logfatal("instr_srl");
    }

    template<AddressingMode src>
    int instr_srl() {
        logfatal("instr_srl");
    }

    template<Register src>
    int instr_srl() {
        u8 value = get_register<src>();
        u8 res = value >> 1;
        set_register<src>(res);

        z80.f.c = value & 1;
        z80.f.n = false;
        z80.f.p_v = parity(res);
        z80.f.h = false;
        z80.f.s = ((s8)res) < 0;
        z80.f.z = res == 0;

        z80.f.b5 = (res >> 5) & 1;
        z80.f.b3 = (res >> 3) & 1;

        return 8;
    }

    template<int n, AddressingMode src>
    int instr_bit() {
        u16 addr = get_address<src>();
        u8 val = z80.read_byte(addr);
        u8 res = val & (1 << n);
        z80.f.s = ((s8)res) < 0;
        z80.f.z = res == 0;
        z80.f.b5 = (addr >> 5) & 1;
        z80.f.h = true;
        z80.f.b3 = (addr >> 3) & 1;
        z80.f.p_v = res == 0;
        z80.f.n = false;
        return 20;
    }

    template<int n, Register src>
    int instr_bit() {
        u8 val = get_register<src>();
        u8 res = val & (1 << n);
        z80.f.s = ((s8)res) < 0;
        z80.f.z = res == 0;
        z80.f.b5 = (val >> 5) & 1;
        z80.f.h = true;
        z80.f.b3 = (val >> 3) & 1;
        z80.f.p_v = res == 0;
        z80.f.n = false;
        return 20;
    }

    template<int n, AddressingMode src>
    int instr_res() {
        logfatal("instr_res");
    }

    template<u8 n, Register src>
    int instr_res() {
        u8 val = get_register<src>();
        val &= ~(1 << n);
        set_register<src>(val);
        return 8;
    }

    template<int n, AddressingMode src, Register reg>
    int instr_res() {
        logfatal("instr_res");
    }

    template<int n, AddressingMode src>
    int instr_set() {
        logfatal("instr_set");
    }

    template<int n, Register src>
    int instr_set() {
        logfatal("instr_set");
    }

    template<int n, AddressingMode src, Register reg>
    int instr_set() {
        logfatal("instr_set");
    }

    int instr_im_1() {
        Z80::z80.interrupt_mode = 1;
        return 8;
    }

    int instr_cpl() {
        z80.a = ~z80.a;
        z80.f.n = true;
        z80.f.h = true;
        z80.f.b5 = (z80.a >> 5) & 1;
        z80.f.b3 = (z80.a >> 3) & 1;
        return 4;
    }

    template<u16 offset>
    int instr_rst() {
        stack_push<u16>(z80.pc);
        z80.pc = offset;
        return 11;
    }

    int instr_rra() {
        bool new_carry = z80.a & 1;
        z80.a >>= 1;
        z80.a |= (z80.f.c ? 1 : 0) << 7;
        z80.f.c = new_carry;
        return 4;
    }

    int instr_daa() {
        u8 offset = 0;
        u8 lo4 = z80.a & 0xF;
        //u8 hi4 = (z80.a >> 4) & 0xF;

        if (z80.f.h || lo4 > 0x9) {
            offset = 0x6;
        }

        if (z80.f.c || z80.a > 0x99) {
            offset += 0x60;
            z80.f.c = true;
        }

        if (z80.f.n) {
            z80.f.h = z80.f.h && lo4 < 0x6;
            z80.a -= offset;
        } else {
            z80.f.h = lo4 > 9;
            z80.a += offset;
        }

        z80.f.s = ((s8)z80.a) < 0;
        z80.f.z = z80.a == 0;
        z80.f.p_v = parity(z80.a);
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
    }

    int instr_scf() {
        z80.f.c = true;
        z80.f.n = false;
        z80.f.h = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
    }

    int instr_ccf() {
        z80.f.h = z80.f.c;
        z80.f.c = !z80.f.c;
        z80.f.n = false;
        z80.f.b3 = (z80.a >> 3) & 1;
        z80.f.b5 = (z80.a >> 5) & 1;
        return 4;
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
            /* 12 */ instr_ld<AddressingMode::DE, Register::A>,
            /* 13 */ instr_inc<Register::DE>,
            /* 14 */ instr_inc<Register::D>,
            /* 15 */ instr_dec<Register::D>,
            /* 16 */ instr_ld<Register::D, AddressingMode::Immediate>,
            /* 17 */ instr_rla,
            /* 18 */ instr_jr<Condition::Always>,
            /* 19 */ instr_add<Register::HL, Register::DE>,
            /* 1A */ instr_ld<Register::A, AddressingMode::DE>,
            /* 1B */ instr_dec<Register::DE>,
            /* 1C */ instr_inc<Register::E>,
            /* 1D */ instr_dec<Register::E>,
            /* 1E */ instr_ld<Register::E, AddressingMode::Immediate>,
            /* 1F */ instr_rra,
            /* 20 */ instr_jr<Condition::NZ>,
            /* 21 */ instr_ld<Register::HL, AddressingMode::Immediate>,
            /* 22 */ instr_ld<AddressingMode::Indirect, Register::HL>,
            /* 23 */ instr_inc<Register::HL>,
            /* 24 */ instr_inc<Register::H>,
            /* 25 */ instr_dec<Register::H>,
            /* 26 */ instr_ld<Register::H, AddressingMode::Immediate>,
            /* 27 */ instr_daa,
            /* 28 */ instr_jr<Condition::Z>,
            /* 29 */ instr_add<Register::HL, Register::HL>,
            /* 2A */ instr_ld<Register::HL, AddressingMode::Indirect>,
            /* 2B */ instr_dec<Register::HL>,
            /* 2C */ instr_inc<Register::L>,
            /* 2D */ instr_dec<Register::L>,
            /* 2E */ instr_ld<Register::L, AddressingMode::Immediate>,
            /* 2F */ instr_cpl,
            /* 30 */ instr_jr<Condition::NC>,
            /* 31 */ instr_ld<Register::SP, AddressingMode::Immediate>,
            /* 32 */ instr_ld<AddressingMode::Indirect, Register::A>,
            /* 33 */ instr_inc<Register::SP>,
            /* 34 */ instr_inc<AddressingMode::HL>,
            /* 35 */ instr_dec<AddressingMode::HL>,
            /* 36 */ instr_ld<AddressingMode::HL, AddressingMode::Immediate>,
            /* 37 */ instr_scf,
            /* 38 */ instr_jr<Condition::C>,
            /* 39 */ instr_add<Register::HL, Register::SP>,
            /* 3A */ instr_ld<Register::A, AddressingMode::Indirect>,
            /* 3B */ instr_dec<Register::SP>,
            /* 3C */ instr_inc<Register::A>,
            /* 3D */ instr_dec<Register::A>,
            /* 3E */ instr_ld<Register::A, AddressingMode::Immediate>,
            /* 3F */ instr_ccf,
            /* 40 */ instr_ld<Register::B, Register::B>,
            /* 41 */ instr_ld<Register::B, Register::C>,
            /* 42 */ instr_ld<Register::B, Register::D>,
            /* 43 */ instr_ld<Register::B, Register::E>,
            /* 44 */ instr_ld<Register::B, Register::H>,
            /* 45 */ instr_ld<Register::B, Register::L>,
            /* 46 */ instr_ld<Register::B, AddressingMode::HL>,
            /* 47 */ instr_ld<Register::B, Register::A>,
            /* 48 */ instr_ld<Register::C, Register::B>,
            /* 49 */ instr_ld<Register::C, Register::C>,
            /* 4A */ instr_ld<Register::C, Register::D>,
            /* 4B */ instr_ld<Register::C, Register::E>,
            /* 4C */ instr_ld<Register::C, Register::H>,
            /* 4D */ instr_ld<Register::C, Register::L>,
            /* 4E */ instr_ld<Register::C, AddressingMode::HL>,
            /* 4F */ instr_ld<Register::C, Register::A>,
            /* 50 */ instr_ld<Register::D, Register::B>,
            /* 51 */ instr_ld<Register::D, Register::C>,
            /* 52 */ instr_ld<Register::D, Register::D>,
            /* 53 */ instr_ld<Register::D, Register::E>,
            /* 54 */ instr_ld<Register::D, Register::H>,
            /* 55 */ instr_ld<Register::D, Register::L>,
            /* 56 */ instr_ld<Register::D, AddressingMode::HL>,
            /* 57 */ instr_ld<Register::D, Register::A>,
            /* 58 */ instr_ld<Register::E, Register::B>,
            /* 59 */ instr_ld<Register::E, Register::C>,
            /* 5A */ instr_ld<Register::E, Register::D>,
            /* 5B */ instr_ld<Register::E, Register::E>,
            /* 5C */ instr_ld<Register::E, Register::H>,
            /* 5D */ instr_ld<Register::E, Register::L>,
            /* 5E */ instr_ld<Register::E, AddressingMode::HL>,
            /* 5F */ instr_ld<Register::E, Register::A>,
            /* 60 */ instr_ld<Register::H, Register::B>,
            /* 61 */ instr_ld<Register::H, Register::C>,
            /* 62 */ instr_ld<Register::H, Register::D>,
            /* 63 */ instr_ld<Register::H, Register::E>,
            /* 64 */ instr_ld<Register::H, Register::H>,
            /* 65 */ instr_ld<Register::H, Register::L>,
            /* 66 */ instr_ld<Register::H, AddressingMode::HL>,
            /* 67 */ instr_ld<Register::H, Register::A>,
            /* 68 */ instr_ld<Register::L, Register::B>,
            /* 69 */ instr_ld<Register::L, Register::C>,
            /* 6A */ instr_ld<Register::L, Register::D>,
            /* 6B */ instr_ld<Register::L, Register::E>,
            /* 6C */ instr_ld<Register::L, Register::H>,
            /* 6D */ instr_ld<Register::L, Register::L>,
            /* 6E */ instr_ld<Register::L, AddressingMode::HL>,
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
            /* 80 */ instr_add<Register::A, Register::B>,
            /* 81 */ instr_add<Register::A, Register::C>,
            /* 82 */ instr_add<Register::A, Register::D>,
            /* 83 */ instr_add<Register::A, Register::E>,
            /* 84 */ instr_add<Register::A, Register::H>,
            /* 85 */ instr_add<Register::A, Register::L>,
            /* 86 */ instr_add<Register::A, AddressingMode::HL>,
            /* 87 */ instr_add<Register::A, Register::A>,
            /* 88 */ instr_adc<Register::A, Register::B>,
            /* 89 */ instr_adc<Register::A, Register::C>,
            /* 8A */ instr_adc<Register::A, Register::D>,
            /* 8B */ instr_adc<Register::A, Register::E>,
            /* 8C */ instr_adc<Register::A, Register::H>,
            /* 8D */ instr_adc<Register::A, Register::L>,
            /* 8E */ instr_adc<Register::A, AddressingMode::HL>,
            /* 8F */ instr_adc<Register::A, Register::A>,
            /* 90 */ instr_sub<Register::B>,
            /* 91 */ instr_sub<Register::C>,
            /* 92 */ instr_sub<Register::D>,
            /* 93 */ instr_sub<Register::E>,
            /* 94 */ instr_sub<Register::H>,
            /* 95 */ instr_sub<Register::L>,
            /* 96 */ instr_sub<AddressingMode::HL>,
            /* 97 */ instr_sub<Register::A>,
            /* 98 */ instr_sbc<Register::A, Register::B>,
            /* 99 */ instr_sbc<Register::A, Register::C>,
            /* 9A */ instr_sbc<Register::A, Register::D>,
            /* 9B */ instr_sbc<Register::A, Register::E>,
            /* 9C */ instr_sbc<Register::A, Register::H>,
            /* 9D */ instr_sbc<Register::A, Register::L>,
            /* 9E */ instr_sbc<Register::A, AddressingMode::HL>,
            /* 9F */ instr_sbc<Register::A, Register::A>,
            /* A0 */ instr_and<Register::B>,
            /* A1 */ instr_and<Register::C>,
            /* A2 */ instr_and<Register::D>,
            /* A3 */ instr_and<Register::E>,
            /* A4 */ instr_and<Register::H>,
            /* A5 */ instr_and<Register::L>,
            /* A6 */ instr_and<AddressingMode::HL>,
            /* A7 */ instr_and<Register::A>,
            /* A8 */ instr_xor<Register::B>,
            /* A9 */ instr_xor<Register::C>,
            /* AA */ instr_xor<Register::D>,
            /* AB */ instr_xor<Register::E>,
            /* AC */ instr_xor<Register::H>,
            /* AD */ instr_xor<Register::L>,
            /* AE */ instr_xor<AddressingMode::HL>,
            /* AF */ instr_xor<Register::A>,
            /* B0 */ instr_or<Register::B>,
            /* B1 */ instr_or<Register::C>,
            /* B2 */ instr_or<Register::D>,
            /* B3 */ instr_or<Register::E>,
            /* B4 */ instr_or<Register::H>,
            /* B5 */ instr_or<Register::L>,
            /* B6 */ instr_or<AddressingMode::HL>,
            /* B7 */ instr_or<Register::A>,
            /* B8 */ instr_cp<Register::B>,
            /* B9 */ instr_cp<Register::C>,
            /* BA */ instr_cp<Register::D>,
            /* BB */ instr_cp<Register::E>,
            /* BC */ instr_cp<Register::H>,
            /* BD */ instr_cp<Register::L>,
            /* BE */ instr_cp<AddressingMode::HL>,
            /* BF */ instr_cp<Register::A>,
            /* C0 */ instr_ret<Condition::NZ>,
            /* C1 */ instr_pop<Register::BC>,
            /* C2 */ instr_jp<Condition::NZ, AddressingMode::Indirect>,
            /* C3 */ instr_jp<Condition::Always, AddressingMode::Indirect>,
            /* C4 */ instr_call<Condition::NZ>,
            /* C5 */ instr_push<Register::BC>,
            /* C6 */ instr_add<Register::A, AddressingMode::Immediate>,
            /* C7 */ unimplemented_instr<0xC7>,
            /* C8 */ instr_ret<Condition::Z>,
            /* C9 */ instr_ret<Condition::Always>,
            /* CA */ instr_jp<Condition::Z, AddressingMode::Indirect>,
            /* CB */ instr_cb,
            /* CC */ instr_call<Condition::Z>,
            /* CD */ instr_call<Condition::Always>,
            /* CE */ instr_adc<Register::A, AddressingMode::Immediate>,
            /* CF */ instr_rst<0x08>,
            /* D0 */ instr_ret<Condition::NC>,
            /* D1 */ instr_pop<Register::DE>,
            /* D2 */ instr_jp<Condition::NC, AddressingMode::Indirect>,
            /* D3 */ instr_out<AddressingMode::Immediate, Register::A>,
            /* D4 */ instr_call<Condition::NC>,
            /* D5 */ instr_push<Register::DE>,
            /* D6 */ instr_sub<AddressingMode::Immediate>,
            /* D7 */ instr_rst<0x10>,
            /* D8 */ instr_ret<Condition::C>,
            /* D9 */ instr_exx,
            /* DA */ instr_jp<Condition::C, AddressingMode::Indirect>,
            /* DB */ instr_in,
            /* DC */ instr_call<Condition::C>,
            /* DD */ instr_dd,
            /* DE */ instr_sbc<Register::A, AddressingMode::Immediate>,
            /* DF */ instr_rst<0x18>,
            /* E0 */ instr_ret<Condition::PO>,
            /* E1 */ instr_pop<Register::HL>,
            /* E2 */ instr_jp<Condition::PO, AddressingMode::Indirect>,
            /* E3 */ unimplemented_instr<0xE3>,
            /* E4 */ instr_call<Condition::PO>,
            /* E5 */ instr_push<Register::HL>,
            /* E6 */ instr_and<AddressingMode::Immediate>,
            /* E7 */ instr_rst<0x20>,
            /* E8 */ instr_ret<Condition::PE>,
            /* E9 */ instr_jp<Condition::Always, AddressingMode::HL>,
            /* EA */ instr_jp<Condition::PE, AddressingMode::Indirect>,
            /* EB */ instr_ex_de_hl,
            /* EC */ instr_call<Condition::PE>,
            /* ED */ instr_ed,
            /* EE */ instr_xor<AddressingMode::Immediate>,
            /* EF */ instr_rst<0x28>,
            /* F0 */ instr_ret<Condition::P>,
            /* F1 */ instr_pop<Register::AF>,
            /* F2 */ instr_jp<Condition::P, AddressingMode::Indirect>,
            /* F3 */ instr_di,
            /* F4 */ instr_call<Condition::P>,
            /* F5 */ instr_push<Register::AF>,
            /* F6 */ instr_or<AddressingMode::Immediate>,
            /* F7 */ instr_rst<0x30>,
            /* F8 */ instr_ret<Condition::M>,
            /* F9 */ instr_ld<Register::SP, Register::HL>,
            /* FA */ instr_jp<Condition::M, AddressingMode::Indirect>,
            /* FB */ instr_ei,
            /* FC */ instr_call<Condition::M>,
            /* FD */ instr_fd,
            /* FE */ instr_cp<AddressingMode::Immediate>,
            /* FF */ instr_rst<0x38>,
    };

    const instruction cb_instructions[0x100] = {
            /* CB 00 */ instr_rlc<Register::B>,
            /* CB 01 */ instr_rlc<Register::C>,
            /* CB 02 */ instr_rlc<Register::D>,
            /* CB 03 */ instr_rlc<Register::E>,
            /* CB 04 */ instr_rlc<Register::H>,
            /* CB 05 */ instr_rlc<Register::L>,
            /* CB 06 */ instr_rlc<AddressingMode::HL>,
            /* CB 07 */ instr_rlc<Register::A>,
            /* CB 08 */ instr_rrc<Register::B>,
            /* CB 09 */ instr_rrc<Register::C>,
            /* CB 0A */ instr_rrc<Register::D>,
            /* CB 0B */ instr_rrc<Register::E>,
            /* CB 0C */ instr_rrc<Register::H>,
            /* CB 0D */ instr_rrc<Register::L>,
            /* CB 0E */ instr_rrc<AddressingMode::HL>,
            /* CB 0F */ instr_rrc<Register::A>,
            /* CB 10 */ instr_rl<Register::B>,
            /* CB 11 */ instr_rl<Register::C>,
            /* CB 12 */ instr_rl<Register::D>,
            /* CB 13 */ instr_rl<Register::E>,
            /* CB 14 */ instr_rl<Register::H>,
            /* CB 15 */ instr_rl<Register::L>,
            /* CB 16 */ instr_rl<AddressingMode::HL>,
            /* CB 17 */ instr_rl<Register::A>,
            /* CB 18 */ instr_rr<Register::B>,
            /* CB 19 */ instr_rr<Register::C>,
            /* CB 1A */ instr_rr<Register::D>,
            /* CB 1B */ instr_rr<Register::E>,
            /* CB 1C */ instr_rr<Register::H>,
            /* CB 1D */ instr_rr<Register::L>,
            /* CB 1E */ instr_rr<AddressingMode::HL>,
            /* CB 1F */ instr_rr<AddressingMode::IXPlusPrevious, Register::A>,
            /* CB 20 */ instr_sla<Register::B>,
            /* CB 21 */ instr_sla<Register::C>,
            /* CB 22 */ instr_sla<Register::D>,
            /* CB 23 */ instr_sla<Register::E>,
            /* CB 24 */ instr_sla<Register::H>,
            /* CB 25 */ instr_sla<Register::L>,
            /* CB 26 */ instr_sla<AddressingMode::HL>,
            /* CB 27 */ instr_sla<Register::A>,
            /* CB 28 */ instr_sra<Register::B>,
            /* CB 29 */ instr_sra<Register::C>,
            /* CB 2A */ instr_sra<Register::D>,
            /* CB 2B */ instr_sra<Register::E>,
            /* CB 2C */ instr_sra<Register::H>,
            /* CB 2D */ instr_sra<Register::L>,
            /* CB 2E */ instr_sra<AddressingMode::HL>,
            /* CB 2F */ instr_sra<Register::A>,
            /* CB 30 */ instr_sll<Register::B>,
            /* CB 31 */ instr_sll<Register::C>,
            /* CB 32 */ instr_sll<Register::D>,
            /* CB 33 */ instr_sll<Register::E>,
            /* CB 34 */ instr_sll<Register::H>,
            /* CB 35 */ instr_sll<Register::L>,
            /* CB 36 */ instr_sll<AddressingMode::HL>,
            /* CB 37 */ instr_sll<Register::A>,
            /* CB 38 */ instr_srl<Register::B>,
            /* CB 39 */ instr_srl<Register::C>,
            /* CB 3A */ instr_srl<Register::D>,
            /* CB 3B */ instr_srl<Register::E>,
            /* CB 3C */ instr_srl<Register::H>,
            /* CB 3D */ instr_srl<Register::L>,
            /* CB 3E */ instr_srl<AddressingMode::HL>,
            /* CB 3F */ instr_srl<Register::A>,
            /* CB 40 */ instr_bit<0, Register::B>,
            /* CB 41 */ instr_bit<0, Register::C>,
            /* CB 42 */ instr_bit<0, Register::D>,
            /* CB 43 */ instr_bit<0, Register::E>,
            /* CB 44 */ instr_bit<0, Register::H>,
            /* CB 45 */ instr_bit<0, Register::L>,
            /* CB 46 */ instr_bit<0, AddressingMode::HL>,
            /* CB 47 */ instr_bit<0, Register::A>,
            /* CB 48 */ instr_bit<1, Register::B>,
            /* CB 49 */ instr_bit<1, Register::C>,
            /* CB 4A */ instr_bit<1, Register::D>,
            /* CB 4B */ instr_bit<1, Register::E>,
            /* CB 4C */ instr_bit<1, Register::H>,
            /* CB 4D */ instr_bit<1, Register::L>,
            /* CB 4E */ instr_bit<1, AddressingMode::HL>,
            /* CB 4F */ instr_bit<1, Register::A>,
            /* CB 50 */ instr_bit<2, Register::B>,
            /* CB 51 */ instr_bit<2, Register::C>,
            /* CB 52 */ instr_bit<2, Register::D>,
            /* CB 53 */ instr_bit<2, Register::E>,
            /* CB 54 */ instr_bit<2, Register::H>,
            /* CB 55 */ instr_bit<2, Register::L>,
            /* CB 56 */ instr_bit<2, AddressingMode::HL>,
            /* CB 57 */ instr_bit<2, Register::A>,
            /* CB 58 */ instr_bit<3, Register::B>,
            /* CB 59 */ instr_bit<3, Register::C>,
            /* CB 5A */ instr_bit<3, Register::D>,
            /* CB 5B */ instr_bit<3, Register::E>,
            /* CB 5C */ instr_bit<3, Register::H>,
            /* CB 5D */ instr_bit<3, Register::L>,
            /* CB 5E */ instr_bit<3, AddressingMode::HL>,
            /* CB 5F */ instr_bit<3, Register::A>,
            /* CB 60 */ instr_bit<4, Register::B>,
            /* CB 61 */ instr_bit<4, Register::C>,
            /* CB 62 */ instr_bit<4, Register::D>,
            /* CB 63 */ instr_bit<4, Register::E>,
            /* CB 64 */ instr_bit<4, Register::H>,
            /* CB 65 */ instr_bit<4, Register::L>,
            /* CB 66 */ instr_bit<4, AddressingMode::HL>,
            /* CB 67 */ instr_bit<4, Register::A>,
            /* CB 68 */ instr_bit<5, Register::B>,
            /* CB 69 */ instr_bit<5, Register::C>,
            /* CB 6A */ instr_bit<5, Register::D>,
            /* CB 6B */ instr_bit<5, Register::E>,
            /* CB 6C */ instr_bit<5, Register::H>,
            /* CB 6D */ instr_bit<5, Register::L>,
            /* CB 6E */ instr_bit<5, AddressingMode::HL>,
            /* CB 6F */ instr_bit<5, Register::A>,
            /* CB 70 */ instr_bit<6, Register::B>,
            /* CB 71 */ instr_bit<6, Register::C>,
            /* CB 72 */ instr_bit<6, Register::D>,
            /* CB 73 */ instr_bit<6, Register::E>,
            /* CB 74 */ instr_bit<6, Register::H>,
            /* CB 75 */ instr_bit<6, Register::L>,
            /* CB 76 */ instr_bit<6, AddressingMode::HL>,
            /* CB 77 */ instr_bit<6, Register::A>,
            /* CB 78 */ instr_bit<7, Register::B>,
            /* CB 79 */ instr_bit<7, Register::C>,
            /* CB 7A */ instr_bit<7, Register::D>,
            /* CB 7B */ instr_bit<7, Register::E>,
            /* CB 7C */ instr_bit<7, Register::H>,
            /* CB 7D */ instr_bit<7, Register::L>,
            /* CB 7E */ instr_bit<7, AddressingMode::HL>,
            /* CB 7F */ instr_bit<7, Register::A>,
            /* CB 80 */ instr_res<0, Register::B>,
            /* CB 81 */ instr_res<0, Register::C>,
            /* CB 82 */ instr_res<0, Register::D>,
            /* CB 83 */ instr_res<0, Register::E>,
            /* CB 84 */ instr_res<0, Register::H>,
            /* CB 85 */ instr_res<0, Register::L>,
            /* CB 86 */ instr_res<0, AddressingMode::HL>,
            /* CB 87 */ instr_res<0, Register::A>,
            /* CB 88 */ instr_res<1, Register::B>,
            /* CB 89 */ instr_res<1, Register::C>,
            /* CB 8A */ instr_res<1, Register::D>,
            /* CB 8B */ instr_res<1, Register::E>,
            /* CB 8C */ instr_res<1, Register::H>,
            /* CB 8D */ instr_res<1, Register::L>,
            /* CB 8E */ instr_res<1, AddressingMode::HL>,
            /* CB 8F */ instr_res<1, Register::A>,
            /* CB 90 */ instr_res<2, Register::B>,
            /* CB 91 */ instr_res<2, Register::C>,
            /* CB 92 */ instr_res<2, Register::D>,
            /* CB 93 */ instr_res<2, Register::E>,
            /* CB 94 */ instr_res<2, Register::H>,
            /* CB 95 */ instr_res<2, Register::L>,
            /* CB 96 */ instr_res<2, AddressingMode::HL>,
            /* CB 97 */ instr_res<2, Register::A>,
            /* CB 98 */ instr_res<3, Register::B>,
            /* CB 99 */ instr_res<3, Register::C>,
            /* CB 9A */ instr_res<3, Register::D>,
            /* CB 9B */ instr_res<3, Register::E>,
            /* CB 9C */ instr_res<3, Register::H>,
            /* CB 9D */ instr_res<3, Register::L>,
            /* CB 9E */ instr_res<3, AddressingMode::HL>,
            /* CB 9F */ instr_res<3, Register::A>,
            /* CB A0 */ instr_res<4, Register::B>,
            /* CB A1 */ instr_res<4, Register::C>,
            /* CB A2 */ instr_res<4, Register::D>,
            /* CB A3 */ instr_res<4, Register::E>,
            /* CB A4 */ instr_res<4, Register::H>,
            /* CB A5 */ instr_res<4, Register::L>,
            /* CB A6 */ instr_res<4, AddressingMode::HL>,
            /* CB A7 */ instr_res<4, Register::A>,
            /* CB A8 */ instr_res<5, Register::B>,
            /* CB A9 */ instr_res<5, Register::C>,
            /* CB AA */ instr_res<5, Register::D>,
            /* CB AB */ instr_res<5, Register::E>,
            /* CB AC */ instr_res<5, Register::H>,
            /* CB AD */ instr_res<5, Register::L>,
            /* CB AE */ instr_res<5, AddressingMode::HL>,
            /* CB AF */ instr_res<5, Register::A>,
            /* CB B0 */ instr_res<6, Register::B>,
            /* CB B1 */ instr_res<6, Register::C>,
            /* CB B2 */ instr_res<6, Register::D>,
            /* CB B3 */ instr_res<6, Register::E>,
            /* CB B4 */ instr_res<6, Register::H>,
            /* CB B5 */ instr_res<6, Register::L>,
            /* CB B6 */ instr_res<6, AddressingMode::HL>,
            /* CB B7 */ instr_res<6, Register::A>,
            /* CB B8 */ instr_res<7, Register::B>,
            /* CB B9 */ instr_res<7, Register::C>,
            /* CB BA */ instr_res<7, Register::D>,
            /* CB BB */ instr_res<7, Register::E>,
            /* CB BC */ instr_res<7, Register::H>,
            /* CB BD */ instr_res<7, Register::L>,
            /* CB BE */ instr_res<7, AddressingMode::HL>,
            /* CB BF */ instr_res<7, Register::A>,
            /* CB C0 */ instr_set<0, Register::B>,
            /* CB C1 */ instr_set<0, Register::C>,
            /* CB C2 */ instr_set<0, Register::D>,
            /* CB C3 */ instr_set<0, Register::E>,
            /* CB C4 */ instr_set<0, Register::H>,
            /* CB C5 */ instr_set<0, Register::L>,
            /* CB C6 */ instr_set<0, AddressingMode::HL>,
            /* CB C7 */ instr_set<0, Register::A>,
            /* CB C8 */ instr_set<1, Register::B>,
            /* CB C9 */ instr_set<1, Register::C>,
            /* CB CA */ instr_set<1, Register::D>,
            /* CB CB */ instr_set<1, Register::E>,
            /* CB CC */ instr_set<1, Register::H>,
            /* CB CD */ instr_set<1, Register::L>,
            /* CB CE */ instr_set<1, AddressingMode::HL>,
            /* CB CF */ instr_set<1, Register::A>,
            /* CB D0 */ instr_set<2, Register::B>,
            /* CB D1 */ instr_set<2, Register::C>,
            /* CB D2 */ instr_set<2, Register::D>,
            /* CB D3 */ instr_set<2, Register::E>,
            /* CB D4 */ instr_set<2, Register::H>,
            /* CB D5 */ instr_set<2, Register::L>,
            /* CB D6 */ instr_set<2, AddressingMode::HL>,
            /* CB D7 */ instr_set<2, Register::A>,
            /* CB D8 */ instr_set<3, Register::B>,
            /* CB D9 */ instr_set<3, Register::C>,
            /* CB DA */ instr_set<3, Register::D>,
            /* CB DB */ instr_set<3, Register::E>,
            /* CB DC */ instr_set<3, Register::H>,
            /* CB DD */ instr_set<3, Register::L>,
            /* CB DE */ instr_set<3, AddressingMode::HL>,
            /* CB DF */ instr_set<3, Register::A>,
            /* CB E0 */ instr_set<4, Register::B>,
            /* CB E1 */ instr_set<4, Register::C>,
            /* CB E2 */ instr_set<4, Register::D>,
            /* CB E3 */ instr_set<4, Register::E>,
            /* CB E4 */ instr_set<4, Register::H>,
            /* CB E5 */ instr_set<4, Register::L>,
            /* CB E6 */ instr_set<4, AddressingMode::HL>,
            /* CB E7 */ instr_set<4, Register::A>,
            /* CB E8 */ instr_set<5, Register::B>,
            /* CB E9 */ instr_set<5, Register::C>,
            /* CB EA */ instr_set<5, Register::D>,
            /* CB EB */ instr_set<5, Register::E>,
            /* CB EC */ instr_set<5, Register::H>,
            /* CB ED */ instr_set<5, Register::L>,
            /* CB EE */ instr_set<5, AddressingMode::HL>,
            /* CB EF */ instr_set<5, Register::A>,
            /* CB F0 */ instr_set<6, Register::B>,
            /* CB F1 */ instr_set<6, Register::C>,
            /* CB F2 */ instr_set<6, Register::D>,
            /* CB F3 */ instr_set<6, Register::E>,
            /* CB F4 */ instr_set<6, Register::H>,
            /* CB F5 */ instr_set<6, Register::L>,
            /* CB F6 */ instr_set<6, AddressingMode::HL>,
            /* CB F7 */ instr_set<6, Register::A>,
            /* CB F8 */ instr_set<7, Register::B>,
            /* CB F9 */ instr_set<7, Register::C>,
            /* CB FA */ instr_set<7, Register::D>,
            /* CB FB */ instr_set<7, Register::E>,
            /* CB FC */ instr_set<7, Register::H>,
            /* CB FD */ instr_set<7, Register::L>,
            /* CB FE */ instr_set<7, AddressingMode::HL>,
            /* CB FF */ instr_set<7, Register::A>
    };

    const instruction dd_instructions[0x100] = {
            /* DD 00 */ instructions[0x00],
            /* DD 01 */ instructions[0x01],
            /* DD 02 */ instructions[0x02],
            /* DD 03 */ instructions[0x03],
            /* DD 04 */ instructions[0x04],
            /* DD 05 */ instructions[0x05],
            /* DD 06 */ instructions[0x06],
            /* DD 07 */ instructions[0x07],
            /* DD 08 */ instructions[0x08],
            /* DD 09 */ instr_add<Register::IX, Register::BC>,
            /* DD 0A */ instructions[0x0A],
            /* DD 0B */ instructions[0x0B],
            /* DD 0C */ instructions[0x0C],
            /* DD 0D */ instructions[0x0D],
            /* DD 0E */ instructions[0x0E],
            /* DD 0F */ instructions[0x0F],
            /* DD 10 */ instructions[0x10],
            /* DD 11 */ instructions[0x11],
            /* DD 12 */ instructions[0x12],
            /* DD 13 */ instructions[0x13],
            /* DD 14 */ instructions[0x14],
            /* DD 15 */ instructions[0x15],
            /* DD 16 */ instructions[0x16],
            /* DD 17 */ instructions[0x17],
            /* DD 18 */ instructions[0x18],
            /* DD 19 */ instr_add<Register::IX, Register::DE>,
            /* DD 1A */ instructions[0x1A],
            /* DD 1B */ instructions[0x1B],
            /* DD 1C */ instructions[0x1C],
            /* DD 1D */ instructions[0x1D],
            /* DD 1E */ instructions[0x1E],
            /* DD 1F */ instructions[0x1F],
            /* DD 20 */ instructions[0x20],
            /* DD 21 */ instr_ld<Register::IX, AddressingMode::Immediate>,
            /* DD 22 */ instr_ld<AddressingMode::Indirect, Register::IX>,
            /* DD 23 */ instr_inc<Register::IX>,
            /* DD 24 */ instr_inc<Register::IXH>,
            /* DD 25 */ instr_dec<Register::IXH>,
            /* DD 26 */ instr_ld<Register::IXH, AddressingMode::Immediate>,
            /* DD 27 */ instructions[0x27],
            /* DD 28 */ instructions[0x28],
            /* DD 29 */ instr_add<Register::IX, Register::IX>,
            /* DD 2A */ instr_ld<Register::IX, AddressingMode::Indirect>,
            /* DD 2B */ instr_dec<Register::IX>,
            /* DD 2C */ instr_inc<Register::IXL>,
            /* DD 2D */ instr_dec<Register::IXL>,
            /* DD 2E */ instr_ld<Register::IXL, AddressingMode::Immediate>,
            /* DD 2F */ instructions[0x2F],
            /* DD 30 */ instructions[0x30],
            /* DD 31 */ instructions[0x31],
            /* DD 32 */ instructions[0x32],
            /* DD 33 */ instructions[0x33],
            /* DD 34 */ instr_inc<AddressingMode::IXPlus>,
            /* DD 35 */ instr_dec<AddressingMode::IXPlus>,
            /* DD 36 */ instr_ld<AddressingMode::IXPlus, AddressingMode::Immediate>,
            /* DD 37 */ instructions[0x37],
            /* DD 38 */ instructions[0x38],
            /* DD 39 */ instr_add<Register::IX, Register::SP>,
            /* DD 3A */ instructions[0x3A],
            /* DD 3B */ instructions[0x3B],
            /* DD 3C */ instructions[0x3C],
            /* DD 3D */ instructions[0x3D],
            /* DD 3E */ instructions[0x3E],
            /* DD 3F */ instructions[0x3F],
            /* DD 40 */ instructions[0x40],
            /* DD 41 */ instructions[0x41],
            /* DD 42 */ instructions[0x42],
            /* DD 43 */ instructions[0x43],
            /* DD 44 */ instr_ld<Register::B, Register::IXH>,
            /* DD 45 */ instr_ld<Register::B, Register::IXL>,
            /* DD 46 */ instr_ld<Register::B, AddressingMode::IXPlus>,
            /* DD 47 */ instructions[0x47],
            /* DD 48 */ instructions[0x48],
            /* DD 49 */ instructions[0x49],
            /* DD 4A */ instructions[0x4A],
            /* DD 4B */ instructions[0x4B],
            /* DD 4C */ instr_ld<Register::C, Register::IXH>,
            /* DD 4D */ instr_ld<Register::C, Register::IXL>,
            /* DD 4E */ instr_ld<Register::C, AddressingMode::IXPlus>,
            /* DD 4F */ instructions[0x4F],
            /* DD 50 */ instructions[0x50],
            /* DD 51 */ instructions[0x51],
            /* DD 52 */ instructions[0x52],
            /* DD 53 */ instructions[0x53],
            /* DD 54 */ instr_ld<Register::D, Register::IXH>,
            /* DD 55 */ instr_ld<Register::D, Register::IXL>,
            /* DD 56 */ instr_ld<Register::D, AddressingMode::IXPlus>,
            /* DD 57 */ instructions[0x57],
            /* DD 58 */ instructions[0x58],
            /* DD 59 */ instructions[0x59],
            /* DD 5A */ instructions[0x5A],
            /* DD 5B */ instructions[0x5B],
            /* DD 5C */ instr_ld<Register::E, Register::IXH>,
            /* DD 5D */ instr_ld<Register::E, Register::IXL>,
            /* DD 5E */ instr_ld<Register::E, AddressingMode::IXPlus>,
            /* DD 5F */ instructions[0x5F],
            /* DD 60 */ instr_ld<Register::IXH, Register::B>,
            /* DD 61 */ instr_ld<Register::IXH, Register::C>,
            /* DD 62 */ instr_ld<Register::IXH, Register::D>,
            /* DD 63 */ instr_ld<Register::IXH, Register::E>,
            /* DD 64 */ instr_ld<Register::IXH, Register::IXH>,
            /* DD 65 */ instr_ld<Register::IXH, Register::IXL>,
            /* DD 66 */ instr_ld<Register::H, AddressingMode::IXPlus>,
            /* DD 67 */ instr_ld<Register::IXH, Register::A>,
            /* DD 68 */ instr_ld<Register::IXL, Register::B>,
            /* DD 69 */ instr_ld<Register::IXL, Register::C>,
            /* DD 6A */ instr_ld<Register::IXL, Register::D>,
            /* DD 6B */ instr_ld<Register::IXL, Register::E>,
            /* DD 6C */ instr_ld<Register::IXL, Register::IXH>,
            /* DD 6D */ instr_ld<Register::IXL, Register::IXL>,
            /* DD 6E */ instr_ld<Register::L, AddressingMode::IXPlus>,
            /* DD 6F */ instr_ld<Register::IXL, Register::A>,
            /* DD 70 */ instr_ld<AddressingMode::IXPlus, Register::B>,
            /* DD 71 */ instr_ld<AddressingMode::IXPlus, Register::C>,
            /* DD 72 */ instr_ld<AddressingMode::IXPlus, Register::D>,
            /* DD 73 */ instr_ld<AddressingMode::IXPlus, Register::E>,
            /* DD 74 */ instr_ld<AddressingMode::IXPlus, Register::H>,
            /* DD 75 */ instr_ld<AddressingMode::IXPlus, Register::L>,
            /* DD 76 */ instructions[0x76],
            /* DD 77 */ instr_ld<AddressingMode::IXPlus, Register::A>,
            /* DD 78 */ instructions[0x78],
            /* DD 79 */ instructions[0x79],
            /* DD 7A */ instructions[0x7A],
            /* DD 7B */ instructions[0x7B],
            /* DD 7C */ instr_ld<Register::A, Register::IXH>,
            /* DD 7D */ instr_ld<Register::A, Register::IXL>,
            /* DD 7E */ instr_ld<Register::A, AddressingMode::IXPlus>,
            /* DD 7F */ instructions[0x7F],
            /* DD 80 */ instructions[0x80],
            /* DD 81 */ instructions[0x81],
            /* DD 82 */ instructions[0x82],
            /* DD 83 */ instructions[0x83],
            /* DD 84 */ instr_add<Register::A, Register::IXH>,
            /* DD 85 */ instr_add<Register::A, Register::IXL>,
            /* DD 86 */ instr_add<Register::A, AddressingMode::IXPlus>,
            /* DD 87 */ instructions[0x87],
            /* DD 88 */ instructions[0x88],
            /* DD 89 */ instructions[0x89],
            /* DD 8A */ instructions[0x8A],
            /* DD 8B */ instructions[0x8B],
            /* DD 8C */ instr_adc<Register::A, Register::IXH>,
            /* DD 8D */ instr_adc<Register::A, Register::IXL>,
            /* DD 8E */ instr_adc<Register::A, AddressingMode::IXPlus>,
            /* DD 8F */ instructions[0x8F],
            /* DD 90 */ instructions[0x90],
            /* DD 91 */ instructions[0x91],
            /* DD 92 */ instructions[0x92],
            /* DD 93 */ instructions[0x93],
            /* DD 94 */ instr_sub<Register::IXH>,
            /* DD 95 */ instr_sub<Register::IXL>,
            /* DD 96 */ instr_sub<AddressingMode::IXPlus>,
            /* DD 97 */ instructions[0x97],
            /* DD 98 */ instructions[0x98],
            /* DD 99 */ instructions[0x99],
            /* DD 9A */ instructions[0x9A],
            /* DD 9B */ instructions[0x9B],
            /* DD 9C */ instr_sbc<Register::A, Register::IXH>,
            /* DD 9D */ instr_sbc<Register::A, Register::IXL>,
            /* DD 9E */ instr_sbc<Register::A, AddressingMode::IXPlus>,
            /* DD 9F */ instructions[0x9F],
            /* DD A0 */ instructions[0xA0],
            /* DD A1 */ instructions[0xA1],
            /* DD A2 */ instructions[0xA2],
            /* DD A3 */ instructions[0xA3],
            /* DD A4 */ instr_and<Register::IXH>,
            /* DD A5 */ instr_and<Register::IXL>,
            /* DD A6 */ instr_and<AddressingMode::IXPlus>,
            /* DD A7 */ instructions[0xA7],
            /* DD A8 */ instructions[0xA8],
            /* DD A9 */ instructions[0xA9],
            /* DD AA */ instructions[0xAA],
            /* DD AB */ instructions[0xAB],
            /* DD AC */ instr_xor<Register::IXH>,
            /* DD AD */ instr_xor<Register::IXL>,
            /* DD AE */ instr_xor<AddressingMode::IXPlus>,
            /* DD AF */ instructions[0xAF],
            /* DD B0 */ instructions[0xB0],
            /* DD B1 */ instructions[0xB1],
            /* DD B2 */ instructions[0xB2],
            /* DD B3 */ instructions[0xB3],
            /* DD B4 */ instr_or<Register::IXH>,
            /* DD B5 */ instr_or<Register::IXL>,
            /* DD B6 */ instr_or<AddressingMode::IXPlus>,
            /* DD B7 */ instructions[0xB7],
            /* DD B8 */ instructions[0xB8],
            /* DD B9 */ instructions[0xB9],
            /* DD BA */ instructions[0xBA],
            /* DD BB */ instructions[0xBB],
            /* DD BC */ instr_cp<Register::IXH>,
            /* DD BD */ instr_cp<Register::IXL>,
            /* DD BE */ instr_cp<AddressingMode::IXPlus>,
            /* DD BF */ instructions[0xBF],
            /* DD C0 */ instructions[0xC0],
            /* DD C1 */ instructions[0xC1],
            /* DD C2 */ instructions[0xC2],
            /* DD C3 */ instructions[0xC3],
            /* DD C4 */ instructions[0xC4],
            /* DD C5 */ instructions[0xC5],
            /* DD C6 */ instructions[0xC6],
            /* DD C7 */ instructions[0xC7],
            /* DD C8 */ instructions[0xC8],
            /* DD C9 */ instructions[0xC9],
            /* DD CA */ instructions[0xCA],
            /* DD CB */ instr_ddcb,
            /* DD CC */ instructions[0xCC],
            /* DD CD */ instructions[0xCD],
            /* DD CE */ instructions[0xCE],
            /* DD CF */ instructions[0xCF],
            /* DD D0 */ instructions[0xD0],
            /* DD D1 */ instructions[0xD1],
            /* DD D2 */ instructions[0xD2],
            /* DD D3 */ instructions[0xD3],
            /* DD D4 */ instructions[0xD4],
            /* DD D5 */ instructions[0xD5],
            /* DD D6 */ instructions[0xD6],
            /* DD D7 */ instructions[0xD7],
            /* DD D8 */ instructions[0xD8],
            /* DD D9 */ instructions[0xD9],
            /* DD DA */ instructions[0xDA],
            /* DD DB */ instructions[0xDB],
            /* DD DC */ instructions[0xDC],
            /* DD DD */ unimplemented_dd_instr<0xDD>,
            /* DD DE */ instructions[0xDE],
            /* DD DF */ instructions[0xDF],
            /* DD E0 */ instructions[0xE0],
            /* DD E1 */ instr_pop<Register::IX>,
            /* DD E2 */ instructions[0xE2],
            /* DD E3 */ unimplemented_dd_instr<0xE3>, // ex (sp), ix
            /* DD E4 */ instructions[0xE4],
            /* DD E5 */ instr_push<Register::IX>,
            /* DD E6 */ instructions[0xE6],
            /* DD E7 */ instructions[0xE7],
            /* DD E8 */ instructions[0xE8],
            /* DD E9 */ instr_jp<Condition::Always, AddressingMode::IX>,
            /* DD EA */ instructions[0xEA],
            /* DD EB */ instructions[0xEB],
            /* DD EC */ instructions[0xEC],
            /* DD ED */ instructions[0xED],
            /* DD EE */ instructions[0xEE],
            /* DD EF */ instructions[0xEF],
            /* DD F0 */ instructions[0xF0],
            /* DD F1 */ instructions[0xF1],
            /* DD F2 */ instructions[0xF2],
            /* DD F3 */ instructions[0xF3],
            /* DD F4 */ instructions[0xF4],
            /* DD F5 */ instructions[0xF5],
            /* DD F6 */ instructions[0xF6],
            /* DD F7 */ instructions[0xF7],
            /* DD F8 */ instructions[0xF8],
            /* DD F9 */ instructions[0xF9],
            /* DD FA */ unimplemented_dd_instr<0xFA>,
            /* DD FB */ instructions[0xFB],
            /* DD FC */ instructions[0xFC],
            /* DD FD */ instructions[0xFD],
            /* DD FE */ instructions[0xFE],
            /* DD FF */ instructions[0xFF],
    };

    const instruction ddcb_instructions[0x100] = {
            /* DD CB 00 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 01 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 02 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 03 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 04 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 05 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 06 */ instr_rlc<AddressingMode::IXPlusPrevious>,
            /* DD CB 07 */ instr_rlc<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 08 */ instr_rrc<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 09 */ instr_rrc<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 0A */ instr_rrc<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 0B */ instr_rrc<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 0C */ instr_rrc<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 0D */ instr_rrc<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 0E */ instr_rrc<AddressingMode::IXPlusPrevious>,
            /* DD CB 0F */ instr_rrc<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 10 */ instr_rl<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 11 */ instr_rl<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 12 */ instr_rl<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 13 */ instr_rl<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 14 */ instr_rl<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 15 */ instr_rl<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 16 */ instr_rl<AddressingMode::IXPlusPrevious>,
            /* DD CB 17 */ instr_rl<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 18 */ instr_rr<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 19 */ instr_rr<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 1A */ instr_rr<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 1B */ instr_rr<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 1C */ instr_rr<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 1D */ instr_rr<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 1E */ instr_rr<AddressingMode::IXPlusPrevious>,
            /* DD CB 1F */ instr_rr<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 20 */ instr_sla<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 21 */ instr_sla<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 22 */ instr_sla<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 23 */ instr_sla<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 24 */ instr_sla<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 25 */ instr_sla<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 26 */ instr_sla<AddressingMode::IXPlusPrevious>,
            /* DD CB 27 */ instr_sla<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 28 */ instr_sra<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 29 */ instr_sra<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 2A */ instr_sra<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 2B */ instr_sra<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 2C */ instr_sra<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 2D */ instr_sra<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 2E */ instr_sra<AddressingMode::IXPlusPrevious>,
            /* DD CB 2F */ instr_sra<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 30 */ instr_sll<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 31 */ instr_sll<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 32 */ instr_sll<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 33 */ instr_sll<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 34 */ instr_sll<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 35 */ instr_sll<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 36 */ instr_sll<AddressingMode::IXPlusPrevious>,
            /* DD CB 37 */ instr_sll<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 38 */ instr_srl<AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 39 */ instr_srl<AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 3A */ instr_srl<AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 3B */ instr_srl<AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 3C */ instr_srl<AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 3D */ instr_srl<AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 3E */ instr_srl<AddressingMode::IXPlusPrevious>,
            /* DD CB 3F */ instr_srl<AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 40 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 41 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 42 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 43 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 44 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 45 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 46 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 47 */ instr_bit<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 48 */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 49 */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4A */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4B */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4C */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4D */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4E */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 4F */ instr_bit<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 50 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 51 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 52 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 53 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 54 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 55 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 56 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 57 */ instr_bit<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 58 */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 59 */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5A */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5B */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5C */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5D */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5E */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 5F */ instr_bit<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 60 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 61 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 62 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 63 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 64 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 65 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 66 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 67 */ instr_bit<4, AddressingMode::IXPlusPrevious>,
            /* DD CB 68 */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 69 */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6A */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6B */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6C */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6D */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6E */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 6F */ instr_bit<5, AddressingMode::IXPlusPrevious>,
            /* DD CB 70 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 71 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 72 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 73 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 74 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 75 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 76 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 77 */ instr_bit<6, AddressingMode::IXPlusPrevious>,
            /* DD CB 78 */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 79 */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7A */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7B */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7C */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7D */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7E */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 7F */ instr_bit<7, AddressingMode::IXPlusPrevious>,
            /* DD CB 80 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 81 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 82 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 83 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 84 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 85 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 86 */ instr_res<0, AddressingMode::IXPlusPrevious>,
            /* DD CB 87 */ instr_res<0, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 88 */ instr_res<1, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 89 */ instr_res<1, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 8A */ instr_res<1, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 8B */ instr_res<1, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 8C */ instr_res<1, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 8D */ instr_res<1, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 8E */ instr_res<1, AddressingMode::IXPlusPrevious>,
            /* DD CB 8F */ instr_res<1, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 90 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 91 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 92 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 93 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 94 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 95 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 96 */ instr_res<2, AddressingMode::IXPlusPrevious>,
            /* DD CB 97 */ instr_res<2, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB 98 */ instr_res<3, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB 99 */ instr_res<3, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB 9A */ instr_res<3, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB 9B */ instr_res<3, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB 9C */ instr_res<3, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB 9D */ instr_res<3, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB 9E */ instr_res<3, AddressingMode::IXPlusPrevious>,
            /* DD CB 9F */ instr_res<3, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB A0 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB A1 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB A2 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB A3 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB A4 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB A5 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB A6 */ instr_res<4, AddressingMode::IXPlusPrevious>,
            /* DD CB A7 */ instr_res<4, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB A8 */ instr_res<5, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB A9 */ instr_res<5, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB AA */ instr_res<5, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB AB */ instr_res<5, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB AC */ instr_res<5, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB AD */ instr_res<5, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB AE */ instr_res<5, AddressingMode::IXPlusPrevious>,
            /* DD CB AF */ instr_res<5, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB B0 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB B1 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB B2 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB B3 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB B4 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB B5 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB B6 */ instr_res<6, AddressingMode::IXPlusPrevious>,
            /* DD CB B7 */ instr_res<6, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB B8 */ instr_res<7, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB B9 */ instr_res<7, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB BA */ instr_res<7, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB BB */ instr_res<7, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB BC */ instr_res<7, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB BD */ instr_res<7, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB BE */ instr_res<7, AddressingMode::IXPlusPrevious>,
            /* DD CB BF */ instr_res<7, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB C0 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB C1 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB C2 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB C3 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB C4 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB C5 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB C6 */ instr_set<0, AddressingMode::IXPlusPrevious>,
            /* DD CB C7 */ instr_set<0, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB C8 */ instr_set<1, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB C9 */ instr_set<1, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB CA */ instr_set<1, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB CB */ instr_set<1, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB CC */ instr_set<1, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB CD */ instr_set<1, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB CE */ instr_set<1, AddressingMode::IXPlusPrevious>,
            /* DD CB CF */ instr_set<1, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB D0 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB D1 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB D2 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB D3 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB D4 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB D5 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB D6 */ instr_set<2, AddressingMode::IXPlusPrevious>,
            /* DD CB D7 */ instr_set<2, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB D8 */ instr_set<3, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB D9 */ instr_set<3, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB DA */ instr_set<3, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB DB */ instr_set<3, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB DC */ instr_set<3, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB DD */ instr_set<3, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB DE */ instr_set<3, AddressingMode::IXPlusPrevious>,
            /* DD CB DF */ instr_set<3, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB E0 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB E1 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB E2 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB E3 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB E4 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB E5 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB E6 */ instr_set<4, AddressingMode::IXPlusPrevious>,
            /* DD CB E7 */ instr_set<4, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB E8 */ instr_set<5, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB E9 */ instr_set<5, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB EA */ instr_set<5, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB EB */ instr_set<5, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB EC */ instr_set<5, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB ED */ instr_set<5, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB EE */ instr_set<5, AddressingMode::IXPlusPrevious>,
            /* DD CB EF */ instr_set<5, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB F0 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB F1 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB F2 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB F3 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB F4 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB F5 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB F6 */ instr_set<6, AddressingMode::IXPlusPrevious>,
            /* DD CB F7 */ instr_set<6, AddressingMode::IXPlusPrevious, Register::A>,
            /* DD CB F8 */ instr_set<7, AddressingMode::IXPlusPrevious, Register::B>,
            /* DD CB F9 */ instr_set<7, AddressingMode::IXPlusPrevious, Register::C>,
            /* DD CB FA */ instr_set<7, AddressingMode::IXPlusPrevious, Register::D>,
            /* DD CB FB */ instr_set<7, AddressingMode::IXPlusPrevious, Register::E>,
            /* DD CB FC */ instr_set<7, AddressingMode::IXPlusPrevious, Register::H>,
            /* DD CB FD */ instr_set<7, AddressingMode::IXPlusPrevious, Register::L>,
            /* DD CB FE */ instr_set<7, AddressingMode::IXPlusPrevious>,
            /* DD CB FF */ instr_set<7, AddressingMode::IXPlusPrevious, Register::A>
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
            /* ED 42 */ instr_sbc<Register::HL, Register::BC>,
            /* ED 43 */ instr_ld<AddressingMode::Indirect, Register::BC>,
            /* ED 44 */ instr_neg,
            /* ED 45 */ unimplemented_ed_instr<0x45>,
            /* ED 46 */ unimplemented_ed_instr<0x46>,
            /* ED 47 */ instr_ld<Register::I, Register::A>,
            /* ED 48 */ unimplemented_ed_instr<0x48>,
            /* ED 49 */ unimplemented_ed_instr<0x49>,
            /* ED 4A */ instr_adc<Register::HL, Register::BC>,
            /* ED 4B */ instr_ld<Register::BC, AddressingMode::Indirect>,
            /* ED 4C */ unimplemented_ed_instr<0x4C>,
            /* ED 4D */ unimplemented_ed_instr<0x4D>,
            /* ED 4E */ unimplemented_ed_instr<0x4E>,
            /* ED 4F */ instr_ld<Register::R, Register::A>,
            /* ED 50 */ unimplemented_ed_instr<0x50>,
            /* ED 51 */ instr_out<Register::C, Register::D>,
            /* ED 52 */ instr_sbc<Register::HL, Register::DE>,
            /* ED 53 */ instr_ld<AddressingMode::Indirect, Register::DE>,
            /* ED 54 */ unimplemented_ed_instr<0x54>,
            /* ED 55 */ unimplemented_ed_instr<0x55>,
            /* ED 56 */ instr_im_1,
            /* ED 57 */ instr_ld<Register::A, Register::I>,
            /* ED 58 */ unimplemented_ed_instr<0x58>,
            /* ED 59 */ unimplemented_ed_instr<0x59>,
            /* ED 5A */ instr_adc<Register::HL, Register::DE>,
            /* ED 5B */ instr_ld<Register::DE, AddressingMode::Indirect>,
            /* ED 5C */ unimplemented_ed_instr<0x5C>,
            /* ED 5D */ unimplemented_ed_instr<0x5D>,
            /* ED 5E */ unimplemented_ed_instr<0x5E>,
            /* ED 5F */ instr_ld<Register::A, Register::R>,
            /* ED 60 */ unimplemented_ed_instr<0x60>,
            /* ED 61 */ unimplemented_ed_instr<0x61>,
            /* ED 62 */ instr_sbc<Register::HL, Register::HL>,
            /* ED 63 */ instr_ld<AddressingMode::Indirect, Register::H>,
            /* ED 64 */ unimplemented_ed_instr<0x64>,
            /* ED 65 */ unimplemented_ed_instr<0x65>,
            /* ED 66 */ unimplemented_ed_instr<0x66>,
            /* ED 67 */ unimplemented_ed_instr<0x67>,
            /* ED 68 */ unimplemented_ed_instr<0x68>,
            /* ED 69 */ unimplemented_ed_instr<0x69>,
            /* ED 6A */ instr_adc<Register::HL, Register::HL>,
            /* ED 6B */ instr_ld<Register::HL, AddressingMode::Indirect>,
            /* ED 6C */ unimplemented_ed_instr<0x6C>,
            /* ED 6D */ unimplemented_ed_instr<0x6D>,
            /* ED 6E */ unimplemented_ed_instr<0x6E>,
            /* ED 6F */ unimplemented_ed_instr<0x6F>,
            /* ED 70 */ unimplemented_ed_instr<0x70>,
            /* ED 71 */ unimplemented_ed_instr<0x71>,
            /* ED 72 */ instr_sbc<Register::HL, Register::SP>,
            /* ED 73 */ instr_ld<AddressingMode::Indirect, Register::SP>,
            /* ED 74 */ unimplemented_ed_instr<0x74>,
            /* ED 75 */ unimplemented_ed_instr<0x75>,
            /* ED 76 */ unimplemented_ed_instr<0x76>,
            /* ED 77 */ unimplemented_ed_instr<0x77>,
            /* ED 78 */ unimplemented_ed_instr<0x78>,
            /* ED 79 */ instr_out<Register::C, Register::A>,
            /* ED 7A */ instr_adc<Register::HL, Register::SP>,
            /* ED 7B */ instr_ld<Register::SP, AddressingMode::Indirect>,
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
            /* ED A0 */ instr_ldi,
            /* ED A1 */ instr_cpd_cpi<1>,
            /* ED A2 */ unimplemented_ed_instr<0xA2>,
            /* ED A3 */ instr_outi,
            /* ED A4 */ unimplemented_ed_instr<0xA4>,
            /* ED A5 */ unimplemented_ed_instr<0xA5>,
            /* ED A6 */ unimplemented_ed_instr<0xA6>,
            /* ED A7 */ unimplemented_ed_instr<0xA7>,
            /* ED A8 */ instr_ldd,
            /* ED A9 */ instr_cpd_cpi<-1>,
            /* ED AA */ unimplemented_ed_instr<0xAA>,
            /* ED AB */ unimplemented_ed_instr<0xAB>,
            /* ED AC */ unimplemented_ed_instr<0xAC>,
            /* ED AD */ unimplemented_ed_instr<0xAD>,
            /* ED AE */ unimplemented_ed_instr<0xAE>,
            /* ED AF */ unimplemented_ed_instr<0xAF>,
            /* ED B0 */ instr_ldir,
            /* ED B1 */ instr_cpdr_cpir<1>,
            /* ED B2 */ unimplemented_ed_instr<0xB2>,
            /* ED B3 */ instr_otir,
            /* ED B4 */ unimplemented_ed_instr<0xB4>,
            /* ED B5 */ unimplemented_ed_instr<0xB5>,
            /* ED B6 */ unimplemented_ed_instr<0xB6>,
            /* ED B7 */ unimplemented_ed_instr<0xB7>,
            /* ED B8 */ instr_lddr,
            /* ED B9 */ instr_cpdr_cpir<-1>,
            /* ED BA */ unimplemented_ed_instr<0xBA>,
            /* ED BB */ unimplemented_ed_instr<0xBB>,
            /* ED BC */ unimplemented_ed_instr<0xBC>,
            /* ED BD */ unimplemented_ed_instr<0xBD>,
            /* ED BE */ unimplemented_ed_instr<0xBE>,
            /* ED BF */ unimplemented_ed_instr<0xBF>,
    };

    const instruction fd_instructions[0x100] = {
            /* FD 00 */ instructions[0x00],
            /* FD 01 */ instructions[0x01],
            /* FD 02 */ instructions[0x02],
            /* FD 03 */ instructions[0x03],
            /* FD 04 */ instructions[0x04],
            /* FD 05 */ instructions[0x05],
            /* FD 06 */ instructions[0x06],
            /* FD 07 */ instructions[0x07],
            /* FD 08 */ instructions[0x08],
            /* FD 09 */ instr_add<Register::IY, Register::BC>,
            /* FD 0A */ instructions[0x0A],
            /* FD 0B */ instructions[0x0B],
            /* FD 0C */ instructions[0x0C],
            /* FD 0D */ instructions[0x0D],
            /* FD 0E */ instructions[0x0E],
            /* FD 0F */ instructions[0x0F],
            /* FD 10 */ instructions[0x10],
            /* FD 11 */ instructions[0x11],
            /* FD 12 */ instructions[0x12],
            /* FD 13 */ instructions[0x13],
            /* FD 14 */ instructions[0x14],
            /* FD 15 */ instructions[0x15],
            /* FD 16 */ instructions[0x16],
            /* FD 17 */ instructions[0x17],
            /* FD 18 */ instructions[0x18],
            /* FD 19 */ instr_add<Register::IY, Register::DE>,
            /* FD 1A */ instructions[0x1A],
            /* FD 1B */ instructions[0x1B],
            /* FD 1C */ instructions[0x1C],
            /* FD 1D */ instructions[0x1D],
            /* FD 1E */ instructions[0x1E],
            /* FD 1F */ instructions[0x1F],
            /* FD 20 */ instructions[0x20],
            /* FD 21 */ instr_ld<Register::IY, AddressingMode::Immediate>,
            /* FD 22 */ instr_ld<AddressingMode::Indirect, Register::IY>,
            /* FD 23 */ instr_inc<Register::IY>,
            /* FD 24 */ instr_inc<Register::IYH>,
            /* FD 25 */ instr_dec<Register::IYH>,
            /* FD 26 */ instr_ld<Register::IYH, AddressingMode::Immediate>,
            /* FD 27 */ instructions[0x27],
            /* FD 28 */ instructions[0x28],
            /* FD 29 */ instr_add<Register::IY, Register::IY>,
            /* FD 2A */ instr_ld<Register::IY, AddressingMode::Indirect>,
            /* FD 2B */ instr_dec<Register::IY>,
            /* FD 2C */ instr_inc<Register::IYL>,
            /* FD 2D */ instr_dec<Register::IYL>,
            /* FD 2E */ instr_ld<Register::IYL, AddressingMode::Immediate>,
            /* FD 2F */ instructions[0x2F],
            /* FD 30 */ instructions[0x30],
            /* FD 31 */ instructions[0x31],
            /* FD 32 */ instructions[0x32],
            /* FD 33 */ instructions[0x33],
            /* FD 34 */ instr_inc<AddressingMode::IYPlus>,
            /* FD 35 */ instr_dec<AddressingMode::IYPlus>,
            /* FD 36 */ instr_ld<AddressingMode::IYPlus, AddressingMode::Immediate>,
            /* FD 37 */ instructions[0x37],
            /* FD 38 */ instructions[0x38],
            /* FD 39 */ instr_add<Register::IY, Register::SP>,
            /* FD 3A */ instructions[0x3A],
            /* FD 3B */ instructions[0x3B],
            /* FD 3C */ instructions[0x3C],
            /* FD 3D */ instructions[0x3D],
            /* FD 3E */ instructions[0x3E],
            /* FD 3F */ instructions[0x3F],
            /* FD 40 */ instructions[0x40],
            /* FD 41 */ instructions[0x41],
            /* FD 42 */ instructions[0x42],
            /* FD 43 */ instructions[0x43],
            /* FD 44 */ instr_ld<Register::B, Register::IYH>,
            /* FD 45 */ instr_ld<Register::B, Register::IYL>,
            /* FD 46 */ instr_ld<Register::B, AddressingMode::IYPlus>,
            /* FD 47 */ instructions[0x47],
            /* FD 48 */ instructions[0x48],
            /* FD 49 */ instructions[0x49],
            /* FD 4A */ instructions[0x4A],
            /* FD 4B */ instructions[0x4B],
            /* FD 4C */ instr_ld<Register::C, Register::IYH>,
            /* FD 4D */ instr_ld<Register::C, Register::IYL>,
            /* FD 4E */ instr_ld<Register::C, AddressingMode::IYPlus>,
            /* FD 4F */ instructions[0x4F],
            /* FD 50 */ instructions[0x50],
            /* FD 51 */ instructions[0x51],
            /* FD 52 */ instructions[0x52],
            /* FD 53 */ instructions[0x53],
            /* FD 54 */ instr_ld<Register::D, Register::IYH>,
            /* FD 55 */ instr_ld<Register::D, Register::IYL>,
            /* FD 56 */ instr_ld<Register::D, AddressingMode::IYPlus>,
            /* FD 57 */ instructions[0x57],
            /* FD 58 */ instructions[0x58],
            /* FD 59 */ instructions[0x59],
            /* FD 5A */ instructions[0x5A],
            /* FD 5B */ instructions[0x5B],
            /* FD 5C */ instr_ld<Register::E, Register::IYH>,
            /* FD 5D */ instr_ld<Register::E, Register::IYL>,
            /* FD 5E */ instr_ld<Register::E, AddressingMode::IYPlus>,
            /* FD 5F */ instructions[0x5F],
            /* FD 60 */ instr_ld<Register::IYH, Register::B>,
            /* FD 61 */ instr_ld<Register::IYH, Register::C>,
            /* FD 62 */ instr_ld<Register::IYH, Register::D>,
            /* FD 63 */ instr_ld<Register::IYH, Register::E>,
            /* FD 64 */ instr_ld<Register::IYH, Register::IYH>,
            /* FD 65 */ instr_ld<Register::IYH, Register::IYL>,
            /* FD 66 */ instr_ld<Register::H, AddressingMode::IYPlus>,
            /* FD 67 */ instr_ld<Register::IYH, Register::A>,
            /* FD 68 */ instr_ld<Register::IYL, Register::B>,
            /* FD 69 */ instr_ld<Register::IYL, Register::C>,
            /* FD 6A */ instr_ld<Register::IYL, Register::D>,
            /* FD 6B */ instr_ld<Register::IYL, Register::E>,
            /* FD 6C */ instr_ld<Register::IYL, Register::IYH>,
            /* FD 6D */ instr_ld<Register::IYL, Register::IYL>,
            /* FD 6E */ instr_ld<Register::L, AddressingMode::IYPlus>,
            /* FD 6F */ instr_ld<Register::IYL, Register::A>,
            /* FD 70 */ instr_ld<AddressingMode::IYPlus, Register::B>,
            /* FD 71 */ instr_ld<AddressingMode::IYPlus, Register::C>,
            /* FD 72 */ instr_ld<AddressingMode::IYPlus, Register::D>,
            /* FD 73 */ instr_ld<AddressingMode::IYPlus, Register::E>,
            /* FD 74 */ instr_ld<AddressingMode::IYPlus, Register::H>,
            /* FD 75 */ instr_ld<AddressingMode::IYPlus, Register::L>,
            /* FD 76 */ instructions[0x76],
            /* FD 77 */ instr_ld<AddressingMode::IYPlus, Register::A>,
            /* FD 78 */ instructions[0x78],
            /* FD 79 */ instructions[0x79],
            /* FD 7A */ instructions[0x7A],
            /* FD 7B */ instructions[0x7B],
            /* FD 7C */ instr_ld<Register::A, Register::IYH>,
            /* FD 7D */ instr_ld<Register::A, Register::IYL>,
            /* FD 7E */ instr_ld<Register::A, AddressingMode::IYPlus>,
            /* FD 7F */ instructions[0x7F],
            /* FD 80 */ instructions[0x80],
            /* FD 81 */ instructions[0x81],
            /* FD 82 */ instructions[0x82],
            /* FD 83 */ instructions[0x83],
            /* FD 84 */ instr_add<Register::A, Register::IYH>,
            /* FD 85 */ instr_add<Register::A, Register::IYL>,
            /* FD 86 */ instr_add<Register::A, AddressingMode::IYPlus>,
            /* FD 87 */ instructions[0x87],
            /* FD 88 */ instructions[0x88],
            /* FD 89 */ instructions[0x89],
            /* FD 8A */ instructions[0x8A],
            /* FD 8B */ instructions[0x8B],
            /* FD 8C */ instr_adc<Register::A, Register::IYH>,
            /* FD 8D */ instr_adc<Register::A, Register::IYL>,
            /* FD 8E */ instr_adc<Register::A, AddressingMode::IYPlus>,
            /* FD 8F */ instructions[0x8F],
            /* FD 90 */ instructions[0x90],
            /* FD 91 */ instructions[0x91],
            /* FD 92 */ instructions[0x92],
            /* FD 93 */ instructions[0x93],
            /* FD 94 */ instr_sub<Register::IYH>,
            /* FD 95 */ instr_sub<Register::IYL>,
            /* FD 96 */ instr_sub<AddressingMode::IYPlus>,
            /* FD 97 */ instructions[0x97],
            /* FD 98 */ instructions[0x98],
            /* FD 99 */ instructions[0x99],
            /* FD 9A */ instructions[0x9A],
            /* FD 9B */ instructions[0x9B],
            /* FD 9C */ instr_sbc<Register::A, Register::IYH>,
            /* FD 9D */ instr_sbc<Register::A, Register::IYL>,
            /* FD 9E */ instr_sbc<Register::A, AddressingMode::IYPlus>,
            /* FD 9F */ instructions[0x9F],
            /* FD A0 */ instructions[0xA0],
            /* FD A1 */ instructions[0xA1],
            /* FD A2 */ instructions[0xA2],
            /* FD A3 */ instructions[0xA3],
            /* FD A4 */ instr_and<Register::IYH>,
            /* FD A5 */ instr_and<Register::IYL>,
            /* FD A6 */ instr_and<AddressingMode::IYPlus>,
            /* FD A7 */ instructions[0xA7],
            /* FD A8 */ instructions[0xA8],
            /* FD A9 */ instructions[0xA9],
            /* FD AA */ instructions[0xAA],
            /* FD AB */ instructions[0xAB],
            /* FD AC */ instr_xor<Register::IYH>,
            /* FD AD */ instr_xor<Register::IYL>,
            /* FD AE */ instr_xor<AddressingMode::IYPlus>,
            /* FD AF */ instructions[0xAF],
            /* FD B0 */ instructions[0xB0],
            /* FD B1 */ instructions[0xB1],
            /* FD B2 */ instructions[0xB2],
            /* FD B3 */ instructions[0xB3],
            /* FD B4 */ instr_or<Register::IYH>,
            /* FD B5 */ instr_or<Register::IYL>,
            /* FD B6 */ instr_or<AddressingMode::IYPlus>,
            /* FD B7 */ instructions[0xB7],
            /* FD B8 */ instructions[0xB8],
            /* FD B9 */ instructions[0xB9],
            /* FD BA */ instructions[0xBA],
            /* FD BB */ instructions[0xBB],
            /* FD BC */ instr_cp<Register::IYH>,
            /* FD BD */ instr_cp<Register::IYL>,
            /* FD BE */ instr_cp<AddressingMode::IYPlus>,
            /* FD BF */ instructions[0xBF],
            /* FD C0 */ instructions[0xC0],
            /* FD C1 */ instructions[0xC1],
            /* FD C2 */ instructions[0xC2],
            /* FD C3 */ instructions[0xC3],
            /* FD C4 */ instructions[0xC4],
            /* FD C5 */ instructions[0xC5],
            /* FD C6 */ instructions[0xC6],
            /* FD C7 */ instructions[0xC7],
            /* FD C8 */ instructions[0xC8],
            /* FD C9 */ instructions[0xC9],
            /* FD CA */ instructions[0xCA],
            /* FD CB */ instr_fdcb,
            /* FD CC */ instructions[0xCC],
            /* FD CD */ instructions[0xCD],
            /* FD CE */ instructions[0xCE],
            /* FD CF */ instructions[0xCF],
            /* FD D0 */ instructions[0xD0],
            /* FD D1 */ instructions[0xD1],
            /* FD D2 */ instructions[0xD2],
            /* FD D3 */ instructions[0xD3],
            /* FD D4 */ instructions[0xD4],
            /* FD D5 */ instructions[0xD5],
            /* FD D6 */ instructions[0xD6],
            /* FD D7 */ instructions[0xD7],
            /* FD D8 */ instructions[0xD8],
            /* FD D9 */ instructions[0xD9],
            /* FD DA */ instructions[0xDA],
            /* FD DB */ instructions[0xDB],
            /* FD DC */ instructions[0xDC],
            /* FD DD */ unimplemented_fd_instr<0xDD>, // This is probably actually valid, but FD DD xx would be strange to see
            /* FD DE */ instructions[0xDE],
            /* FD DF */ instructions[0xDF],
            /* FD E0 */ instructions[0xE0],
            /* FD E1 */ instr_pop<Register::IY>,
            /* FD E2 */ instructions[0xE2],
            /* FD E3 */ unimplemented_fd_instr<0xE3>, // ex (sp), iy
            /* FD E4 */ instructions[0xE4],
            /* FD E5 */ instr_push<Register::IY>,
            /* FD E6 */ instructions[0xE6],
            /* FD E7 */ instructions[0xE7],
            /* FD E8 */ instructions[0xE8],
            /* FD E9 */ instr_jp<Condition::Always, AddressingMode::IY>,
            /* FD EA */ instructions[0xEA],
            /* FD EB */ instructions[0xEB],
            /* FD EC */ instructions[0xEC],
            /* FD ED */ instructions[0xED],
            /* FD EE */ instructions[0xEE],
            /* FD EF */ instructions[0xEF],
            /* FD F0 */ instructions[0xF0],
            /* FD F1 */ instructions[0xF1],
            /* FD F2 */ instructions[0xF2],
            /* FD F3 */ instructions[0xF3],
            /* FD F4 */ instructions[0xF4],
            /* FD F5 */ instructions[0xF5],
            /* FD F6 */ instructions[0xF6],
            /* FD F7 */ instructions[0xF7],
            /* FD F8 */ instructions[0xF8],
            /* FD F9 */ instructions[0xF9],
            /* FD FA */ unimplemented_fd_instr<0xFA>,
            /* FD FB */ instructions[0xFB],
            /* FD FC */ instructions[0xFC],
            /* FD FD */ unimplemented_fd_instr<0xFD>, // This is also probably valid, FD FD xx would be strange, and could be chained infinitely
            /* FD FE */ instructions[0xFE],
            /* FD FF */ instructions[0xFF],
    };

    const instruction fdcb_instructions[0x100] = {
            /* FD CB 00 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 01 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 02 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 03 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 04 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 05 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 06 */ instr_rlc<AddressingMode::IYPlusPrevious>,
            /* FD CB 07 */ instr_rlc<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 08 */ instr_rrc<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 09 */ instr_rrc<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 0A */ instr_rrc<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 0B */ instr_rrc<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 0C */ instr_rrc<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 0D */ instr_rrc<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 0E */ instr_rrc<AddressingMode::IYPlusPrevious>,
            /* FD CB 0F */ instr_rrc<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 10 */ instr_rl<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 11 */ instr_rl<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 12 */ instr_rl<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 13 */ instr_rl<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 14 */ instr_rl<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 15 */ instr_rl<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 16 */ instr_rl<AddressingMode::IYPlusPrevious>,
            /* FD CB 17 */ instr_rl<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 18 */ instr_rr<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 19 */ instr_rr<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 1A */ instr_rr<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 1B */ instr_rr<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 1C */ instr_rr<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 1D */ instr_rr<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 1E */ instr_rr<AddressingMode::IYPlusPrevious>,
            /* FD CB 1F */ instr_rr<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 20 */ instr_sla<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 21 */ instr_sla<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 22 */ instr_sla<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 23 */ instr_sla<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 24 */ instr_sla<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 25 */ instr_sla<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 26 */ instr_sla<AddressingMode::IYPlusPrevious>,
            /* FD CB 27 */ instr_sla<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 28 */ instr_sra<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 29 */ instr_sra<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 2A */ instr_sra<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 2B */ instr_sra<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 2C */ instr_sra<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 2D */ instr_sra<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 2E */ instr_sra<AddressingMode::IYPlusPrevious>,
            /* FD CB 2F */ instr_sra<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 30 */ instr_sll<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 31 */ instr_sll<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 32 */ instr_sll<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 33 */ instr_sll<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 34 */ instr_sll<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 35 */ instr_sll<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 36 */ instr_sll<AddressingMode::IYPlusPrevious>,
            /* FD CB 37 */ instr_sll<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 38 */ instr_srl<AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 39 */ instr_srl<AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 3A */ instr_srl<AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 3B */ instr_srl<AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 3C */ instr_srl<AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 3D */ instr_srl<AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 3E */ instr_srl<AddressingMode::IYPlusPrevious>,
            /* FD CB 3F */ instr_srl<AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 40 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 41 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 42 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 43 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 44 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 45 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 46 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 47 */ instr_bit<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 48 */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 49 */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4A */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4B */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4C */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4D */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4E */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 4F */ instr_bit<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 50 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 51 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 52 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 53 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 54 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 55 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 56 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 57 */ instr_bit<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 58 */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 59 */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5A */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5B */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5C */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5D */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5E */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 5F */ instr_bit<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 60 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 61 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 62 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 63 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 64 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 65 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 66 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 67 */ instr_bit<4, AddressingMode::IYPlusPrevious>,
            /* FD CB 68 */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 69 */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6A */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6B */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6C */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6D */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6E */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 6F */ instr_bit<5, AddressingMode::IYPlusPrevious>,
            /* FD CB 70 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 71 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 72 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 73 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 74 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 75 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 76 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 77 */ instr_bit<6, AddressingMode::IYPlusPrevious>,
            /* FD CB 78 */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 79 */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7A */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7B */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7C */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7D */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7E */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 7F */ instr_bit<7, AddressingMode::IYPlusPrevious>,
            /* FD CB 80 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 81 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 82 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 83 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 84 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 85 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 86 */ instr_res<0, AddressingMode::IYPlusPrevious>,
            /* FD CB 87 */ instr_res<0, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 88 */ instr_res<1, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 89 */ instr_res<1, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 8A */ instr_res<1, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 8B */ instr_res<1, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 8C */ instr_res<1, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 8D */ instr_res<1, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 8E */ instr_res<1, AddressingMode::IYPlusPrevious>,
            /* FD CB 8F */ instr_res<1, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 90 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 91 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 92 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 93 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 94 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 95 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 96 */ instr_res<2, AddressingMode::IYPlusPrevious>,
            /* FD CB 97 */ instr_res<2, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB 98 */ instr_res<3, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB 99 */ instr_res<3, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB 9A */ instr_res<3, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB 9B */ instr_res<3, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB 9C */ instr_res<3, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB 9D */ instr_res<3, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB 9E */ instr_res<3, AddressingMode::IYPlusPrevious>,
            /* FD CB 9F */ instr_res<3, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB A0 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB A1 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB A2 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB A3 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB A4 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB A5 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB A6 */ instr_res<4, AddressingMode::IYPlusPrevious>,
            /* FD CB A7 */ instr_res<4, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB A8 */ instr_res<5, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB A9 */ instr_res<5, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB AA */ instr_res<5, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB AB */ instr_res<5, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB AC */ instr_res<5, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB AD */ instr_res<5, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB AE */ instr_res<5, AddressingMode::IYPlusPrevious>,
            /* FD CB AF */ instr_res<5, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB B0 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB B1 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB B2 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB B3 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB B4 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB B5 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB B6 */ instr_res<6, AddressingMode::IYPlusPrevious>,
            /* FD CB B7 */ instr_res<6, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB B8 */ instr_res<7, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB B9 */ instr_res<7, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB BA */ instr_res<7, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB BB */ instr_res<7, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB BC */ instr_res<7, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB BD */ instr_res<7, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB BE */ instr_res<7, AddressingMode::IYPlusPrevious>,
            /* FD CB BF */ instr_res<7, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB C0 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB C1 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB C2 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB C3 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB C4 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB C5 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB C6 */ instr_set<0, AddressingMode::IYPlusPrevious>,
            /* FD CB C7 */ instr_set<0, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB C8 */ instr_set<1, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB C9 */ instr_set<1, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB CA */ instr_set<1, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB CB */ instr_set<1, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB CC */ instr_set<1, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB CD */ instr_set<1, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB CE */ instr_set<1, AddressingMode::IYPlusPrevious>,
            /* FD CB CF */ instr_set<1, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB D0 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB D1 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB D2 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB D3 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB D4 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB D5 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB D6 */ instr_set<2, AddressingMode::IYPlusPrevious>,
            /* FD CB D7 */ instr_set<2, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB D8 */ instr_set<3, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB D9 */ instr_set<3, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB DA */ instr_set<3, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB DB */ instr_set<3, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB DC */ instr_set<3, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB DD */ instr_set<3, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB DE */ instr_set<3, AddressingMode::IYPlusPrevious>,
            /* FD CB DF */ instr_set<3, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB E0 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB E1 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB E2 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB E3 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB E4 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB E5 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB E6 */ instr_set<4, AddressingMode::IYPlusPrevious>,
            /* FD CB E7 */ instr_set<4, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB E8 */ instr_set<5, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB E9 */ instr_set<5, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB EA */ instr_set<5, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB EB */ instr_set<5, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB EC */ instr_set<5, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB ED */ instr_set<5, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB EE */ instr_set<5, AddressingMode::IYPlusPrevious>,
            /* FD CB EF */ instr_set<5, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB F0 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB F1 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB F2 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB F3 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB F4 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB F5 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB F6 */ instr_set<6, AddressingMode::IYPlusPrevious>,
            /* FD CB F7 */ instr_set<6, AddressingMode::IYPlusPrevious, Register::A>,
            /* FD CB F8 */ instr_set<7, AddressingMode::IYPlusPrevious, Register::B>,
            /* FD CB F9 */ instr_set<7, AddressingMode::IYPlusPrevious, Register::C>,
            /* FD CB FA */ instr_set<7, AddressingMode::IYPlusPrevious, Register::D>,
            /* FD CB FB */ instr_set<7, AddressingMode::IYPlusPrevious, Register::E>,
            /* FD CB FC */ instr_set<7, AddressingMode::IYPlusPrevious, Register::H>,
            /* FD CB FD */ instr_set<7, AddressingMode::IYPlusPrevious, Register::L>,
            /* FD CB FE */ instr_set<7, AddressingMode::IYPlusPrevious>,
            /* FD CB FF */ instr_set<7, AddressingMode::IYPlusPrevious, Register::A>
    };
}