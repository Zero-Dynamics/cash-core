// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2017 The Dash Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenode.h"

#include "activeservicenode.h"
#include "base58.h"
#include "chain.h"
#include "clientversion.h"
#include "servicenode-payments.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "fluid/fluiddb.h"
#include "init.h"
#include "messagesigner.h"
#include "netbase.h"
#include "script/standard.h"
#include "util.h"
#include "validation.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif // ENABLE_WALLET

#include <boost/lexical_cast.hpp>

CServiceNode::CServiceNode() : servicenode_info_t{SERVICENODE_ENABLED, PROTOCOL_VERSION, GetAdjustedTime()},
                     fAllowMixingTx(true)
{
}

CServiceNode::CServiceNode(CService addr, COutPoint outpoint, CPubKey pubKeyCollateralAddress, CPubKey pubKeyServiceNode, int nProtocolVersionIn) : servicenode_info_t{SERVICENODE_ENABLED, nProtocolVersionIn, GetAdjustedTime(),
                                                                                                                                         outpoint, addr, pubKeyCollateralAddress, pubKeyServiceNode},
                                                                                                                                     fAllowMixingTx(true)
{
}

CServiceNode::CServiceNode(const CServiceNode& other) : servicenode_info_t{other},
                                         lastPing(other.lastPing),
                                         vchSig(other.vchSig),
                                         nCollateralMinConfBlockHash(other.nCollateralMinConfBlockHash),
                                         nBlockLastPaid(other.nBlockLastPaid),
                                         nPoSeBanScore(other.nPoSeBanScore),
                                         nPoSeBanHeight(other.nPoSeBanHeight),
                                         fAllowMixingTx(other.fAllowMixingTx),
                                         fUnitTest(other.fUnitTest)
{
}

CServiceNode::CServiceNode(const CServiceNodeBroadcast& snb) : servicenode_info_t{snb.nActiveState, snb.nProtocolVersion, snb.sigTime,
                                                    snb.outpoint, snb.addr, snb.pubKeyCollateralAddress, snb.pubKeyServiceNode},
                                                lastPing(snb.lastPing),
                                                vchSig(snb.vchSig),
                                                fAllowMixingTx(true)
{
}

