#ifndef SMS_BUS_H
#define SMS_BUS_H

#include <util/types.h>

namespace Bus {
    u8 read_byte(u16 address);
    void write_byte(u16 address, u8 value);
    void port_out(u8 port, u8 value);
    u8 port_in(u8 port);
}

#endif //SMS_BUS_H
