#include "nfu.h"

NFUState nfuState; // global NFU state object

// shifts touched pages' bitstrings and updates time since tick
static void tickNFU() {
    for (auto& page : nfuState.pages) {
        uint16_t s = (uint16_t)(page.bitstring >> 1); // right shift by 1
        // if page was accessed in this interval, right shift and set MSB to 1
        if (nfuState.accessed.count(page.vpn)) {
            s = s | 0x8000; // set MSB to 1 (0x8000 = 0b1000 0000 0000 0000)
        }
        page.bitstring = s;
    }
    nfuState.accessed.clear(); // clear accessed set for next interval
    nfuState.timeSinceTick = 0; // reset time since last tick
}

// Intializes NFU state with given number of frames and update interval
void initNFUState(int numFrames, int updateInterval) {
    nfuState = NFUState{};
    nfuState.maxFrames = numFrames;
    nfuState.interval = updateInterval;
    nfuState.currentTime   = 0;
    nfuState.timeSinceTick = 0;
}

// Called at beginning of each memory access to update virtual time and shift bitstring if interval reached 
void beforeAccessNFU() {
    nfuState.currentTime++;
    nfuState.timeSinceTick++;
    if (nfuState.timeSinceTick > nfuState.interval) {
        tickNFU();
    }
}

// updates a page's last access time and marks it as accessed when a page is already loaded 
void onHitNFU(uint32_t vpn) {
    auto it = nfuState.vpnToIndex.find(vpn);
    if (it != nfuState.vpnToIndex.end()) { // if iterator reaches end, vpn not found
        size_t index = it->second; // get index in vpnToIndex map
        nfuState.pages[index].lastAccessTime = nfuState.currentTime; // update last access time
        nfuState.accessed.insert(vpn); // mark page as accessed in this interval
    }
}

// adds new page and initializes its bitstring when a page is not loaded
void onMissNFU(uint32_t vpn, int pfn) {
    LoadedPage newPage{pfn, vpn, (uint16_t)(0x8000), nfuState.currentTime};
    nfuState.pages.push_back(newPage); // add new page to pages vector
    nfuState.vpnToIndex[vpn] = nfuState.pages.size()-1; // map vpn to index in vpnToIndex map
    nfuState.accessed.insert(vpn); // mark page as accessed in this interval
}

// returns true if all frames are currently used
bool isFullNFU() {
    return nfuState.pages.size() >= (size_t)nfuState.maxFrames;
}

// selects victim page to evict based on bitstring, and in case of tie, last access time
int selectVictimNFU() {
    if (nfuState.pages.empty()) return -1;
    int victimIndex = 0;
    for (size_t i = 1; i < nfuState.pages.size(); i++) {
        const LoadedPage& candidate1 = nfuState.pages[i];
        const LoadedPage& candidate2 = nfuState.pages[victimIndex];
        if (candidate1.bitstring < candidate2.bitstring ||
   (candidate1.bitstring == candidate2.bitstring && 
    (candidate1.lastAccessTime < candidate2.lastAccessTime ||
     (candidate1.lastAccessTime == candidate2.lastAccessTime && candidate1.pfn < candidate2.pfn))))
{
    victimIndex = (int)i;
}
    }
    return victimIndex;
}

// reuses the victim page's frame for new VPN, returns old vpn and bitstring for logging
pair<uint32_t, uint16_t> reuseSlotNFU(int victimIndex, uint32_t newVPN) {
    LoadedPage& victimPage = nfuState.pages[victimIndex];
    uint32_t oldVPN = victimPage.vpn;
    uint16_t oldBitstring = victimPage.bitstring;

    // remove old vpn from vpnToIndex map
    nfuState.vpnToIndex.erase(oldVPN);

    nfuState.accessed.erase(oldVPN);

    // update victim page with new VPN and reset bitstring and last access time
    victimPage.vpn = newVPN;
    victimPage.bitstring = (uint16_t)(0x8000); // set MSB to 1
    victimPage.lastAccessTime = nfuState.currentTime;

    // add new vpn to vpnToIndex map
    nfuState.vpnToIndex[newVPN] = victimIndex;

    // mark new page as accessed in this interval
    nfuState.accessed.insert(newVPN);

    return {oldVPN, oldBitstring};
}