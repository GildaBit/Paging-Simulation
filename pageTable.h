#pragma once
#include <vector>
#include <cstdint>
#include "level.h"

using namespace std;

struct PageTable {
    int numLevels = 0; // Number of levels in the page table
    vector<unsigned> entryCount; // Number of entries possible at each level. 1 << levelBits[i]
    vector<unsigned> bitmasks; // Bitmask for each level
    vector<unsigned> shifts; // Right shift amount for each level
    unsigned offsetBits = 0; // Number of offset bits
    unsigned offsetMask = 0; //Bitmask for offset
    Level* rootLevel = nullptr; // Pointer to the root level of the page table

    // Function to build masks/shifts/entries from levelBits
    void initFromLevelBits(const vector<int>& levelBits);

    // returns the number of bytes of each page table entry
    // example: if offset bits is 12, each page is 1000 0000 0000 in binary, or 4096 bytes
    unsigned pageSizeBytes() const { return (1u << offsetBits); } // u makes 1 unsigned, << is left shift

    // returns a given individual VPN piece
    // example: if levels are 6 6 8 and level = 0, returns bits 31-26
    unsigned getVPNPiece(uint32_t vaddr, int level) const {
        // & is bitwise AND, >> is right shift
        return (vaddr & bitmasks[level]) >> shifts[level];
    }

    // gets the offset from a virtual address
    unsigned getOffset(uint32_t vaddr) const {return vaddr & offsetMask; }

    // Map* searchMappedPfn(uint32_t vaddr);
    // void  insertMapForVpn2Pfn(uint32_t vaddr, int pfn);
};

