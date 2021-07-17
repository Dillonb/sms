#ifndef SMS_REGISTERS_H
#define SMS_REGISTERS_H

#include "z80.h"

namespace Z80 {
    enum class WideRegister : u16 {
        Lo = 0x00FF,
        Hi = 0xFF00,
        All = 0xFFFF
    };

    struct FlagRegister {
    public:
        bool s;   // Signed
        bool z;   // Zero
        bool b5;  // Bit 5 copy
        bool h;   // Half Carry
        bool b3;  // Bit 3 copy
        bool p_v; // Parity / Overflow
        bool n;   // Add / Subtract
        bool c;   // Carry
        u8 assemble() const {
            return (s << 7)
                   | (z << 6)
                   | (b5 << 5)
                   | (h << 4)
                   | (b3 << 3)
                   | (p_v << 2)
                   | (n << 1)
                   | (c << 0);
        }
        void set(u8 value) {
            s   = ((value >> 7) & 1) == 1;
            z   = ((value >> 6) & 1) == 1;
            b5  = ((value >> 5) & 1) == 1;
            h   = ((value >> 4) & 1) == 1;
            b3  = ((value >> 3) & 1) == 1;
            p_v = ((value >> 2) & 1) == 1;
            n   = ((value >> 1) & 1) == 1;
            c   = ((value >> 0) & 1) == 1;
        }
    };
    enum class Register {
        A,
        F,
        AF,
        AF_,

        B,
        C,
        BC,
        BC_,

        D,
        E,
        DE,
        DE_,

        H,
        L,
        HL,
        HL_,

        I,
        IX,
        IXH,
        IXL,
        IY,
        IYH,
        IYL,

        SP,
    };

    template<Register r>
    struct reg_type {
        using type = void;
    };

    template<> struct reg_type<Register::A> { using type = u8; };
    template<> struct reg_type<Register::F> { using type = u8; };
    template<> struct reg_type<Register::AF> { using type = u16; };
    template<> struct reg_type<Register::AF_> { using type = u16; };

    template<> struct reg_type<Register::B> { using type = u8; };
    template<> struct reg_type<Register::C> { using type = u8; };
    template<> struct reg_type<Register::BC> { using type = u16; };
    template<> struct reg_type<Register::BC_> { using type = u16; };

    template<> struct reg_type<Register::D> { using type = u8; };
    template<> struct reg_type<Register::E> { using type = u8; };
    template<> struct reg_type<Register::DE> { using type = u16; };
    template<> struct reg_type<Register::DE_> { using type = u16; };

    template<> struct reg_type<Register::H> { using type = u8; };
    template<> struct reg_type<Register::L> { using type = u8; };
    template<> struct reg_type<Register::HL> { using type = u16; };
    template<> struct reg_type<Register::HL_> { using type = u16; };

    template<> struct reg_type<Register::I> { using type = u8; };

    template<> struct reg_type<Register::IX> { using type = u16; };
    template<> struct reg_type<Register::IXH> { using type = u8; };
    template<> struct reg_type<Register::IXL> { using type = u8; };

    template<> struct reg_type<Register::IY> { using type = u16; };
    template<> struct reg_type<Register::IYH> { using type = u8; };
    template<> struct reg_type<Register::IYL> { using type = u8; };


    template<> struct reg_type<Register::SP> { using type = u16; };

    template <Register reg>
    constexpr int get_register_size() {
        switch (reg) {
            case Register::A:
            case Register::B:
            case Register::C:
            case Register::D:
            case Register::E:
            case Register::H:
            case Register::L:
            case Register::IXH:
            case Register::IXL:
            case Register::IYH:
            case Register::IYL:
                return sizeof(u8);
            case Register::BC:
            case Register::DE:
            case Register::HL:
            case Register::SP:
            case Register::IX:
            case Register::IY:
                return sizeof(u16);
        }
    }

}

#endif //SMS_REGISTERS_H
