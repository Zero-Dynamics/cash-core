// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2017 The Dash Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode-sync.h"

#include "activemasternode.h"
#include "checkpoints.h"
#include "masternode-payments.h"
#include "masternode.h"
#include "masternodeman.h"
#include "governance.h"
#include "init.h"
#include "netfulfilledman.h"
#include "netmessagemaker.h"
#include "spork.h"
#include "ui_interface.h"
#include "util.h"
#include "validation.h"

class CMasternodeSync;
CMasternodeSync masternodeSync;

void CMasternodeSync::Fail()
{
    nTimeLastFailure = GetTime();
    nRequestedMasternodeAssets = MASTERNODE_SYNC_FAILED;
}

void CMasternodeSync::Reset()
{
    nRequestedMasternodeAssets = MASTERNODE_SYNC_INITIAL;
    nRequestedMasternodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastBumped = GetTime();
    nTimeLastFailure = 0;
}

void CMasternodeSync::BumpAssetLastTime(const std::string strFuncName)
{
    if (IsSynced() || IsFailed())
        return;
    nTimeLastBumped = GetTime();
    LogPrint("mnsync", "CMasternodeSync::BumpAssetLastTime -- %s\n", strFuncName);
}

std::string CMasternodeSync::GetAssetName()
{
    switch (nRequestedMasternodeAssets) {
    case (MASTERNODE_SYNC_INITIAL):
        return "MASTERNODE_SYNC_INITIAL";
    case (MASTERNODE_SYNC_WAITING):
        return "MASTERNODE_SYNC_WAITING";
    case (MASTERNODE_SYNC_LIST):
        return "MASTERNODE_SYNC_LIST";
    case (MASTERNODE_SYNC_MNW):
        return "MASTERNODE_SYNC_MNW";
    case (MASTERNODE_SYNC_GOVERNANCE):
        return "MASTERNODE_SYNC_GOVERNANCE";
    case (MASTERNODE_SYNC_FAILED):
        return "MASTERNODE_SYNC_FAILED";
    case MASTERNODE_SYNC_FINISHED:
        return "MASTERNODE_SYNC_FINISHED";
    default:
        return "UNKNOWN";
    }
}

void CMasternodeSync::SwitchToNextAsset(CConnman& connman)
{
    switch (nRequestedMasternodeAssets) {
    case (MASTERNODE_SYNC_FAILED):
        throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
        break;
    case (MASTERNODE_SYNC_INITIAL):
        nRequestedMasternodeAssets = MASTERNODE_SYNC_WAITING;
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
        break;
    case (MASTERNODE_SYNC_WAITING):
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Completed %s in %llds\n", GetAssetName(), GetTime() - nTimeAssetSyncStarted);
        nRequestedMasternodeAssets = MASTERNODE_SYNC_LIST;
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
        break;
    case (MASTERNODE_SYNC_LIST):
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Completed %s in %llds\n", GetAssetName(), GetTime() - nTimeAssetSyncStarted);
        nRequestedMasternodeAssets = MASTERNODE_SYNC_MNW;
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
        break;
    case (MASTERNODE_SYNC_MNW):
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Completed %s in %llds\n", GetAssetName(), GetTime() - nTimeAssetSyncStarted);
        nRequestedMasternodeAssets = MASTERNODE_SYNC_GOVERNANCE;
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
        break;
    case (MASTERNODE_SYNC_GOVERNANCE):
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Completed %s in %llds\n", GetAssetName(), GetTime() - nTimeAssetSyncStarted);
        nRequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;
        uiInterface.NotifyAdditionalDataSyncProgressChanged(1);
        //try to activate our masternode if possible
        activeMasternode.ManageState(connman);

        // TODO: Find out whether we can just use LOCK instead of:
        // TRY_LOCK(cs_vNodes, lockRecv);
        // if(lockRecv) { ... }

        connman.ForEachNode(CConnman::AllNodes, [](CNode* pnode) {
            netfulfilledman.AddFulfilledRequest(pnode->addr, "full-sync");
        });
        LogPrint("masternode", "CMasternodeSync::SwitchToNextAsset -- Sync has finished\n");

        break;
    }
    nRequestedMasternodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    BumpAssetLastTime("CMasternodeSync::SwitchToNextAsset");
}

std::string CMasternodeSync::GetSyncStatus()
{
    switch (masternodeSync.nRequestedMasternodeAssets) {
    case MASTERNODE_SYNC_INITIAL:
        return _("Synchronizing blockchain...");
    case MASTERNODE_SYNC_WAITING:
        return _("Synchronization pending...");
    case MASTERNODE_SYNC_LIST:
        return _("Synchronizing Masternodes...");
    case MASTERNODE_SYNC_MNW:
        return _("Synchronizing Masternode payments...");
    case MASTERNODE_SYNC_GOVERNANCE:
        return _("Synchronizing governance objects...");
    case MASTERNODE_SYNC_FAILED:
        return _("Synchronization failed");
    case MASTERNODE_SYNC_FINISHED:
        return _("Synchronization finished");
    default:
        return "";
    }
}

