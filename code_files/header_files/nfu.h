/**
 * Author: Gilad Bitton
 * RedID: 130621085
 *
 * **/

#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace std;

struct LoadedPage {
    int pfn; // Physical Frame Number
    uint32_t vpn; // Virtual Page Number
    uint16_t bitstring; // 16-bit aging bitstring
    uint32_t lastAccessTime; // last access time for tie-breaking
};

struct NFUState {
    vector<LoadedPage> pages; // all currently  loaded pages
    unordered_map<uint32_t, size_t> vpnToIndex; // maps VPN to index in pages vector for quick lookup
    unordered_set<uint32_t> accessed; // all VPNs accessed in this interval will have their bitstring's MSB set to 1
    uint32_t currentTime; // current time for tie-breaking
    uint32_t timeSinceTick; // time since last bitstring update
    int maxFrames = 0; // maximum number of physical frames allocated before beginning page replacement
    int interval = 0; // interval for updating bitstrings
};

extern NFUState nfuState; // makes a global NFUState object accessible across multiple files

// Initializes the NFU state with the given number of frames and update interval
void initNFUState(int numFrames, int updateInterval);
// Called at beginning of each memory access to update virtual time and shift bitstring if interval reached
void beforeAccessNFU();
// updates page's last access time and marks it as accessed when a page is already loaded
void onHitNFU(uint32_t vpn);
// adds new page and initializes its bitstring when a page is not loaded
void onMissNFU(uint32_t vpn, int pfn);
// returns true if all frames are currently used
bool isFullNFU();
// selects victim page to evict based on bitstring, and in case of tie, last access time
int selectVictimNFU();
// reuses the victim page's frame for new VPN, returns old vpn and bitstring for logging
pair<uint32_t, uint16_t> reuseSlotNFU(int victimIndex, uint32_t newVPN);