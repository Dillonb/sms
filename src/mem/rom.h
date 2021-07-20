#ifndef SMS_ROM_H
#define SMS_ROM_H

#include <vector>
#include <util/types.h>

using std::vector;

namespace Rom {
    extern unsigned int bank_offsets[3];
    struct Rom {
        vector<u8> data;
    };

    void load(const char* path);
    u8 read(u16 address);
    void mapper_ctrl_write(u16 address, u8 value);

    extern Rom rom;
}

#endif //SMS_ROM_H
