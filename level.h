#pragma once
#include "map.h"

using namespace std;

struct Level {
    bool isLeaf = false; // Is this level a leaf level
    unsigned entryCount = 0; // Number of entries possible at this level
    Level **children; // Child levels
    Map *mappings; // Mappings at this level (only for leaf levels)
    unsigned depth; // Depth of this level in the page table

    // Constructor
    Level(unsigned entryCount_, bool isLeaf_, unsigned depth_ = 0)
        : entryCount(entryCount_), isLeaf(isLeaf_), children(nullptr), mappings(nullptr), depth(depth_) {}
    
    // Destructor
    ~Level();

    Level(const Level&) = delete; // Disable copy constructor
    Level& operator=(const Level&) = delete; // Disable copy assignment

    // Allocate the interior child pointer array (non lead levels)
    void allocateChildren();

    // Allocate the leaf mappings array (leaf levels)
    void allocateMappings();

    // ensures a child exists at the given index, allocating if necessary
    Level* ensureChild(unsigned index, unsigned childEntryCount, bool childIsLeadf);

    // Accessors
    inline Level* getChild(unsigned index) const { return children ? children[index] : nullptr; }
    inline Map& getMapping(unsigned index) {return mappings[index]; } // modifiable reference is returned
    inline const Map& getMap(unsigned index) const {return mappings[index];} // constant reference is returned

};