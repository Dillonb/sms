#include "rom.h"

#include <util/load_bin.h>
#include <util/log.h>

using std::filesystem::exists;

namespace Rom {
    Rom rom;

    void load(const char* path) {
        if (exists(path)) {
            rom.data = load_bin<u8>(path);
        } else {
            logfatal("%s not found!", path);
        }
    }
}