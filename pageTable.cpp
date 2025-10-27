#include "pageTable.h"

void PageTable::initFromLevelBits(const vector<int>& levelBits) {
    numLevels = levelBits.size();

    // sets the vector size to be the same as numLevels
    entryCount.resize(numLevels);
    bitmasks.resize(numLevels);
    shifts.resize(numLevels);

    // total number of page table bits
    int total = 0;
    for(int bits : levelBits) { total += bits;}

    // sets amount of offset bits and offset's bitmask
    offsetBits = 32u - (unsigned)total;
    offsetMask = (1u << offsetBits) - 1u; // e.g. if offsetBits is 12, then 1u << 12 is 1 0000 0000 0000, -1u makes it 1111 1111 1111

    // calculates the shifts for each level
    for (int i = numLevels - 1; i >=0; i--) {
        if (i == numLevels - 1) {
            shifts[i] = offsetBits;
        } else {
            shifts[i] = shifts[i + 1] + levelBits[i + 1];
        }
    }

    // builds the entryCounts and bitmasks vectors
    for (int i = 0; i < numLevels; i++) {
        entryCount[i] = 1u << levelBits[i];
        bitmasks[i] = ((1u << levelBits[i]) - 1u) << shifts[i];
    }

    // root level allocated here
}