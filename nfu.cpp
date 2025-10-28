#include "nfu.h"
#include <iostream>
#include <cstdint>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <vector>

// Global NFU state object (defined in nfu.h)
NFUState nfuState;

using namespace std;

/*───────────────────────────────────────────────────────────────────────────────
  Internal helper: advance NFU "time" window.

  - Right-shifts each page's 16-bit bitstring (age/usage history).
  - If the page was accessed during the last interval, sets the MSB to 1.
  - Clears the 'accessed' set and resets the interval counter.

  Notes:
  * 0x8000 == 0b1000'0000'0000'0000 (MSB for a 16-bit value).
───────────────────────────────────────────────────────────────────────────────*/
static void tickNFU() {
    for (auto& page : nfuState.pages) {
        // Shift right by one to age the usage history
        uint16_t s = static_cast<uint16_t>(page.bitstring >> 1);

        // If this page was accessed during the interval, set MSB to 1
        if (nfuState.accessed.count(page.vpn)) {
            s = static_cast<uint16_t>(s | 0x8000);
        }

        page.bitstring = s;
    }

    // Prepare for the next interval
    nfuState.accessed.clear();
    nfuState.timeSinceTick = 0;
}

/*───────────────────────────────────────────────────────────────────────────────
  Initialize NFU state.

  @param numFrames       Maximum number of frames NFU can hold (capacity).
  @param updateInterval  Number of accesses per "tick" of the bit aging.
───────────────────────────────────────────────────────────────────────────────*/
void initNFUState(int numFrames, int updateInterval) {
    nfuState = NFUState{};          // reset all fields
    nfuState.maxFrames     = numFrames;
    nfuState.interval      = updateInterval;
    nfuState.currentTime   = 0;
    nfuState.timeSinceTick = 0;
}

/*───────────────────────────────────────────────────────────────────────────────
  Called before every memory access.

  - Advances virtual time counters.
  - Triggers a tick (bit aging) if we've reached the interval length.
───────────────────────────────────────────────────────────────────────────────*/
void beforeAccessNFU() {
    nfuState.currentTime++;
    nfuState.timeSinceTick++;

    if (nfuState.timeSinceTick >= nfuState.interval) {
        tickNFU();
    }
}

/*───────────────────────────────────────────────────────────────────────────────
  NFU hit handler.

  - Updates the page's last access time.
  - Marks the page as accessed in this interval (except exactly on tick boundary).

  @param vpn  Virtual page number that just hit.
───────────────────────────────────────────────────────────────────────────────*/
void onHitNFU(uint32_t vpn) {
    auto it = nfuState.vpnToIndex.find(vpn);
    if (it == nfuState.vpnToIndex.end()) return; // defensive: unknown page

    const size_t index = it->second;
    nfuState.pages[index].lastAccessTime = nfuState.currentTime;

    // If not precisely on the tick boundary, record the access for MSB set at tick
    if (nfuState.currentTime % nfuState.interval != 0) {
        nfuState.accessed.insert(vpn);
    }
}

/*───────────────────────────────────────────────────────────────────────────────
  NFU miss handler (page load).

  - Creates a new LoadedPage with MSB set (recently used).
  - Adds it to the page table and reverse index.
  - Marks it as accessed in this interval (except exactly on tick boundary).

  @param vpn  Virtual page number being loaded.
  @param pfn  Physical frame number assigned to this VPN.
───────────────────────────────────────────────────────────────────────────────*/
void onMissNFU(uint32_t vpn, int pfn) {
    LoadedPage newPage{
        pfn,
        vpn,
        static_cast<uint16_t>(0x8000), // recent use flagged in MSB
        nfuState.currentTime
    };

    nfuState.pages.push_back(newPage);
    nfuState.vpnToIndex[vpn] = nfuState.pages.size() - 1;

    if (nfuState.currentTime % nfuState.interval != 0) {
        nfuState.accessed.insert(vpn);
    }
}

/*───────────────────────────────────────────────────────────────────────────────
  Capacity check.

  @return true if all frames are currently in use; false otherwise.
───────────────────────────────────────────────────────────────────────────────*/
bool isFullNFU() {
    return nfuState.pages.size() >= static_cast<size_t>(nfuState.maxFrames);
}

/*───────────────────────────────────────────────────────────────────────────────
  Victim selection (Not Frequently Used):

  - Chooses the page with the smallest bitstring (least recent aggregate usage).
  - Ties broken by earliest lastAccessTime (older → evict first).
  - Final tie-breaker: smaller PFN (deterministic choice).

  @return index into nfuState.pages of the victim, or -1 if no pages loaded.
───────────────────────────────────────────────────────────────────────────────*/
int selectVictimNFU() {
    if (nfuState.pages.empty()) return -1;

    int victimIndex = 0;

    for (size_t i = 1; i < nfuState.pages.size(); i++) {
        const LoadedPage& cand = nfuState.pages[i];
        const LoadedPage& best = nfuState.pages[static_cast<size_t>(victimIndex)];

        const bool betterBitstring   = (cand.bitstring < best.bitstring);
        const bool equalBitstring    = (cand.bitstring == best.bitstring);
        const bool olderLastAccess   = (cand.lastAccessTime < best.lastAccessTime);
        const bool equalLastAccess   = (cand.lastAccessTime == best.lastAccessTime);
        const bool smallerPFN        = (cand.pfn < best.pfn);

        if (betterBitstring ||
            (equalBitstring && (olderLastAccess || (equalLastAccess && smallerPFN)))) {
            victimIndex = static_cast<int>(i);
        }
    }

    return victimIndex;
}

/*───────────────────────────────────────────────────────────────────────────────
  Reuse a victim slot for a new VPN.

  - Removes the victim's VPN from the index & 'accessed' set.
  - Overwrites the LoadedPage with the new VPN, resets bitstring (MSB=1),
    and updates lastAccessTime.
  - Adds new VPN to the index and marks it as accessed (except on tick boundary).

  @param victimIndex  Index of victim page in nfuState.pages.
  @param newVPN       VPN to place into victim's frame.

  @return {oldVPN, oldBitstring} for logging/reporting.
───────────────────────────────────────────────────────────────────────────────*/
pair<uint32_t, uint16_t> reuseSlotNFU(int victimIndex, uint32_t newVPN) {
    LoadedPage& victimPage = nfuState.pages[static_cast<size_t>(victimIndex)];

    const uint32_t oldVPN       = victimPage.vpn;
    const uint16_t oldBitstring = victimPage.bitstring;

    // Remove old mappings
    nfuState.vpnToIndex.erase(oldVPN);
    nfuState.accessed.erase(oldVPN);

    // Overwrite with new VPN, reset usage history and timestamp
    victimPage.vpn            = newVPN;
    victimPage.bitstring      = static_cast<uint16_t>(0x8000); // mark as recently used
    victimPage.lastAccessTime = nfuState.currentTime;

    // Add new mapping
    nfuState.vpnToIndex[newVPN] = victimIndex;

    if (nfuState.currentTime % nfuState.interval != 0) {
        nfuState.accessed.insert(newVPN);
    }

    return { oldVPN, oldBitstring };
}
