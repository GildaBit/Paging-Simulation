/**
 * Author: Gilad Bitton
 * RedID: 130621085
 *
 * **/

#pragma once

using namespace std;

struct Map {
    int pfn = -1; // Physical Frame Number -1, indicates unmapped
    bool valid = false; // Valid bit
};