//
// When a new ServiceNode broadcast is sent, update our information
//
bool CServiceNode::UpdateFromNewBroadcast(CServiceNodeBroadcast& snb, CConnman& connman)
{
    if (snb.sigTime <= sigTime && !snb.fRecovery)
        return false;

    pubKeyServiceNode = snb.pubKeyServiceNode;
    sigTime = snb.sigTime;
    vchSig = snb.vchSig;
    nProtocolVersion = snb.nProtocolVersion;
    addr = snb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (!snb.lastPing || (snb.lastPing && snb.lastPing.CheckAndUpdate(this, true, nDos, connman))) {
        lastPing = snb.lastPing;
        snodeman.mapSeenServiceNodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our ServiceNode privkey...
    if (fServiceNodeMode && pubKeyServiceNode == activeServiceNode.pubKeyServiceNode) {
        nPoSeBanScore = -SERVICENODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeServiceNode.ManageState(connman);
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CServiceNode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your SN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a ServiceNode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CServiceNode::CalculateScore(const uint256& blockHash) const
{
    // Deterministically calculate a "score" for a ServiceNode based on any given (block)hash
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << outpoint << nCollateralMinConfBlockHash << blockHash;
    return UintToArith256(ss.GetHash());
}

CServiceNode::CollateralStatus CServiceNode::CheckCollateral(const COutPoint& outpoint, const CPubKey& pubkey)
{
    int nHeight;
    return CheckCollateral(outpoint, pubkey, nHeight);
}

CServiceNode::CollateralStatus CServiceNode::CheckCollateral(const COutPoint& outpoint, const CPubKey& pubkey, int& nHeightRet)
{
    AssertLockHeld(cs_main);

    Coin coin;
    if (!GetUTXOCoin(outpoint, coin)) {
        return COLLATERAL_UTXO_NOT_FOUND;
    }

    if (coin.out.nValue != 15000 * COIN) {
        return COLLATERAL_INVALID_AMOUNT;
    }

    if (pubkey == CPubKey() || coin.out.scriptPubKey != GetScriptForDestination(pubkey.GetID())) {
        return COLLATERAL_INVALID_PUBKEY;
    }

    nHeightRet = coin.nHeight;
    return COLLATERAL_OK;
}

void CServiceNode::Check(bool fForce)
{
    AssertLockHeld(cs_main);
    LOCK(cs);

    if (ShutdownRequested())
        return;

    if (!fForce && (GetTime() - nTimeLastChecked < SERVICENODE_CHECK_SECONDS))
        return;
    nTimeLastChecked = GetTime();

    LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state\n", outpoint.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent())
        return;

    int nHeight = 0;
    if (!fUnitTest) {
        Coin coin;
        if (!GetUTXOCoin(outpoint, coin)) {
            nActiveState = SERVICENODE_OUTPOINT_SPENT;
            LogPrint("servicenode", "CServiceNode::Check -- Failed to find ServiceNode UTXO, servicenode=%s\n", outpoint.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight)
            return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // ServiceNode still will be on the edge and can be banned back easily if it keeps ignoring snverify
        // or connect attempts. Will require few snverify messages to strengthen its position in sn list.
        LogPrintf("CServiceNode::Check -- ServiceNode %s is unbanned and back in list now\n", outpoint.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= SERVICENODE_POSE_BAN_MAX_SCORE) {
        nActiveState = SERVICENODE_POSE_BAN;
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + snodeman.size();
        LogPrintf("CServiceNode::Check -- ServiceNode %s is banned till block %d now\n", outpoint.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurServiceNode = fServiceNodeMode && activeServiceNode.pubKeyServiceNode == pubKeyServiceNode;
    // ServiceNode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < snpayments.GetMinServiceNodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurServiceNode && nProtocolVersion < PROTOCOL_VERSION);

    if (fRequireUpdate) {
        nActiveState = SERVICENODE_UPDATE_REQUIRED;
        if (nActiveStatePrev != nActiveState) {
            LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old ServiceNodes on start, give them a chance to receive updates...
    bool fWaitForPing = !servicenodeSync.IsServiceNodeListSynced() && !IsPingedWithin(SERVICENODE_MIN_SNP_SECONDS);

    if (fWaitForPing && !fOurServiceNode) {
        // ...but if it was already expired before the initial check - return right away
        bool isSentinelPingExpired = false;
        if (sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG))
            isSentinelPingExpired = IsSentinelPingExpired();

        if (IsExpired() || isSentinelPingExpired || IsNewStartRequired()) {
            LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state, waiting for ping\n", outpoint.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own servicenode
    if (!fWaitForPing || fOurServiceNode) {
        if (!IsPingedWithin(SERVICENODE_NEW_START_REQUIRED_SECONDS)) {
            nActiveState = SERVICENODE_NEW_START_REQUIRED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(SERVICENODE_EXPIRATION_SECONDS)) {
            nActiveState = SERVICENODE_EXPIRED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
            }
            return;
        }
        if (sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG)) {
            // part 1: expire based on cashd ping
            bool fSentinelPingActive = servicenodeSync.IsSynced() && snodeman.IsSentinelPingActive();
            bool fSentinelPingExpired = fSentinelPingActive && !IsPingedWithin(SERVICENODE_SENTINEL_PING_MAX_SECONDS);
            LogPrint("servicenode", "CServiceNode::Check -- outpoint=%s, GetAdjustedTime()=%d, fSentinelPingExpired=%d\n",
                outpoint.ToStringShort(), GetAdjustedTime(), fSentinelPingExpired);

            if (fSentinelPingExpired) {
                nActiveState = SERVICENODE_SENTINEL_PING_EXPIRED;
                if (nActiveStatePrev != nActiveState) {
                    LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
                }
                return;
            }
        }
    }

    // We require SNs to be in PRE_ENABLED until they either start to expire or receive a ping and go into ENABLED state
    // Works on mainnet/testnet only and not the case on regtest/devnet.
    if (Params().NetworkIDString() != CBaseChainParams::REGTEST) {
        if (lastPing.sigTime - sigTime < SERVICENODE_MIN_SNP_SECONDS) {
            nActiveState = SERVICENODE_PRE_ENABLED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (!fWaitForPing || fOurServiceNode) {
        // part 2: expire based on sentinel info
        if (sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG)) {
            bool fSentinelPingActive = servicenodeSync.IsSynced() && snodeman.IsSentinelPingActive();
            bool fSentinelPingExpired = fSentinelPingActive && !lastPing.fSentinelIsCurrent;

            LogPrint("servicenode", "CServiceNode::Check -- outpoint=%s, GetAdjustedTime()=%d, fSentinelPingExpired=%d\n",
                outpoint.ToStringShort(), GetAdjustedTime(), fSentinelPingExpired);

            if (fSentinelPingExpired) {
                nActiveState = SERVICENODE_SENTINEL_PING_EXPIRED;
                if (nActiveStatePrev != nActiveState) {
                    LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
                }
                return;
            }
        }
    }

    nActiveState = SERVICENODE_ENABLED; // OK
    if (nActiveStatePrev != nActiveState) {
        LogPrint("servicenode", "CServiceNode::Check -- ServiceNode %s is in %s state now\n", outpoint.ToStringShort(), GetStateString());
    }
}


bool CServiceNode::IsValidNetAddr()
{
    return IsValidNetAddr(addr);
}

bool CServiceNode::IsValidNetAddr(CService addrIn)
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && !addrIn.IsIPv6() && IsReachable(addrIn) && addrIn.IsRoutable());
}

servicenode_info_t CServiceNode::GetInfo() const
{
    servicenode_info_t info{*this};
    info.nTimeLastPing = lastPing.sigTime;
    info.fInfoValid = true;
    return info;
}

std::string CServiceNode::StateToString(int nStateIn)
{
    switch (nStateIn) {
    case SERVICENODE_PRE_ENABLED:
        return "PRE_ENABLED";
    case SERVICENODE_ENABLED:
        return "ENABLED";
    case SERVICENODE_EXPIRED:
        return "EXPIRED";
    case SERVICENODE_OUTPOINT_SPENT:
        return "OUTPOINT_SPENT";
    case SERVICENODE_UPDATE_REQUIRED:
        return "UPDATE_REQUIRED";
    case SERVICENODE_SENTINEL_PING_EXPIRED:
        return "SENTINEL_PING_EXPIRED";
    case SERVICENODE_NEW_START_REQUIRED:
        return "NEW_START_REQUIRED";
    case SERVICENODE_POSE_BAN:
        return "POSE_BAN";
    default:
        return "UNKNOWN";
    }
}

std::string CServiceNode::GetStateString() const
{
    return StateToString(nActiveState);
}

std::string CServiceNode::GetStatus() const
{
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

void CServiceNode::UpdateLastPaid(const CBlockIndex* pindex, int nMaxBlocksToScanBack)
{
    if (!pindex)
        return;

    const CBlockIndex* BlockReading = pindex;

    CScript snpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    // LogPrint("servicenode", "CServiceNode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapServiceNodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
        if (snpayments.mapServiceNodeBlocks.count(BlockReading->nHeight) &&
            snpayments.mapServiceNodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(snpayee, 2)) {
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
                continue;

            CAmount nServiceNodePayment = GetFluidServiceNodeReward(BlockReading->nHeight);

            for (const auto& txout : block.vtx[0]->vout)
                if (snpayee == txout.scriptPubKey && nServiceNodePayment == txout.nValue) {
                    nBlockLastPaid = BlockReading->nHeight;
                    nTimeLastPaid = BlockReading->nTime;
                    LogPrint("servicenode", "CServiceNode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", outpoint.ToStringShort(), nBlockLastPaid);
                    return;
                }
        }

        if (BlockReading->pprev == nullptr) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this ServiceNode wasn't found in latest snpayments blocks
    // or it was found in snpayments blocks but wasn't found in the blockchain.
    // LogPrint("servicenode", "CServiceNode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

#ifdef ENABLE_WALLET
bool CServiceNodeBroadcast::Create(const std::string strService, const std::string strKeyServiceNode, const std::string strTxHash, const std::string strOutputIndex, std::string& strErrorRet, CServiceNodeBroadcast& snbRet, bool fOffline)
{
    COutPoint outpoint;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyServiceNodeNew;
    CKey keyServiceNodeNew;

    auto Log = [&strErrorRet](std::string sErr) -> bool {
        strErrorRet = sErr;
        LogPrintf("CServiceNodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    };

    // Wait for sync to finish because snb simply won't be relayed otherwise
    if (!fOffline && !servicenodeSync.IsSynced())
        return Log("Sync in progress. Must wait until sync is complete to start ServiceNode");

    if (!CMessageSigner::GetKeysFromSecret(strKeyServiceNode, keyServiceNodeNew, pubKeyServiceNodeNew))
        return Log(strprintf("Invalid ServiceNode key %s", strKeyServiceNode));

    if (!pwalletMain->GetServiceNodeOutpointAndKeys(outpoint, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex))
        return Log(strprintf("Could not allocate outpoint %s:%s for servicenode %s", strTxHash, strOutputIndex, strService));

    CService service;
    if (!Lookup(strService.c_str(), service, 0, false))
        return Log(strprintf("Invalid address %s for servicenode.", strService));
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort)
            return Log(strprintf("Invalid port %u for servicenode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort));
    } else if (service.GetPort() == mainnetDefaultPort)
        return Log(strprintf("Invalid port %u for servicenode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort));

    return Create(outpoint, service, keyCollateralAddressNew, pubKeyCollateralAddressNew, keyServiceNodeNew, pubKeyServiceNodeNew, strErrorRet, snbRet);
}

bool CServiceNodeBroadcast::Create(const COutPoint& outpoint, const CService& service, const CKey& keyCollateralAddressNew, const CPubKey& pubKeyCollateralAddressNew, const CKey& keyServiceNodeNew, const CPubKey& pubKeyServiceNodeNew, std::string& strErrorRet, CServiceNodeBroadcast& snbRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex)
        return false;

    LogPrint("servicenode", "CServiceNodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyServiceNodeNew.GetID() = %s\n",
        CDebitAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
        pubKeyServiceNodeNew.GetID().ToString());

    auto Log = [&strErrorRet, &snbRet](std::string sErr) -> bool {
        strErrorRet = sErr;
        LogPrintf("CServiceNodeBroadcast::Create -- %s\n", strErrorRet);
        snbRet = CServiceNodeBroadcast();
        return false;
    };

    CServiceNodePing snp(outpoint);
    if (!snp.Sign(keyServiceNodeNew, pubKeyServiceNodeNew))
        return Log(strprintf("Failed to sign ping, servicenode=%s", outpoint.ToStringShort()));

    snbRet = CServiceNodeBroadcast(service, outpoint, pubKeyCollateralAddressNew, pubKeyServiceNodeNew, PROTOCOL_VERSION);

    if (!snbRet.IsValidNetAddr())
        return Log(strprintf("Invalid IP address, servicenode=%s", outpoint.ToStringShort()));

    snbRet.lastPing = snp;
    if (!snbRet.Sign(keyCollateralAddressNew))
        return Log(strprintf("Failed to sign broadcast, servicenode=%s", outpoint.ToStringShort()));


    return true;
}
#endif // ENABLE_WALLET

bool CServiceNodeBroadcast::SimpleCheck(int& nDos)
{
    nDos = 0;

    AssertLockHeld(cs_main);

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        LogPrint("servicenode", "CServiceNodeBroadcast::SimpleCheck -- Invalid addr, rejected: ServiceNode=%s  addr=%s\n",
            outpoint.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrint("servicenode", "CServiceNodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: ServiceNode=%s\n", outpoint.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (!lastPing || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        nActiveState = SERVICENODE_EXPIRED;
    }

    if (nProtocolVersion < snpayments.GetMinServiceNodePaymentsProto()) {
        LogPrint("servicenode", "CServiceNodeBroadcast::SimpleCheck -- outdated ServiceNode: ServiceNode=%s  nProtocolVersion=%d\n", outpoint.ToStringShort(), nProtocolVersion);
        nActiveState = SERVICENODE_UPDATE_REQUIRED;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrint("servicenode", "CServiceNodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyServiceNode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrint("servicenode", "CServiceNodeBroadcast::SimpleCheck -- pubKeyServiceNode has the wrong size\n");
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort)
            return false;
    } else if (addr.GetPort() == mainnetDefaultPort)
        return false;

    return true;
}

bool CServiceNodeBroadcast::Update(CServiceNode* psn, int& nDos, CConnman& connman)
{
    nDos = 0;

    AssertLockHeld(cs_main);

    if (psn->sigTime == sigTime && !fRecovery) {
        // mapSeenServiceNodeBroadcast in CServiceNodeMan::CheckSnbAndUpdateServiceNodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (psn->sigTime > sigTime) {
        LogPrint("servicenode", "CServiceNodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for ServiceNode %s %s\n",
            sigTime, psn->sigTime, outpoint.ToStringShort(), addr.ToString());
        return false;
    }

    psn->Check();

    // ServiceNode is banned by PoSe
    if (psn->IsPoSeBanned()) {
        LogPrint("servicenode", "CServiceNodeBroadcast::Update -- Banned by PoSe, ServiceNode=%s\n", outpoint.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (psn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrint("servicenode", "CServiceNodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrint("servicenode", "CServiceNodeBroadcast::Update -- CheckSignature() failed, servicenode=%s\n", outpoint.ToStringShort());
        return false;
    }

    // if there was no ServiceNode broadcast recently or if it matches our ServiceNode privkey...
    if (!psn->IsBroadcastedWithin(SERVICENODE_MIN_SNB_SECONDS) || (fServiceNodeMode && pubKeyServiceNode == activeServiceNode.pubKeyServiceNode)) {
        // take the newest entry
        LogPrint("servicenode", "CServiceNodeBroadcast::Update -- Got UPDATED ServiceNode entry: addr=%s\n", addr.ToString());
        if (psn->UpdateFromNewBroadcast(*this, connman)) {
            psn->Check();
            Relay(connman);
        }
        servicenodeSync.BumpAssetLastTime("CServiceNodeBroadcast::Update");
    }

    return true;
}

bool CServiceNodeBroadcast::CheckOutpoint(int& nDos)
{
    // we are a ServiceNode with the same vin (i.e. already activated) and this snb is ours (matches our ServiceNodes privkey)
    // so nothing to do here for us
    if (fServiceNodeMode && outpoint == activeServiceNode.outpoint && pubKeyServiceNode == activeServiceNode.pubKeyServiceNode) {
        return false;
    }

    AssertLockHeld(cs_main);

    int nHeight;
    CollateralStatus err = CheckCollateral(outpoint, pubKeyCollateralAddress, nHeight);
    if (err == COLLATERAL_UTXO_NOT_FOUND) {
        LogPrint("servicenode", "CServiceNodeBroadcast::CheckOutpoint -- Failed to find ServiceNode UTXO, servicenode=%s\n", outpoint.ToStringShort());
        return false;
    }

    if (err == COLLATERAL_INVALID_AMOUNT) {
        LogPrint("servicenode", "CServiceNodeBroadcast::CheckOutpoint -- ServiceNode UTXO should have 1000 0DYNC, servicenode=%s\n", outpoint.ToStringShort());
        nDos = 33;
        return false;
    }

    if (err == COLLATERAL_INVALID_PUBKEY) {
        LogPrint("servicenode", "CServiceNodeBroadcast::CheckOutpoint -- ServiceNode UTXO should match pubKeyCollateralAddress, servicenode=%s\n", outpoint.ToStringShort());
        nDos = 33;
        return false;
    }

    if (chainActive.Height() - nHeight + 1 < Params().GetConsensus().nServiceNodeMinimumConfirmations) {
        LogPrintf("CServiceNodeBroadcast::CheckOutpoint -- ServiceNode UTXO must have at least %d confirmations, servicenode=%s\n",
            Params().GetConsensus().nServiceNodeMinimumConfirmations, outpoint.ToStringShort());
        // UTXO is legit but has not enough confirmations.
        // Maybe we miss few blocks, let this snb be checked again later.
        snodeman.mapSeenServiceNodeBroadcast.erase(GetHash());
        return false;
    }

    LogPrint("servicenode", "CServiceNodeBroadcast::CheckOutpoint -- ServiceNode UTXO verified\n");

    // Verify that sig time is legit, should be at least not earlier than the timestamp of the block
    // at which collateral became nServiceNodeMinimumConfirmations blocks deep.
    // NOTE: this is not accurate because block timestamp is NOT guaranteed to be 100% correct one.
    CBlockIndex* pRequredConfIndex = chainActive[nHeight + Params().GetConsensus().nServiceNodeMinimumConfirmations - 1]; // block where tx got nServiceNodeMinimumConfirmations
    if (pRequredConfIndex->GetBlockTime() > sigTime) {
        LogPrintf("CServiceNodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for ServiceNode %s %s\n",
            sigTime, Params().GetConsensus().nServiceNodeMinimumConfirmations, pRequredConfIndex->GetBlockTime(), outpoint.ToStringShort(), addr.ToString());
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CServiceNodeBroadcast::CheckOutpoint -- CheckSignature() failed, servicenode=%s\n", outpoint.ToStringShort());
        return false;
    }

    // remember the block hash when collateral for this servicenode had minimum required confirmations
    nCollateralMinConfBlockHash = pRequredConfIndex->GetBlockHash();

    return true;
}

uint256 CServiceNodeBroadcast::GetHash() const
{
    // Note: doesn't match serialization

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << outpoint << uint8_t{} << 0xffffffff; // adding dummy values here to match old hashing format
    ss << pubKeyCollateralAddress;
    ss << sigTime;
    return ss.GetHash();
}

uint256 CServiceNodeBroadcast::GetSignatureHash() const
{
    // TODO: replace with "return SerializeHash(*this);" after migration to 71000
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << outpoint;
    ss << addr;
    ss << pubKeyCollateralAddress;
    ss << pubKeyServiceNode;
    ss << sigTime;
    ss << nProtocolVersion;
    return ss.GetHash();
}

bool CServiceNodeBroadcast::Sign(const CKey& keyCollateralAddress)
{
    std::string strError;

    sigTime = GetAdjustedTime();

    if (sporkManager.IsSporkActive(SPORK_6_NEW_SIGS)) {
        uint256 hash = GetSignatureHash();

        if (!CHashSigner::SignHash(hash, keyCollateralAddress, vchSig)) {
            LogPrintf("CServiceNodeBroadcast::Sign -- SignHash() failed\n");
            return false;
        }

        if (!CHashSigner::VerifyHash(hash, pubKeyCollateralAddress, vchSig, strError)) {
            LogPrintf("CServiceNodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
            return false;
        }
    } else {
        std::string strMessage = addr.ToString(false) + std::to_string(sigTime) +
                                 pubKeyCollateralAddress.GetID().ToString() + pubKeyServiceNode.GetID().ToString() +
                                 std::to_string(nProtocolVersion);

        if (!CMessageSigner::SignMessage(strMessage, vchSig, keyCollateralAddress)) {
            LogPrintf("CServiceNodeBroadcast::Sign -- SignMessage() failed\n");
            return false;
        }

        if (!CMessageSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
            LogPrintf("CServiceNodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
            return false;
        }
    }

    return true;
}

bool CServiceNodeBroadcast::CheckSignature(int& nDos) const
{
    std::string strError = "";
    nDos = 0;

    if (sporkManager.IsSporkActive(SPORK_6_NEW_SIGS)) {
        uint256 hash = GetSignatureHash();

        if (!CHashSigner::VerifyHash(hash, pubKeyCollateralAddress, vchSig, strError)) {
            // maybe it's in old format
            std::string strMessage = addr.ToString(false) + std::to_string(sigTime) +
                                     pubKeyCollateralAddress.GetID().ToString() + pubKeyServiceNode.GetID().ToString() +
                                     std::to_string(nProtocolVersion);

            if (!CMessageSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
                // nope, not in old format either
                LogPrintf("CServiceNodeBroadcast::CheckSignature -- Got bad ServiceNode announce signature, error: %s\n", strError);
                nDos = 100;
                return false;
            }
        }
    } else {
        std::string strMessage = addr.ToString(false) + std::to_string(sigTime) +
                                 pubKeyCollateralAddress.GetID().ToString() + pubKeyServiceNode.GetID().ToString() +
                                 std::to_string(nProtocolVersion);

        if (!CMessageSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
            LogPrintf("CServiceNodeBroadcast::CheckSignature -- Got bad ServiceNode announce signature, error: %s\n", strError);
            nDos = 100;
            return false;
        }
    }

    return true;
}

void CServiceNodeBroadcast::Relay(CConnman& connman) const
{
    // Do not relay until fully synced
    if (!servicenodeSync.IsSynced()) {
        LogPrint("servicenode", "CServiceNodeBroadcast::Relay -- won't relay until fully synced\n");
        return;
    }

    CInv inv(MSG_SERVICENODE_ANNOUNCE, GetHash());
    connman.RelayInv(inv);
}

uint256 CServiceNodePing::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    if (sporkManager.IsSporkActive(SPORK_6_NEW_SIGS)) {
        // TODO: replace with "return SerializeHash(*this);" after migration to 71000
        ss << servicenodeOutpoint;
        ss << blockHash;
        ss << sigTime;
        ss << fSentinelIsCurrent;
        ss << nSentinelVersion;
        ss << nDaemonVersion;
    } else {
        // Note: doesn't match serialization

        ss << servicenodeOutpoint << uint8_t{} << 0xffffffff; // adding dummy values here to match old hashing format
        ss << sigTime;
    }
    return ss.GetHash();
}

uint256 CServiceNodePing::GetSignatureHash() const
{
    return GetHash();
}

CServiceNodePing::CServiceNodePing(const COutPoint& outpoint)
{
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12)
        return;

    servicenodeOutpoint = outpoint;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    nDaemonVersion = CLIENT_VERSION;
}

bool CServiceNodePing::Sign(const CKey& keyServiceNode, const CPubKey& pubKeyServiceNode)
{
    std::string strError;

    sigTime = GetAdjustedTime();

    if (sporkManager.IsSporkActive(SPORK_6_NEW_SIGS)) {
        uint256 hash = GetSignatureHash();

        if (!CHashSigner::SignHash(hash, keyServiceNode, vchSig)) {
            LogPrintf("CServiceNodePing::Sign -- SignHash() failed\n");
            return false;
        }

        if (!CHashSigner::VerifyHash(hash, pubKeyServiceNode, vchSig, strError)) {
            LogPrintf("CServiceNodePing::Sign -- VerifyHash() failed, error: %s\n", strError);
            return false;
        }
    } else {
        std::string strMessage = CTxIn(servicenodeOutpoint).ToString() + blockHash.ToString() +
                                 std::to_string(sigTime);

        if (!CMessageSigner::SignMessage(strMessage, vchSig, keyServiceNode)) {
            LogPrintf("CServiceNodePing::Sign -- SignMessage() failed\n");
            return false;
        }

        if (!CMessageSigner::VerifyMessage(pubKeyServiceNode, vchSig, strMessage, strError)) {
            LogPrintf("CServiceNodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
            return false;
        }
    }

    return true;
}

bool CServiceNodePing::CheckSignature(const CPubKey& pubKeyServiceNode, int& nDos) const
{
    std::string strError = "";
    nDos = 0;

    if (sporkManager.IsSporkActive(SPORK_6_NEW_SIGS)) {
        uint256 hash = GetSignatureHash();

        if (!CHashSigner::VerifyHash(hash, pubKeyServiceNode, vchSig, strError)) {
            std::string strMessage = CTxIn(servicenodeOutpoint).ToString() + blockHash.ToString() +
                                     std::to_string(sigTime);

            if (!CMessageSigner::VerifyMessage(pubKeyServiceNode, vchSig, strMessage, strError)) {
                LogPrintf("CServiceNodePing::CheckSignature -- Got bad ServiceNode ping signature, servicenode=%s, error: %s\n", servicenodeOutpoint.ToStringShort(), strError);
                nDos = 33;
                return false;
            }
        }
    } else {
        std::string strMessage = CTxIn(servicenodeOutpoint).ToString() + blockHash.ToString() +
                                 std::to_string(sigTime);

        if (!CMessageSigner::VerifyMessage(pubKeyServiceNode, vchSig, strMessage, strError)) {
            LogPrintf("CServiceNodePing::CheckSignature -- Got bad ServiceNode ping signature, servicenode=%s, error: %s\n", servicenodeOutpoint.ToStringShort(), strError);
            nDos = 33;
            return false;
        }
    }

    return true;
}

bool CServiceNodePing::SimpleCheck(int& nDos)
{
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CServiceNodePing::SimpleCheck -- Signature rejected, too far into the future, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
        nDos = 1;
        return false;
    }

    {
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            LogPrint("servicenode", "ServiceNodePing::SimpleCheck -- ServiceNode ping is invalid, unknown block hash: ServiceNode=%s blockHash=%s\n", servicenodeOutpoint.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("servicenode", "CServiceNodePing::SimpleCheck -- ServiceNode ping verified: ServiceNode=%s  blockHash=%s  sigTime=%d\n", servicenodeOutpoint.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CServiceNodePing::CheckAndUpdate(CServiceNode* psn, bool fFromNewBroadcast, int& nDos, CConnman& connman)
{
    AssertLockHeld(cs_main);

    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (psn == nullptr) {
        LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- Couldn't find ServiceNode entry, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (psn->IsUpdateRequired()) {
            LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode protocol is outdated, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
            return false;
        }

        if (psn->IsNewStartRequired()) {
            LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode is completely expired, new start is required, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
            return false;
        }
    }

    {
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode ping is invalid, block hash is too old: servicenode=%s  blockHash=%s\n", servicenodeOutpoint.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- New ping: ServiceNode=%s  blockHash=%s  sigTime=%d\n", servicenodeOutpoint.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("snping - Found corresponding sn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this ServiceNode or
    // last ping was more then SERVICENODE_MIN_SNP_SECONDS-60 ago comparing to this one
    if (psn->IsPingedWithin(SERVICENODE_MIN_SNP_SECONDS - 60, sigTime)) {
        LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode ping arrived too early, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(psn->pubKeyServiceNode, nDos))
        return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this sn for quite a while
    // (NOTE: assuming that SERVICENODE_EXPIRATION_SECONDS/2 should be enough to finish sn list sync)
    if (!servicenodeSync.IsServiceNodeListSynced() && !psn->IsPingedWithin(SERVICENODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- bumping sync timeout, servicenode=%s\n", servicenodeOutpoint.ToStringShort());
        servicenodeSync.BumpAssetLastTime("CServiceNodePing::CheckAndUpdate");
    }

    // let's store this ping as the last one
    LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode ping accepted, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
    psn->lastPing = *this;

    // and update snodeman.mapSeenServiceNodeBroadcast.lastPing which is probably outdated
    CServiceNodeBroadcast snb(*psn);
    uint256 hash = snb.GetHash();
    if (snodeman.mapSeenServiceNodeBroadcast.count(hash)) {
        snodeman.mapSeenServiceNodeBroadcast[hash].second.lastPing = *this;
    }

    // force update, ignoring cache
    psn->Check(true);
    // relay ping for nodes in ENABLED/EXPIRED/SENTINEL_PING_EXPIRED state only, skip everyone else
    if (!psn->IsEnabled() && !psn->IsExpired() && !psn->IsSentinelPingExpired())
        return false;

    LogPrint("servicenode", "CServiceNodePing::CheckAndUpdate -- ServiceNode ping acceepted and relayed, ServiceNode=%s\n", servicenodeOutpoint.ToStringShort());
    Relay(connman);

    return true;
}

void CServiceNodePing::Relay(CConnman& connman)
{
    // Do not relay until fully synced
    if (!servicenodeSync.IsSynced()) {
        LogPrint("servicenode", "CServiceNodePing::Relay -- won't relay until fully synced\n");
        return;
    }

    CInv inv(MSG_SERVICENODE_PING, GetHash());
    connman.RelayInv(inv);
}

std::string CServiceNodePing::GetSentinelString() const
{
    return nSentinelVersion > DEFAULT_SENTINEL_VERSION ? SafeIntVersionToString(nSentinelVersion) : "Unknown";
}
std::string CServiceNodePing::GetDaemonString() const
{
    return nDaemonVersion > DEFAULT_DAEMON_VERSION ? FormatVersion(nDaemonVersion) : "Unknown";
}

void CServiceNode::AddGovernanceVote(uint256 nGovernanceObjectHash)
{
    if (mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
    } else {
        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
    }
}

void CServiceNode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
{
    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
    if (it == mapGovernanceObjectsVotedOn.end()) {
        return;
    }
    mapGovernanceObjectsVotedOn.erase(it);
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When ServiceNode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
void CServiceNode::FlagGovernanceItemsAsDirty()
{
    std::vector<uint256> vecDirty;
    {
        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
        while (it != mapGovernanceObjectsVotedOn.end()) {
            vecDirty.push_back(it->first);
            ++it;
        }
    }
    for (size_t i = 0; i < vecDirty.size(); ++i) {
        snodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
    }
}
