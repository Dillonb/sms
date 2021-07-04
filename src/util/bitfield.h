#ifndef SMS_BITFIELD_H
#define SMS_BITFIELD_H

#include <bit>
#include <type_traits>

#include "types.h"

namespace Util {

    template<class E>
    struct Bitfield {
        using underlying_t = std::underlying_type_t<E>;
        underlying_t raw;
    private:
        struct FieldRef {
            Bitfield<E>& parent;
            underlying_t field;

            constexpr void operator=(const underlying_t& value) {
                parent.raw &= ~field;
                parent.raw |= value << std::countr_zero(field);
            }

            constexpr void operator|=(const underlying_t& value) {
                parent.raw |= value << std::countr_zero(field);
            }

            constexpr void operator&=(const underlying_t& value) {
                parent.raw &= value << std::countr_zero(field);
            }
        };

    public:
        template<typename T>
        constexpr Bitfield<E>& operator=(const T& value) {
            raw = static_cast<underlying_t>(value);
            return *this;
        }

        template<typename T>
        constexpr __attribute__((const)) underlying_t operator[](const T& _field) const {
            static_assert(sizeof(underlying_t) == sizeof(*this));  // had to check this somewhere
            const auto field = static_cast<underlying_t>(_field);
            return (raw & field) >> std::countr_zero(field);
        }

        template<typename T>
        constexpr Bitfield<E>& operator|=(const T& value) {
            raw |= static_cast<underlying_t>(value);
            return *this;
        }

        template<typename T>
        constexpr underlying_t operator|(const T& value) const {
            return raw | static_cast<u32>(value);
        }

        template<typename T>
        constexpr Bitfield<E>& operator&=(const T& value) {
            raw &= value;
            return *this;
        }

        template<typename T>
        constexpr underlying_t operator&(const T& value) const {
            return raw & value;
        }

        template<typename T>
        constexpr FieldRef operator()(const T& _field) {
            const auto field = static_cast<underlying_t>(_field);
            return { *this, field };
        }
    };

}

#endif //SMS_BITFIELD_H
