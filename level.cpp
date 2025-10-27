#include "level.h"
#include <cstdlib>

// Destructor
Level::~Level() {
    if (isLeaf) {
        // delete mappings array
        delete[] mappings;
        mappings = nullptr;
    } 
    if (children) {
        // delete children levels recursively
        for (unsigned i = 0; i < entryCount; i++) {
            if(children[i]) {delete children[i]; children[i] = nullptr;}
        }
        delete[] children;
        children = nullptr;
    }
}

// if is not a leaf and children is null, allocate children array
void Level::allocateChildren() {
    if (!isLeaf && !children) {
        children = new Level*[entryCount];
        for(unsigned i = 0; i < entryCount; i++) {
            children[i] = nullptr;
        }
    }
}

// if is a leaf and mappings is null, allocate mappings array
void Level::allocateMappings() {
    if (isLeaf && !mappings) {
        mappings = new Map[entryCount];
    }
}

// ensures a child exists at the given index, allocating if necessary
Level* Level::ensureChild(unsigned index, unsigned childEntryCount, bool childIsLeaf) {
    if (!children) {
        allocateChildren();
    }
    if (!children[index]) {
        children[index] = new Level(childEntryCount, childIsLeaf, depth + 1);
    }
    return children[index];
}