#pragma once
#include <cstdint>

struct LoadedPage {
    int pfn; // Physical Frame Number
    uint32_t vpn; // Virtual Page Number
    uint16_t bitstring; // 16-bit aging bitstring
    uint32_t lastAccessTime; // last access time for tie-breaking
};
