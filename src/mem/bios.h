#ifndef SMS_BIOS_H
#define SMS_BIOS_H

#include <vector>

namespace Bios {
    bool try_load();
    extern std::vector<u8> data;
}

#endif //SMS_BIOS_H
