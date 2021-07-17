#ifndef SMS_ROM_H
#define SMS_ROM_H

#include <vector>
#include <util/types.h>

using std::vector;

namespace Rom {
    struct Rom {
        vector<u8> data;
    };

    void load(const char* path);

    extern Rom rom;
}

#endif //SMS_ROM_H
