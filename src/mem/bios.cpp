#include <filesystem>
#include <util/types.h>
#include <util/load_bin.h>
#include "bios.h"
#include "util/log.h"

namespace Bios {
    constexpr const char* possible_paths[] {
        "bios13fx.sms"
    };

    std::vector<u8> data;

    bool try_load() {
        for (const auto &path : possible_paths) {
            if (std::filesystem::exists(path)) {
                logalways("Found bios at %s", path);
                data = load_bin<u8>(path);
                return true;
            }
        }
        return false;
    }
}