void CMasternodeSync::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if (IsSynced() || IsFailed())
            return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        LogPrint("masternode", "SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->id);
    }
}

double CMasternodeSync::SyncProgress()
{
    // Calculate additional data "progress" for syncstatus RPC call
    double nSyncProgress = double(nRequestedMasternodeAttempt + (nRequestedMasternodeAssets - 1) * 8) / (8 * 4);
    if (nSyncProgress < 0)
        nSyncProgress = 0;

    if (nSyncProgress > 1)
        nSyncProgress = 1;

    return nSyncProgress;
}

void CMasternodeSync::ProcessTick(CConnman& connman)
{
    static int nTick = 0;
    nTick++;

    // reset the sync process if the last call to this function was more than 60 minutes ago (client was in sleep mode)
    static int64_t nTimeLastProcess = GetTime();
    if (GetTime() - nTimeLastProcess > 60 * 60) {
        LogPrintf("CMasternodeSync::HasSyncFailures -- WARNING: no actions for too long, restarting sync...\n");
        Reset();
        SwitchToNextAsset(connman);
        nTimeLastProcess = GetTime();
        return;
    }
    nTimeLastProcess = GetTime();

    // reset sync status in case of any other sync failure
    if (IsFailed()) {
        if (nTimeLastFailure + (1 * 60) < GetTime()) { // 1 minute cooldown after failed sync
            LogPrintf("CMasternodeSync::HasSyncFailures -- WARNING: failed to sync, trying again...\n");
            Reset();
            SwitchToNextAsset(connman);
        }
        return;
    }

    // gradually request the rest of the votes after sync finished
    if (IsSynced()) {
        std::vector<CNode*> vNodesCopy = connman.CopyNodeVector(CConnman::FullyConnectedOnly);
        governance.RequestGovernanceObjectVotes(vNodesCopy, connman);
        connman.ReleaseNodeVector(vNodesCopy);
        return;
    }

    // Calculate "progress" for LOG reporting / GUI notification
    double nSyncProgress = double(nRequestedMasternodeAttempt + (nRequestedMasternodeAssets - 1) * 8) / (8 * 4);
    LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nRequestedMasternodeAttempt %d nSyncProgress %f\n", nTick, nRequestedMasternodeAssets, nRequestedMasternodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(nSyncProgress);

    std::vector<CNode*> vNodesCopy = connman.CopyNodeVector(CConnman::FullyConnectedOnly);

    for (auto& pnode : vNodesCopy) {
        CNetMsgMaker msgMaker(pnode->GetSendVersion());
        // Don't try to sync any data from outbound "masternode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "masternode" connection
        // initialted from another node, so skip it too.
        if (pnode->fMasternode || (fMasternodeMode && pnode->fInbound))
            continue;
        // QUICK MODE (REGTEST ONLY!)
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            if (nRequestedMasternodeAttempt <= 2) {
                connman.PushMessage(pnode, msgMaker.Make(NetMsgType::GETSPORKS)); //get current network sporks
            } else if (nRequestedMasternodeAttempt < 4) {
                mnodeman.PsegUpdate(pnode, connman);
            } else if (nRequestedMasternodeAttempt < 6) {
                //sync payment votes
                if (pnode->nVersion == 70900) {
                    connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC, mnpayments.GetStorageLimit())); //sync payment votes
                } else {
                    connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC)); //sync payment votes
                }
                SendGovernanceSyncRequest(pnode, connman);
            } else {
                nRequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;
            }
            nRequestedMasternodeAttempt++;
            connman.ReleaseNodeVector(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if (netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                LogPrint("masternode", "CMasternodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->id);
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC

            if (!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // always get sporks first, only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                connman.PushMessage(pnode, msgMaker.Make(NetMsgType::GETSPORKS));
                LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedMasternodeAssets, pnode->id);
            }

            // INITIAL TIMEOUT

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_WAITING) {
                if (GetTime() - nTimeLastBumped > MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    // At this point we know that:
                    // a) there are peers (because we are looping on at least one of them);
                    // b) we waited for at least MASTERNODE_SYNC_TIMEOUT_SECONDS since we reached
                    //    the headers tip the last time (i.e. since we switched from
                    //     MASTERNODE_SYNC_INITIAL to MASTERNODE_SYNC_WAITING and bumped time);
                    // c) there were no blocks (UpdatedBlockTip, NotifyHeaderTip) or headers (AcceptedBlockHeader)
                    //    for at least MASTERNODE_SYNC_TIMEOUT_SECONDS.
                    // We must be at the tip already, let's move to the next asset.
                    SwitchToNextAsset(connman);
                }
            }

            // MNLIST : SYNC MASTERNODE LIST FROM OTHER CONNECTED CLIENTS

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_LIST) {
                LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nTimeLastBumped %lld GetTime() %lld diff %lld\n", nTick, nRequestedMasternodeAssets, nTimeLastBumped, GetTime(), GetTime() - nTimeLastBumped);
                // check for timeout first
                if (GetTime() - nTimeLastBumped > MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- timeout\n", nTick, nRequestedMasternodeAssets);
                    if (nRequestedMasternodeAttempt == 0) {
                        LogPrintf("CMasternodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without Masternode list, fail here and try later
                        Fail();
                        connman.ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset(connman);
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // request from eight peers max
                if (nRequestedMasternodeAttempt > 7) {
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "masternode-list-sync"))
                    continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "masternode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinMasternodePaymentsProto())
                    continue;
                nRequestedMasternodeAttempt++;

                mnodeman.PsegUpdate(pnode, connman);

                connman.ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC MASTERNODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_MNW) {
                LogPrint("mnpayments", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nTimeLastBumped %lld GetTime() %lld diff %lld\n", nTick, nRequestedMasternodeAssets, nTimeLastBumped, GetTime(), GetTime() - nTimeLastBumped);
                // check for timeout first
                // This might take a lot longer than MASTERNODE_SYNC_TIMEOUT_SECONDS due to new blocks,
                // but that should be OK and it should timeout eventually.
                if (GetTime() - nTimeLastBumped > MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- timeout\n", nTick, nRequestedMasternodeAssets);
                    if (nRequestedMasternodeAttempt == 0) {
                        LogPrintf("CMasternodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        connman.ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset(connman);
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }
                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if (nRequestedMasternodeAttempt > 1 && mnpayments.IsEnoughData()) {
                    LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- found enough data\n", nTick, nRequestedMasternodeAssets);
                    SwitchToNextAsset(connman);
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // request from eight peers max
                if (nRequestedMasternodeAttempt > 7) {
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "masternode-payment-sync"))
                    continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "masternode-payment-sync");

                if (pnode->nVersion < mnpayments.GetMinMasternodePaymentsProto())
                    continue;
                nRequestedMasternodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                if (pnode->nVersion == 70900) {
                    connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC, mnpayments.GetStorageLimit()));
                } else {
                    connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC));
                } // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode, connman);

                connman.ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // GOVOBJ : SYNC GOVERNANCE ITEMS FROM OUR PEERS

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_GOVERNANCE) {
                LogPrint("gobject", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nTimeLastBumped %lld GetTime() %lld diff %lld\n", nTick, nRequestedMasternodeAssets, nTimeLastBumped, GetTime(), GetTime() - nTimeLastBumped);

                // check for timeout first
                if (GetTime() - nTimeLastBumped > MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- timeout\n", nTick, nRequestedMasternodeAssets);
                    if (nRequestedMasternodeAttempt == 0) {
                        LogPrintf("CMasternodeSync::ProcessTick -- WARNING: failed to sync %s\n", GetAssetName());
                        // it's kind of ok to skip this for now, hopefully we'll catch up later?
                    }
                    SwitchToNextAsset(connman);
                    connman.ReleaseNodeVector(vNodesCopy);
                    return;
                }
                // only request obj sync once from each peer, then request votes on per-obj basis
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "governance-sync")) {
                    governance.RequestGovernanceObjectVotes(pnode, connman);
                    int nObjsLeftToAsk = governance.RequestGovernanceObjectVotes(pnode, connman);
                    static int64_t nTimeNoObjectsLeft = 0;
                    // check for data
                    if (nObjsLeftToAsk == 0) {
                        static int nLastTick = 0;
                        static int nLastVotes = 0;
                        if (nTimeNoObjectsLeft == 0) {
                            // asked all objects for votes for the first time
                            nTimeNoObjectsLeft = GetTime();
                        }
                        // make sure the condition below is checked only once per tick
                        if (nLastTick == nTick)
                            continue;
                        if (GetTime() - nTimeNoObjectsLeft > MASTERNODE_SYNC_TIMEOUT_SECONDS &&
                            governance.GetVoteCount() - nLastVotes < std::max(int(0.0001 * nLastVotes), MASTERNODE_SYNC_TICK_SECONDS)) {
                            // We already asked for all objects, waited for MASTERNODE_SYNC_TIMEOUT_SECONDS
                            // after that and less then 0.01% or MASTERNODE_SYNC_TICK_SECONDS
                            // (i.e. 1 per second) votes were recieved during the last tick.
                            // We can be pretty sure that we are done syncing.
                            LogPrint("masternode", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- asked for all objects, nothing to do\n", nTick, nRequestedMasternodeAssets);
                            // reset nTimeNoObjectsLeft to be able to use the same condition on resync
                            nTimeNoObjectsLeft = 0;
                            SwitchToNextAsset(connman);
                            connman.ReleaseNodeVector(vNodesCopy);
                            return;
                        }
                        nLastTick = nTick;
                        nLastVotes = governance.GetVoteCount();
                    }
                    continue;
                }

                netfulfilledman.AddFulfilledRequest(pnode->addr, "governance-sync");

                if (pnode->nVersion < MIN_GOVERNANCE_PEER_PROTO_VERSION)
                    continue;
                nRequestedMasternodeAttempt++;

                SendGovernanceSyncRequest(pnode, connman);

                connman.ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }
        }
    }
    // looped through all nodes, release them
    connman.ReleaseNodeVector(vNodesCopy);
}

