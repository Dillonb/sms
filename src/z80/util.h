#ifndef Z80_UTIL_H
#define Z80_UTIL_H

namespace Z80 {

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
}

#endif //Z80_UTIL_H
