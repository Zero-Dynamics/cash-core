// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_CONSENSUS_PARAMS_H
#define CASH_CONSENSUS_PARAMS_H

#include "uint256.h"

#include <map>
#include <string>

namespace Consensus
{
enum DeploymentPos {
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV,              // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_BIP147, // Deployment of BIP147 (NULLDUMMY)
    DEPLOYMENT_ISAUTOLOCKS, // Deployment of automatic IS locks for simple transactions
    MAX_VERSION_BITS_DEPLOYMENTS // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
    /** The number of past blocks (including the block under consideration) to be taken into account for locking in a fork. */
    int64_t nWindowSize;
    /** A number of blocks, in the range of 1..nWindowSize, which must signal for a fork in order to lock it in. */
    int64_t nThreshold;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nRewardsStart;
    int nFeeRewardStart; 
    int nFirstBlockSpacingSwitchBlock;
    int nMasternodePaymentsStartBlock;
    int nMinCountMasternodesPaymentStart;
    int nInstantSendConfirmationsRequired; // in blocks
    int nInstantSendKeepLock;              // in blocks
    int nBudgetPaymentsStartBlock;
    int nBudgetPaymentsCycleBlocks;
    int nBudgetPaymentsWindowBlocks;
    int nBudgetProposalEstablishingTime; // in seconds
    int nSuperblockStartBlock;
    uint256 nSuperblockStartHash;
    int nSuperblockCycle;     // in blocks
    int nGovernanceMinQuorum; // Min absolute vote count to trigger an action
    int nGovernanceFilterElements;
    int nMasternodeMinimumConfirmations;
    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;

    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargetting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;    
    uint32_t nRuleChangeActivationThresholdOld;
    uint32_t nRuleChangeActivationThresholdNew;
    uint32_t nMinerConfirmationWindow;    
    uint32_t nMinerConfirmationWindowNew;
    uint32_t nMinerConfirmationWindowOld;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetSpacingOld;
    int64_t nPowTargetSpacingNew;    
    int64_t nPowTargetTimespan;
    int64_t nPowAveragingWindow;    
    int64_t nPowAveragingWindowOld;
    int64_t nPowAveragingWindowNew;
    int64_t nPowMaxAdjustUp;
    int64_t nPowMaxAdjustDown;
    int64_t nUpdateDiffAlgoHeight;
    int64_t nMinimumChainWork;

    uint256 defaultAssumeValid;

    int64_t GetCurrentPowTargetSpacing(const int& nHeight) const {
        if (nHeight > nFirstBlockSpacingSwitchBlock)
            return nPowTargetSpacingNew;
        else
            return nPowTargetSpacingOld;
    }

    int64_t GetCurrentPowAveragingWindow(const int& nHeight) const {
        if (nHeight > nFirstBlockSpacingSwitchBlock)
            return nPowAveragingWindowNew;
        else
            return nPowAveragingWindowOld;
    }

    int64_t GetCurrentRuleChangeActivationThreshold(const int& nHeight) const {
        if (nHeight > nFirstBlockSpacingSwitchBlock)
            return nRuleChangeActivationThresholdNew;
        else
            return nRuleChangeActivationThresholdOld;
    }

    int64_t GetCurrentMinerConfirmationWindow(const int& nHeight) const {
        if (nHeight > nFirstBlockSpacingSwitchBlock)
            return nMinerConfirmationWindowNew;
        else
            return nMinerConfirmationWindowOld;
    }

    int64_t AveragingWindowTimespan(const int& nHeight) const { return GetCurrentPowAveragingWindow(nHeight) * GetCurrentPowTargetSpacing(nHeight); }
    int64_t MinActualTimespan(const int& nHeight) const { return (AveragingWindowTimespan(nHeight) * (100 - nPowMaxAdjustUp)) / 100; }
    int64_t MaxActualTimespan(const int& nHeight) const { return (AveragingWindowTimespan(nHeight) * (100 + nPowMaxAdjustDown)) / 100; }
    int64_t DifficultyAdjustmentInterval(const int& nHeight) const { return nPowTargetTimespan / GetCurrentPowTargetSpacing(nHeight); }
};
} // namespace Consensus

#endif // CASH_CONSENSUS_PARAMS_H