void CMasternodeSync::SendGovernanceSyncRequest(CNode* pnode, CConnman& connman)
{
    CNetMsgMaker msgMaker(pnode->GetSendVersion());

    if (pnode->nVersion >= GOVERNANCE_FILTER_PROTO_VERSION) {
        CBloomFilter filter;
        filter.clear();

        connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MNGOVERNANCESYNC, uint256(), filter));
    } else {
        connman.PushMessage(pnode, msgMaker.Make(NetMsgType::MNGOVERNANCESYNC, uint256()));
    }
}

void CMasternodeSync::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    LogPrint("mnsync", "CMasternodeSync::AcceptedBlockHeader -- pindexNew->nHeight: %d\n", pindexNew->nHeight);

    if (!IsBlockchainSynced()) {
        // Postpone timeout each time new block header arrives while we are still syncing blockchain
        BumpAssetLastTime("CMasternodeSync::AcceptedBlockHeader");
    }
}

void CMasternodeSync::NotifyHeaderTip(const CBlockIndex* pindexNew, bool fInitialDownload, CConnman& connman)
{
    LogPrint("mnsync", "CMasternodeSync::NotifyHeaderTip -- pindexNew->nHeight: %d fInitialDownload=%d\n", pindexNew->nHeight, fInitialDownload);

    if (IsFailed() || IsSynced() || !pindexBestHeader)
        return;

    if (!IsBlockchainSynced()) {
        // Postpone timeout each time new block arrives while we are still syncing blockchain
        BumpAssetLastTime("CMasternodeSync::NotifyHeaderTip");
    }
}

