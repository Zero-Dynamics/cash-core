// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "crypto/common.h"
#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

#include <string>

uint256 CBlockHeader::GetHash() const
{
    // UpdateCurrentBlockTime
    CurrentBlockTime::UpdateCurrentBlockTime(nTime);

    // Retrieve network parameters
    std::string network = ChainNameFromCommandLine();
    SelectParams(network);

    // Get the current block time
    uint64_t currentTime = nTime;

    // Determine Argon2d phase
    int hashPhase = 0;
    const uint64_t FirstSwitchTime = Params().FirstArgon2SwitchTime();
    const uint64_t SecondSwitchTime = Params().SecondArgon2SwitchTime();
    const uint64_t ThirdSwitchTime = Params().ThirdArgon2SwitchTime();

    if (currentTime >= FirstSwitchTime && currentTime < SecondSwitchTime) {
        hashPhase = 1;
    } else if (currentTime >= SecondSwitchTime && currentTime < ThirdSwitchTime) {
        hashPhase = 2;
    } else if (currentTime >= ThirdSwitchTime) {
        hashPhase = 3;
    }

    // Return the hash using the determined Argon2d phase
    return hash_Argon2d(BEGIN(nVersion), END(nNonce), hashPhase);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        s << "  " << vtx[i]->ToString() << "\n";
    }
    return s.str();
}

namespace CurrentBlockTime
{
    std::atomic<uint64_t> currentBlockTime{0};
    void UpdateCurrentBlockTime(uint64_t nTime) {
        CurrentBlockTime::currentBlockTime.store(nTime, std::memory_order_relaxed);
    }
    uint64_t GetCurrentBlockTime() {
        return currentBlockTime.load(std::memory_order_acquire);
    }
}
