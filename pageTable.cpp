/**
 * Author: Gilad Bitton
 * RedID: 130621085
 *
 * **/

#include "pageTable.h"

// Destructor
PageTable::~PageTable() {
    if (rootLevel) {
        delete rootLevel;
        rootLevel = nullptr;
    }
}

static uint64_t countLevelEntries(const Level* node) {
    if (!node) return 0;

    uint64_t total = 0;

    if (node->isLeaf) {
        // Count ALL mapping slots at the leaf (if the array was allocated)
        if (node->mappings) {
            total += node->entryCount;
        }
        return total;
    }

    // Internal level: count ALL child slots (if the array was allocated)
    if (node->children) {
        total += node->entryCount;
        for (unsigned i = 0; i < node->entryCount; ++i) {
            if (node->children[i]) {
                total += countLevelEntries(node->children[i]);
            }
        }
    }
    return total;
}

uint64_t PageTable::countEntries(const PageTable* pt) {
    if (!pt) return 0;
    return countLevelEntries(pt->rootLevel);
}

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
    rootLevel = new Level(entryCount[0], numLevels == 1);
}

// searches and returns the Map for the given virtual address
Map* PageTable::searchMappedPfn(unsigned int virtualAddress) {
    // preliminary checks
    if (!rootLevel || numLevels <= 0) { return nullptr;}

    // traverse the page table levels to get to desired leaf level
    Level* currentLevel = rootLevel;
    while (!currentLevel->isLeaf) {
        currentLevel = currentLevel->getChild(getVPNPiece(virtualAddress, currentLevel->depth));
        if (!currentLevel) {
            return nullptr;
        }
    }

    // at lead level, get the mapping, check validity, and return
    unsigned vpnPiece = getVPNPiece(virtualAddress, currentLevel->depth);
    if (!currentLevel->mappings) {
        return nullptr;
    }
    Map& mapping = *currentLevel->getMapping(vpnPiece);
    if (mapping.valid) {
        return &mapping;
    } else {
        return nullptr;
    }
}

// inserts a mapping from the given virtual address to the given frame number
void  PageTable::insertMapForVpn2Pfn(unsigned int virtualAddress, int frame) {
        // preliminary checks
    if (!rootLevel || numLevels <= 0) { return;}

    // traverse the page table levels to get to desired leaf level
    Level* currentLevel = rootLevel;
    while (!currentLevel->isLeaf) {
        unsigned vpnPiece = getVPNPiece(virtualAddress, currentLevel->depth);
        unsigned childEntryCount = entryCount[currentLevel->depth + 1];
        bool childIsLeaf = (currentLevel->depth + 1 == (unsigned)(numLevels - 1));
        currentLevel = currentLevel->ensureChild(vpnPiece, childEntryCount, childIsLeaf);
    }

    // at leaf level, set the mapping and mark valid
    unsigned vpnPiece = getVPNPiece(virtualAddress, currentLevel->depth);
    if (!currentLevel->mappings) {
        currentLevel->allocateMappings();
    }
    Map* mapping = currentLevel->getMapping(vpnPiece);
    mapping->pfn = frame;
    mapping->valid = true;
}

// extracts the VPN piece from the given virtual address using the given mask and shift
unsigned int extractVPNFromVirtualAddress(unsigned int virtualAddress, unsigned int mask, unsigned int shift) {
    return (virtualAddress & mask) >> shift;
}