void CMasternodeSync::UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload, CConnman& connman)
{
    LogPrint("mnsync", "CMasternodeSync::UpdatedBlockTip -- pindexNew->nHeight: %d fInitialDownload=%d\n", pindexNew->nHeight, fInitialDownload);

    if (IsFailed() || IsSynced() || !pindexBestHeader)
        return;

    if (!IsBlockchainSynced()) {
        // Postpone timeout each time new block arrives while we are still syncing blockchain
        BumpAssetLastTime("CMasternodeSync::UpdatedBlockTip");
    }

    if (fInitialDownload) {
        // switched too early
        if (IsBlockchainSynced()) {
            Reset();
        }

        // no need to check any further while still in IBD mode
        return;
    }

    // Note: since we sync headers first, it should be ok to use this
    static bool fReachedBestHeader = false;
    bool fReachedBestHeaderNew = pindexNew->GetBlockHash() == pindexBestHeader->GetBlockHash();

    if (fReachedBestHeader && !fReachedBestHeaderNew) {
        // Switching from true to false means that we previousely stuck syncing headers for some reason,
        // probably initial timeout was not enough,
        // because there is no way we can update tip not having best header
        Reset();
        fReachedBestHeader = false;
        return;
    }

    fReachedBestHeader = fReachedBestHeaderNew;

    LogPrint("mnsync", "CMasternodeSync::NotifyHeaderTip -- pindexNew->nHeight: %d pindexBestHeader->nHeight: %d fInitialDownload=%d fReachedBestHeader=%d\n",
        pindexNew->nHeight, pindexBestHeader->nHeight, fInitialDownload, fReachedBestHeader);

    if (!IsBlockchainSynced() && fReachedBestHeader) {
        if (fLiteMode) {
            // nothing to do in lite mode, just finish the process immediately
            nRequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;
            return;
        }
        // Reached best header while being in initial mode.
        // We must be at the tip already, let's move to the next asset.
        SwitchToNextAsset(connman);
    }
}

void CMasternodeSync::DoMaintenance(CConnman &connman)
{
    if (ShutdownRequested()) return;
     ProcessTick(connman);
}
