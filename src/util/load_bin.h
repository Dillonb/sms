#ifndef SMS_LOAD_BIN_H
#define SMS_LOAD_BIN_H

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

template <typename T>
std::vector<T> load_bin(const char* path) {
    std::ifstream file{path, std::ios::binary};
    return std::vector<T>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

#endif //SMS_LOAD_BIN_H
