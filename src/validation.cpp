// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.h"

#include "alert.h"
#include "arith_uint256.h"
#include "bdap/auditdb.h"
#include "bdap/certificatedb.h"
#include "bdap/domainentrydb.h"
#include "bdap/fees.h"
#include "bdap/linking.h"
#include "bdap/linkingdb.h"
#include "bdap/utils.h"
#include "blockencodings.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "fluid/banaccount.h"
#include "fluid/fluid.h"
#include "fluid/fluiddb.h"
#include "fluid/fluidmasternode.h"
#include "fluid/fluidmining.h"
#include "fluid/fluidmint.h"
#include "hash.h"
#include "init.h"
#include "instantsend.h"
#include "keystore.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "random.h"
#include "rpc/server.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "spork.h"
#include "timedata.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "versionbits.h"
#include "wallet/wallet.h"
#include "warnings.h"

#include <atomic>
#include <sstream>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/thread.hpp>

#if defined(NDEBUG)
#error "Cash cannot be compiled without assertions."
#endif

using namespace boost::placeholders;

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex* pindexBestHeader = NULL;
Mutex g_best_block_mutex;
std::condition_variable g_best_block_cv;
uint256 g_best_block;
std::map<unsigned int, unsigned int> mapHashedBlocks;
int nScriptCheckThreads = 0;
std::atomic_bool fImporting(false);
bool fReindex = false;
bool fTxIndex = true;
bool fAddressIndex = false;
bool fTimestampIndex = false;
bool fSpentIndex = false;
bool fHavePruned = false;
bool fPruneMode = false;
bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;
bool fRequireStandard = true;
unsigned int nBytesPerSigOp = DEFAULT_BYTES_PER_SIGOP;
bool fCheckBlockIndex = false;
bool fCheckpointsEnabled = DEFAULT_CHECKPOINTS_ENABLED;
size_t nCoinCacheUsage = 5000 * 300;
uint64_t nPruneTarget = 0;
bool fAlerts = DEFAULT_ALERTS;
int64_t nMaxTipAge = DEFAULT_MAX_TIP_AGE;
bool fEnableReplacement = DEFAULT_ENABLE_REPLACEMENT;
bool fLoaded = false;
bool fStealthTx = false;
int64_t nReserveBalance = 0;

uint256 hashAssumeValid;

CFeeRate minRelayTxFee = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;

CTxMemPool mempool(::minRelayTxFee);
std::map<uint256, int64_t> mapRejectedBlocks GUARDED_BY(cs_main);

static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams);
static void CheckBlockIndex(const Consensus::Params& consensusParams);

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const std::string strMessageMagic = "Cash Signed Message:\n";

// Internal stuff
namespace
{
struct CBlockIndexWorkComparator {
    bool operator()(CBlockIndex* pa, CBlockIndex* pb) const
    {
        // First sort by most total work, ...
        if (pa->nChainWork > pb->nChainWork)
            return false;
        if (pa->nChainWork < pb->nChainWork)
            return true;

        // ... then by earliest time received, ...
        if (pa->nSequenceId < pb->nSequenceId)
            return false;
        if (pa->nSequenceId > pb->nSequenceId)
            return true;

        // Use pointer address as tie breaker (should only happen with blocks
        // loaded from disk, as those all have id 0).
        if (pa < pb)
            return false;
        if (pa > pb)
            return true;

        // Identical blocks.
        return false;
    }
};

CBlockIndex* pindexBestInvalid;

/**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
std::set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
/** All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
std::multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

CCriticalSection cs_LastBlockFile;
std::vector<CBlockFileInfo> vinfoBlockFile;
int nLastBlockFile = 0;
/** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
bool fCheckForPruning = false;

/**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
CCriticalSection cs_nBlockSequenceId;
/** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
int32_t nBlockSequenceId = 1;
/** Decreasing counter (used by subsequent preciousblock calls). */
int32_t nBlockReverseSequenceId = -1;
/** chainwork for the last block that preciousblock has been applied to. */
arith_uint256 nLastPreciousChainwork = 0;

/** Dirty block index entries. */
std::set<CBlockIndex*> setDirtyBlockIndex;

/** Dirty block file entries. */
std::set<int> setDirtyFileInfo;
} // namespace

/* Use this class to start tracking transactions that are removed from the
 * mempool and pass all those transactions through SyncTransaction when the
 * object goes out of scope. This is currently only used to call SyncTransaction
 * on conflicts removed from the mempool during block connection.  Applied in
 * ActivateBestChain around ActivateBestStep which in turn calls:
 * ConnectTip->removeForBlock->removeConflicts
 */
class MemPoolConflictRemovalTracker
{
private:
    std::vector<CTransactionRef> conflictedTxs;
    CTxMemPool& pool;

public:
    MemPoolConflictRemovalTracker(CTxMemPool& _pool) : pool(_pool)
    {
        pool.NotifyEntryRemoved.connect(boost::bind(&MemPoolConflictRemovalTracker::NotifyEntryRemoved, this, _1, _2));
    }

    void NotifyEntryRemoved(CTransactionRef txRemoved, MemPoolRemovalReason reason)
    {
        if (reason == MemPoolRemovalReason::CONFLICT) {
            conflictedTxs.push_back(txRemoved);
        }
    }

    ~MemPoolConflictRemovalTracker()
    {
        pool.NotifyEntryRemoved.disconnect(boost::bind(&MemPoolConflictRemovalTracker::NotifyEntryRemoved, this, _1, _2));
        for (const auto& tx : conflictedTxs) {
            GetMainSignals().SyncTransaction(*tx, NULL, CMainSignals::SYNC_TRANSACTION_NOT_IN_BLOCK);
        }
        conflictedTxs.clear();
    }
};

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH (const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
        }
    }
    return chain.Genesis();
}

CCoinsViewDB* pcoinsdbview = NULL;
CCoinsViewCache* pcoinsTip = NULL;
CBlockTreeDB* pblocktree = NULL;

enum FlushStateMode {
    FLUSH_STATE_NONE,
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

// See definition for documentation
bool static FlushStateToDisk(CValidationState& state, FlushStateMode mode, int nManualPruneHeight = 0);
void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight);

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;

    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;

    if (nBlockHeight >= fluid.FLUID_ACTIVATE_HEIGHT) {
        if (!fluid.ProvisionalCheckTransaction(tx))
            return false;

        CScript scriptFluid;
        if (IsTransactionFluid(tx, scriptFluid)) {
            std::string strErrorMessage;
            if (!fluid.CheckFluidOperationScript(scriptFluid, nBlockTime, strErrorMessage)) {
                return false;
            }
        }
    }

    for (const auto& txin : tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

bool CheckFinalTx(const CTransaction& tx, int flags)
{
    AssertLockHeld(cs_main);

    // By convention a negative value for flags indicates that the
    // current network-enforced consensus rules should be used. In
    // a future soft-fork scenario that would mean checking which
    // rules would be enforced for the next block and setting the
    // appropriate flags. At the present time no soft-forks are
    // scheduled, so no flags are set.
    flags = std::max(flags, 0);

    // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chainActive.Height() + 1;

    // BIP113 will require that time-locked transactions have nLockTime set to
    // less than the median time of the previous block they're contained in.
    // When the next block is created its previous block will be the current
    // chain tip, so we use that to calculate the median time passed to
    // IsFinalTx() if LOCKTIME_MEDIAN_TIME_PAST is set.
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST) ? chainActive.Tip()->GetMedianTimePast() : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

/**
 * Calculates the block height and previous block's median time past at
 * which the transaction will be considered final in the context of BIP 68.
 * Also removes from the vector of input heights any entries which did not
 * correspond to sequence locked inputs as they do not affect the calculation.
 */
static std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction& tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2 && flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight - 1, 0))->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

static bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction& tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

bool TestLockPointValidity(const LockPoints* lp)
{
    AssertLockHeld(cs_main);
    assert(lp);
    // If there are relative lock times then the maxInputBlock will be set
    // If there are no relative lock times, the LockPoints don't depend on the chain
    if (lp->maxInputBlock) {
        // Check whether chainActive is an extension of the block at which the LockPoints
        // calculation was valid.  If not LockPoints are no longer valid
        if (!chainActive.Contains(lp->maxInputBlock)) {
            return false;
        }
    }

    // LockPoints still valid
    return true;
}

bool CheckSequenceLocks(const CTransaction& tx, int flags, LockPoints* lp, bool useExistingLockPoints)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(mempool.cs);

    CBlockIndex* tip = chainActive.Tip();
    CBlockIndex index;
    index.pprev = tip;
    // CheckSequenceLocks() uses chainActive.Height()+1 to evaluate
    // height based locks because when SequenceLocks() is called within
    // ConnectBlock(), the height of the block *being*
    // evaluated is what is used.
    // Thus if we want to know if a transaction can be part of the
    // *next* block, we need to use one more than chainActive.Height()
    index.nHeight = tip->nHeight + 1;

    std::pair<int, int64_t> lockPair;
    if (useExistingLockPoints) {
        assert(lp);
        lockPair.first = lp->height;
        lockPair.second = lp->time;
    } else {
        // pcoinsTip contains the UTXO set for chainActive.Tip()
        CCoinsViewMemPool viewMemPool(pcoinsTip, mempool);
        std::vector<int> prevheights;
        prevheights.resize(tx.vin.size());
        for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
            const CTxIn& txin = tx.vin[txinIndex];
            Coin coin;
            if (!viewMemPool.GetCoin(txin.prevout, coin)) {
                return error("%s: Missing input", __func__);
            }
            if (coin.nHeight == MEMPOOL_HEIGHT) {
                // Assume all mempool transaction confirm in the next block
                prevheights[txinIndex] = tip->nHeight + 1;
            } else {
                prevheights[txinIndex] = coin.nHeight;
            }
        }
        lockPair = CalculateSequenceLocks(tx, flags, &prevheights, index);
        if (lp) {
            lp->height = lockPair.first;
            lp->time = lockPair.second;
            // Also store the hash of the block with the highest height of
            // all the blocks which have sequence locked prevouts.
            // This hash needs to still be on the chain
            // for these LockPoint calculations to be valid
            // Note: It is impossible to correctly calculate a maxInputBlock
            // if any of the sequence locked inputs depend on unconfirmed txs,
            // except in the special case where the relative lock time/height
            // is 0, which is equivalent to no sequence lock. Since we assume
            // input height of tip+1 for mempool txs and test the resulting
            // lockPair from CalculateSequenceLocks against tip+1.  We know
            // EvaluateSequenceLocks will fail if there was a non-zero sequence
            // lock on a mempool input, so we can use the return value of
            // CheckSequenceLocks to indicate the LockPoints validity
            int maxInputHeight = 0;
            BOOST_FOREACH (int height, prevheights) {
                // Can ignore mempool inputs since we'll fail if they had non-zero locks
                if (height != tip->nHeight + 1) {
                    maxInputHeight = std::max(maxInputHeight, height);
                }
            }
            lp->maxInputBlock = tip->GetAncestor(maxInputHeight);
        }
    }
    return EvaluateSequenceLocks(index, lockPair);
}


unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut& prevout = coin.out;
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

bool GetUTXOCoin(const COutPoint& outpoint, Coin& coin)
{
    LOCK(cs_main);
    if (!pcoinsTip->GetCoin(outpoint, coin))
        return false;
    if (coin.IsSpent())
        return false;
    return true;
}

int GetUTXOHeight(const COutPoint& outpoint)
{
    // -1 means UTXO is yet unknown or already spent
    Coin coin;
    return GetUTXOCoin(outpoint, coin) ? coin.nHeight : -1;
}

int GetUTXOConfirmations(const COutPoint& outpoint)
{
    // -1 means UTXO is yet unknown or already spent
    LOCK(cs_main);
    int nPrevoutHeight = GetUTXOHeight(outpoint);
    return (nPrevoutHeight > -1 && chainActive.Tip()) ? chainActive.Height() - nPrevoutHeight + 1 : -1;
}

bool CheckTransaction(const CTransaction& tx, CValidationState& state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_TX_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for BDAP inputs or outputs so we can validate credit usage
    bool fIsBDAP = false;
    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    CAmount nStandardOut = 0;
    CAmount nCreditsOut = 0;
    CAmount nDataBurned = 0;
    for (const CTxOut& txout : tx.vout) {
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
        if (IsTransactionFluid(txout.scriptPubKey)) {
            if (fluid.FLUID_TRANSACTION_COST > txout.nValue)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-fluid-vout-amount-toolow");
            if (!fluid.ValidationProcesses(state, txout.scriptPubKey, txout.nValue))
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-fluid-validate-failure");
        }
        if (txout.IsBDAP()) {
            fIsBDAP = true;
            nCreditsOut += txout.nValue;
        } else if (txout.IsData()) {
            nDataBurned += txout.nValue;
        } else {
            nStandardOut += txout.nValue;
        }
    }

    // Check for duplicate inputs
    std::set<COutPoint> vInOutPoints;
    for (const auto& txin : tx.vin)
    {
        if (!vInOutPoints.insert(txin.prevout).second)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
    }
    CAmount nStandardIn = 0;
    CAmount nCreditsIn = 0;
    std::vector<Coin> vBdapCoins;
    if (tx.IsCoinBase()) {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    } else {
        for (const CTxIn& txin : tx.vin) {
            if (txin.prevout.IsNull())
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");

            CCoinsViewCache view(pcoinsTip);
            const Coin& coin = view.AccessCoin(txin.prevout);
            if (coin.out.IsBDAP()) {
                vBdapCoins.push_back(coin);
                fIsBDAP = true;
                nCreditsIn += coin.out.nValue;
            } else {
                nStandardIn += coin.out.nValue;
            }
        }
    }

    if (tx.IsCoinBase() && tx.nVersion == BDAP_TX_VERSION)
        return state.DoS(100, false, REJECT_INVALID, "bdap-tx-can-not-be-coinbase");

    // if we find a BDAP txin or txout, then make sure the transaction has the correct version
    if (fIsBDAP && tx.nVersion != BDAP_TX_VERSION)
        return state.DoS(100, false, REJECT_INVALID, "incorrect-bdap-tx-version");

    if (fIsBDAP && !CheckBDAPTxCreditUsage(tx, vBdapCoins, nStandardIn, nCreditsIn, nStandardOut, nCreditsOut, nDataBurned))
        return state.DoS(100, false, REJECT_INVALID, "bad-bdap-credit-use");

    return true;
}

bool CheckBDAPTxCreditUsage(const CTransaction& tx, const std::vector<Coin>& vBdapCoins,
                                const CAmount& nStandardIn, const CAmount& nCreditsIn, const CAmount& nStandardOut, const CAmount& nCreditsOut, const CAmount& nDataBurned)
{
    LogPrint("bdap", "%s -- nStandardIn %d, nCreditsIn %d, nStandardOut %d, nCreditsOut %d, nDataBurned %d\n", __func__,
                    FormatMoney(nStandardIn), FormatMoney(nCreditsIn), FormatMoney(nStandardOut), FormatMoney(nCreditsOut), FormatMoney(nDataBurned));
    // when there are no BDAP inputs, we do not need to check how credits are used.
    if (vBdapCoins.size() == 0 || nCreditsIn == 0)
        return true;

    if (nStandardIn > 0 && nStandardOut > 0 && nStandardOut >= nStandardIn) {
        LogPrintf("%s -- Check failed. Invalid use of BDAP credits. Standard 0DYNC output amounts exceeds or equals standard 0DYNC input amount\n", __func__);
        if (ENFORCE_BDAP_CREDIT_USE)
            return false;
    }

    if (nCreditsOut >= nCreditsIn) {
        LogPrintf("%s -- Check failed. Invalid use of BDAP credits. BDAP credits output amount exceeds BDAP credit input amount\n", __func__);
        if (ENFORCE_BDAP_CREDIT_USE)
            return false;
    }

    std::multimap<CDebitAddress, CServiceCredit> mapInputs;
    std::vector<std::pair<CServiceCredit, CDebitAddress>> vInputInfo;
    for (const Coin& coin : vBdapCoins) {
        int opCode1 = -1; int opCode2 = -1;
        std::vector<std::vector<unsigned char>> vvchOpParameters;
        coin.out.GetBDAPOpCodes(opCode1, opCode2, vvchOpParameters);
        CDebitAddress address = GetScriptAddress(coin.out.scriptPubKey);
        std::string strOpType = GetBDAPOpTypeString(opCode1, opCode2);
        CServiceCredit credit(strOpType, coin.out.nValue, vvchOpParameters);
        vInputInfo.push_back(std::make_pair(credit, address));
        mapInputs.insert({address, credit});
        LogPrint("bdap", "%s -- BDAP Input strOpType %s, opCode1 %d, opCode2 %d, nValue %d, address %s\n", __func__,
            strOpType, opCode1, opCode2, FormatMoney(coin.out.nValue), address.ToString());
    }

    std::multimap<CDebitAddress, CServiceCredit> mapOutputs;
    for (const CTxOut& txout : tx.vout) {
        if (txout.IsBDAP()) {
            int opCode1 = -1; int opCode2 = -1;
            std::vector<std::vector<unsigned char>> vvchOpParameters;
            txout.GetBDAPOpCodes(opCode1, opCode2, vvchOpParameters);
            CDebitAddress address = GetScriptAddress(txout.scriptPubKey);
            std::string strOpType = GetBDAPOpTypeString(opCode1, opCode2);
            CServiceCredit credit(strOpType, txout.nValue, vvchOpParameters);
            mapOutputs.insert({address, credit});
            LogPrint("bdap", "%s -- BDAP Output strOpType %s, opCode1 %d, opCode2 %d, nValue %d, address %s\n", __func__,
                strOpType, opCode1, opCode2, FormatMoney(txout.nValue), address.ToString());
        } else if (txout.IsData()) {
            CDebitAddress address;
            CServiceCredit credit("data", txout.nValue);
            mapOutputs.insert({address, credit});
            LogPrint("bdap", "%s -- BDAP Output strOpType %s, nValue %d\n", __func__, "data", FormatMoney(txout.nValue));
        } else {
            CDebitAddress address = GetScriptAddress(txout.scriptPubKey);
            CServiceCredit credit("standard", txout.nValue);
            mapOutputs.insert({address, credit});
        }
    }

    for (const std::pair<CServiceCredit, CDebitAddress>& credit : vInputInfo) {
        if (credit.first.OpType == "bdap_move_asset") {
            // When an input is a BDAP credit, make sure unconsumed coins go to a BDAP credit change ouput with the same credit input address and parameters
            if (credit.first.vParameters.size() == 2) {
                std::vector<unsigned char> vchMoveSource = credit.first.vParameters[0];
                std::vector<unsigned char> vchMoveDestination = credit.first.vParameters[1];
                if (vchMoveSource != vchFromString(std::string("0DYNC")) || vchMoveDestination != vchFromString(std::string("BDAP"))) {
                    LogPrintf("%s -- Check failed. Invalid use of BDAP credits. BDAP Credit has incorrect parameter. Move Source %s (should be 0DYNC), Move Destination %s (should be BDAP)\n", __func__,
                                            stringFromVch(vchMoveSource), stringFromVch(vchMoveDestination));
                    return false;
                }
            } else {
                LogPrintf("%s -- Check failed. Invalid use of BDAP credits. BDAP Credit has incorrect parameter count.\n", __func__);
                return false;
            }
            // make sure all of the credits are spent when we can't find an output address
            CDebitAddress inputAddress = credit.second;
            std::multimap<CDebitAddress, CServiceCredit>::iterator it = mapOutputs.find(inputAddress);
            if (it == mapOutputs.end()) {
                LogPrintf("%s -- Check failed. Invalid use of BDAP credits. Can't find credit address %s in outputs\n", __func__, inputAddress.ToString());
                if (ENFORCE_BDAP_CREDIT_USE)
                    return false;

            } else {
                // make sure asset doesn't move to another address, check outputs
                CAmount nInputAmount = 0;
                for (auto itr = mapInputs.find(inputAddress); itr != mapInputs.end(); itr++) {
                    nInputAmount += itr->second.nValue;
                }
                CAmount nOutputAmount = 0;
                for (auto itr = mapOutputs.find(inputAddress); itr != mapOutputs.end(); itr++) {
                    nOutputAmount += itr->second.nValue;
                }
                LogPrint("bdap", "%s -- inputAddress %s, nInputAmount %d, nOutputAmount %d, Diff %d\n", __func__,
                                inputAddress.ToString(), FormatMoney(nInputAmount), FormatMoney(nOutputAmount), FormatMoney((nInputAmount - nOutputAmount)));

                if (!((nInputAmount - nOutputAmount) == (nCreditsIn - nCreditsOut))) {
                    LogPrintf("%s -- Check failed. Invalid use of BDAP credits. Fuel used %d should equal total fuel used %d\n", __func__,
                                    FormatMoney((nInputAmount - nOutputAmount)), FormatMoney((nCreditsIn - nCreditsOut)));
                    if (ENFORCE_BDAP_CREDIT_USE)
                        return false;
                }
            }
        } else if (credit.first.OpType == "bdap_new_account" || credit.first.OpType == "bdap_update_account" ||
                        credit.first.OpType == "bdap_new_link_request" || credit.first.OpType == "bdap_new_link_accept" || credit.first.OpType == "bdap_new_audit" ||
                        credit.first.OpType == "bdap_new_certificate" || credit.first.OpType == "bdap_approve_certificate") {
            // When input is a BDAP account new or update operation, make sure deposit change goes back to input wallet address
            // When input is a BDAP link operation, make sure it is only spent by a link update or delete operations with the same input address and parameters
            CDebitAddress inputAddress = credit.second;
            std::multimap<CDebitAddress, CServiceCredit>::iterator it = mapOutputs.find(inputAddress);
            if (it == mapOutputs.end()) {
                LogPrintf("%s -- Check failed. Invalid use of BDAP credits. Can't find account address %s in outputs\n", __func__, inputAddress.ToString());
                if (ENFORCE_BDAP_CREDIT_USE)
                    return false;

            } else {
                // make sure asset doesn't move to another address, check outputs
                CAmount nInputAmount = 0;
                for (auto itr = mapInputs.find(inputAddress); itr != mapInputs.end(); itr++) {
                    nInputAmount += itr->second.nValue;
                }
                CAmount nOutputAmount = 0;
                for (auto itr = mapOutputs.find(inputAddress); itr != mapOutputs.end(); itr++) {
                    nOutputAmount += itr->second.nValue;
                }
                LogPrint("bdap", "%s --inputAddress %s, nInputAmount %d, nOutputAmount %d, Diff %d\n", __func__,
                                inputAddress.ToString(), FormatMoney(nInputAmount), FormatMoney(nOutputAmount), FormatMoney((nInputAmount - nOutputAmount)));

                if (!((nInputAmount - nOutputAmount) == (nCreditsIn - nCreditsOut))) {
                    LogPrintf("%s -- Check failed. Invalid use of BDAP credits. Fuel used %d should equal total fuel used %d\n", __func__,
                                    FormatMoney((nInputAmount - nOutputAmount)), FormatMoney((nCreditsIn - nCreditsOut)));
                    if (ENFORCE_BDAP_CREDIT_USE)
                        return false;
                }
            }
        }
    }
    return true;
}

void LimitMempoolSize(CTxMemPool& pool, size_t limit, unsigned long age)
{
    int expired = pool.Expire(GetTime() - age);
    if (expired != 0)
        LogPrint("mempool", "Expired %i transactions from the memory pool\n", expired);

    std::vector<COutPoint> vNoSpendsRemaining;
    pool.TrimToSize(limit, &vNoSpendsRemaining);
    BOOST_FOREACH (const COutPoint& removed, vNoSpendsRemaining)
        pcoinsTip->Uncache(removed);
}

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState& state)
{
    return strprintf("%s%s (code %i)",
        state.GetRejectReason(),
        state.GetDebugMessage().empty() ? "" : ", " + state.GetDebugMessage(),
        state.GetRejectCode());
}

static bool IsCurrentForFeeEstimation()
{
    AssertLockHeld(cs_main);
    if (IsInitialBlockDownload())
        return false;
    if (chainActive.Tip()->GetBlockTime() < (GetTime() - MAX_FEE_ESTIMATION_TIP_AGE))
        return false;
    if (chainActive.Height() < pindexBestHeader->nHeight - 1)
        return false;
    return true;
}

// Check if BDAP entry is valid
bool ValidateBDAPInputs(const CTransactionRef& tx, CValidationState& state, const CCoinsViewCache& inputs, const CBlock& block, bool fJustCheck, int nHeight, bool bSanity)
{
    if (!CheckDomainEntryDB())
        return true;

    std::string statusRpc = "";
    if (fJustCheck && (IsInitialBlockDownload() || RPCIsInWarmup(&statusRpc)))
        return true;

    std::vector<std::vector<unsigned char> > vvchBDAPArgs;
    int op1 = -1;
    int op2 = -1;
    if (nHeight == 0) {
        nHeight = chainActive.Height() + 1;
    }
    bool bValid = false;
    if (tx->nVersion == BDAP_TX_VERSION) {
        CScript scriptOp;
        if (GetBDAPOpScript(tx, scriptOp, vvchBDAPArgs, op1, op2)) {
            std::string errorMessage;
            if (vvchBDAPArgs.size() > 6) {
                errorMessage = "Too many BDAP parameters in operation transactions.";
                return state.DoS(100, false, REJECT_INVALID, errorMessage);
            }
            if (vvchBDAPArgs.size() < 1) {
                errorMessage = "Not enough BDAP parameters in operation transactions.";
                return state.DoS(100, false, REJECT_INVALID, errorMessage);
            }

            std::string strOpType = GetBDAPOpTypeString(op1, op2);
            if (strOpType == "bdap_new_account" || strOpType == "bdap_update_account" || strOpType == "bdap_delete_account") {
                bValid = CheckDomainEntryTx(tx, scriptOp, op1, op2, vvchBDAPArgs, fJustCheck, nHeight, block.nTime, bSanity, errorMessage);
                if (!bValid) {
                    errorMessage = "ValidateBDAPInputs: " + errorMessage;
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                if (!errorMessage.empty())
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
            }
            else if (strOpType == "bdap_new_link_request") {
                std::vector<unsigned char> vchPubKey = vvchBDAPArgs[0];
                LogPrint("bdap", "%s -- New Link Request vchPubKey = %s\n", __func__, stringFromVch(vchPubKey));
                bValid = CheckLinkTx(tx, op1, op2, vvchBDAPArgs, fJustCheck, nHeight, block.nTime, bSanity, errorMessage);
                if (!bValid) {
                    errorMessage = "ValidateBDAPInputs: CheckLinkTx failed: " + errorMessage;
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                uint256 txid;
                if (GetLinkIndex(vchPubKey, txid)) {
                    if (txid != tx->GetHash()) {
                        errorMessage = "Link request public key already used.";
                        LogPrintf("%s -- %s\n", __func__, errorMessage);
                        return state.DoS(100, false, REJECT_INVALID, errorMessage);
                    }
                }
                return true;
            }
            else if (strOpType == "bdap_new_link_accept") {
                std::vector<unsigned char> vchPubKey = vvchBDAPArgs[0];
                LogPrint("bdap", "%s -- New Link Accept vchPubKey = %s\n", __func__, stringFromVch(vchPubKey));
                bValid = CheckLinkTx(tx, op1, op2, vvchBDAPArgs, fJustCheck, nHeight, block.nTime, bSanity, errorMessage);
                if (!bValid) {
                    errorMessage = "ValidateBDAPInputs: CheckLinkTx failed: " + errorMessage;
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                uint256 txid;
                if (GetLinkIndex(vchPubKey, txid)) {
                    if (txid != tx->GetHash()) {
                        errorMessage = "Link accept public key already used.";
                        return state.DoS(100, false, REJECT_INVALID, errorMessage);
                    }
                }
                return true;
            }
            else if (strOpType == "bdap_move_asset") {
                if (!(vvchBDAPArgs.size() == 2)) {
                    errorMessage = "Incorrect number of parameters used for " + strOpType + " transaction.";
                    LogPrintf("%s -- delete link ignored. %s\n", __func__, errorMessage);
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                LogPrint("bdap", "%s -- BDAP move asset operation. vvchBDAPArgs.size() = %d\n", __func__, vvchBDAPArgs.size());
                return true;
            }
            else if (strOpType == "bdap_new_audit") {
                bValid = CheckAuditTx(tx, scriptOp, op1, op2, vvchBDAPArgs, fJustCheck, nHeight, block.nTime, bSanity, errorMessage);
                if (!bValid) {
                    errorMessage = "ValidateBDAPInputs: " + errorMessage;
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                if (!errorMessage.empty())
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                LogPrint("bdap", "%s -- CheckAuditTx valid.\n", __func__);
                return true;
            }
            else if (strOpType == "bdap_new_certificate" || strOpType == "bdap_approve_certificate") {
                bValid = CheckCertificateTx(tx, scriptOp, op1, op2, vvchBDAPArgs, fJustCheck, nHeight, block.nTime, bSanity, errorMessage);
                if (!bValid) {
                    errorMessage = "ValidateBDAPInputs: " + errorMessage;
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                if (!errorMessage.empty())
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                LogPrint("bdap", "%s -- CheckCertificateTx valid.\n", __func__);
                return true;
            }
            else if (strOpType == "bdap_delete_link_request" || strOpType == "bdap_delete_link_accept") {
                /*
                if (!CheckPreviousLinkInputs(strOpType, scriptOp, vvchBDAPArgs, errorMessage, fJustCheck)) {
                    errorMessage = "ValidateBDAPInputs: Delete link failed" + errorMessage;
                    LogPrintf("%s -- delete link failed. %s\n", __func__, errorMessage);
                    return state.DoS(100, false, REJECT_INVALID, errorMessage);
                }
                */
                // TODO (BDAP): Implement link delete
                errorMessage = "ValidateBDAPInputs: Failed because " + strOpType + " is not implemented yet." + errorMessage;
                LogPrintf("%s -- delete link ignored. %s\n", __func__, errorMessage);
            }
            else if (strOpType == "bdap_update_link_request" || strOpType == "bdap_update_link_accept") {
                // TODO (BDAP): Implement link update, allow for now.
                errorMessage = "ValidateBDAPInputs: Failed because " + strOpType + " is not implemented yet." + errorMessage;
                LogPrintf("%s -- update link ignored. %s\n", __func__, errorMessage);
            }
            else {
                // Do not allow unknown BDAP operations
                errorMessage = strprintf("%s -- Failed, unknown operation found. opcode1 = %d, opcode2 = %d", __func__, op1, op2);
                LogPrintf("%s\n", errorMessage);
                return state.DoS(100, false, REJECT_INVALID, errorMessage);
            }
        }
    }
    return true;
}

bool AcceptToMemoryPoolWorker(CTxMemPool& pool, CValidationState& state, const CTransactionRef& ptx, bool fLimitFree, bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced, bool fOverrideMempoolLimit, const CAmount& nAbsurdFee, std::vector<COutPoint>& coins_to_uncache, bool fDryRun)
{
    const CTransaction& tx = *ptx;
    const uint256 hash = tx.GetHash();
    bool fluidTransaction = false;
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction(tx, state))
        return false; // state filled in by CheckTransaction

    if (!fluid.ProvisionalCheckTransaction(tx))
        return false;

    for (const CTxOut& txout : tx.vout) {
        if (IsTransactionFluid(txout.scriptPubKey)) {
            fluidTransaction = true;
            std::string strErrorMessage;
            // Check if fluid transaction is already in the mempool
            if (fluid.CheckIfExistsInMemPool(pool, txout.scriptPubKey, strErrorMessage)) {
                // fluid transaction is already in the mempool.  Reject tx.
                return state.DoS(100, false, REJECT_INVALID, strErrorMessage);
            }
            std::string strFluidOpScript = ScriptToAsmStr(txout.scriptPubKey);
            std::string verificationWithoutOpCode = GetRidOfScriptStatement(strFluidOpScript);
            std::string strOperationCode = GetRidOfScriptStatement(strFluidOpScript, 0);
            if (strOperationCode == "OP_BDAP_REVOKE" && !sporkManager.IsSporkActive(SPORK_30_ACTIVATE_BDAP))
                return state.Invalid(false, REJECT_INVALID, "bdap-spork-inactive");

            if (!fluid.ExtractCheckTimestamp(strOperationCode, ScriptToAsmStr(txout.scriptPubKey), GetTime())) {
                return state.DoS(100, false, REJECT_INVALID, "fluid-tx-timestamp-error");
            }
            if (!fluid.CheckFluidOperationScript(txout.scriptPubKey, GetTime(), strErrorMessage, true)) {
                return state.DoS(100, false, REJECT_INVALID, strErrorMessage);
            }
        }
    }
    // Don't relay BDAP transaction until spork is activated
    if (tx.nVersion == BDAP_TX_VERSION && !sporkManager.IsSporkActive(SPORK_30_ACTIVATE_BDAP))
        return state.DoS(0, false, REJECT_NONSTANDARD, "inactive-spork-bdap-tx");

    bool fIsBDAP = false;
    //TODO: Create a seperate function to check BDAP tx validity.
    if (tx.nVersion == BDAP_TX_VERSION) {
        fIsBDAP = true;
        CScript scriptBDAPOp;
        std::vector<std::vector<unsigned char>> vvch;
        CScript scriptOp;
        int op1, op2;
        if (!GetBDAPOpScript(ptx, scriptBDAPOp, vvch, op1, op2))
            return state.Invalid(false, REJECT_INVALID, "bdap-txn-script-error");

        std::string strErrorMessage;
        vchCharString vvchOpParameters;
        if (!GetBDAPOpScript(ptx, scriptOp, vvchOpParameters, op1, op2)) {
            return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-op-failed" + strErrorMessage);
        }
        const std::string strOpType = GetBDAPOpTypeString(op1, op2);
        if (strOpType == "bdap_new_account" || strOpType == "bdap_update_account" || strOpType == "bdap_delete_account") {
            CDomainEntry domainEntry(ptx);
            if (domainEntry.CheckIfExistsInMemPool(pool, strErrorMessage)) {
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-account-txn-already-in-mempool " + strErrorMessage);
            }
            if (strOpType == "bdap_new_account") {
                CDomainEntry findDomainEntry;
                if (GetDomainEntry(domainEntry.vchFullObjectPath(), findDomainEntry))
                {
                    strErrorMessage = "AcceptToMemoryPoolWorker -- The entry " + findDomainEntry.GetFullObjectPath() + " already exists.  Rejected by the tx memory pool!";
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-exists " + strErrorMessage);
                }
            } else if (strOpType == "bdap_update_account") {
                CDomainEntry entry;
                CDomainEntry prevEntry;
                std::vector<unsigned char> vchData;
                std::vector<unsigned char> vchHash;
                int nDataOut;
                bool bData = GetBDAPData(ptx, vchData, vchHash, nDataOut);
                if (bData && !entry.UnserializeFromData(vchData, vchHash)) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-data-failed" + strErrorMessage);
                }

                if (!pDomainEntryDB->GetDomainEntryInfo(entry.vchFullObjectPath(), prevEntry)) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-previous-failed" + strErrorMessage);
                }
                CTransactionRef pPrevTx;
                uint256 hashBlock;
                if (!GetTransaction(prevEntry.txHash, pPrevTx, Params().GetConsensus(), hashBlock, true)) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-previous-tx-failed" + strErrorMessage);
                }
                // Get current wallet address used for BDAP tx
                CScript scriptPubKey = scriptBDAPOp;
                CDebitAddress txAddress = GetScriptAddress(scriptPubKey);
                // Get previous wallet address used for BDAP tx
                CScript prevScriptPubKey;
                GetBDAPOpScript(pPrevTx, prevScriptPubKey);
                CDebitAddress prevAddress = GetScriptAddress(prevScriptPubKey);
                if (txAddress.ToString() != prevAddress.ToString()) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-incorrect-wallet-address-used" + strErrorMessage);
                }
            } else if (strOpType == "bdap_delete_account") {
                if (!(vvchOpParameters.size() > 0))
                    return state.Invalid(false, REJECT_INVALID, "bdap-delete-account-get-object-path" + strErrorMessage);

                std::vector<unsigned char> vchFullObjectPath = vvchOpParameters[0];
                CDomainEntry prevEntry;
                if (!pDomainEntryDB->GetDomainEntryInfo(vchFullObjectPath, prevEntry)) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-previous-failed" + strErrorMessage);
                }
                CTransactionRef pPrevTx;
                uint256 hashBlock;
                if (!GetTransaction(prevEntry.txHash, pPrevTx, Params().GetConsensus(), hashBlock, true)) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-get-previous-tx-failed" + strErrorMessage);
                }
                // Get current wallet address used for BDAP tx
                CScript scriptPubKey = scriptBDAPOp;
                CDebitAddress txAddress = GetScriptAddress(scriptPubKey);
                // Get previous wallet address used for BDAP tx
                CScript prevScriptPubKey;
                GetBDAPOpScript(pPrevTx, prevScriptPubKey);
                CDebitAddress prevAddress = GetScriptAddress(prevScriptPubKey);
                if (txAddress.ToString() != prevAddress.ToString()) {
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-txn-incorrect-wallet-address-used" + strErrorMessage);
                }
            }
        } else if (strOpType == "bdap_new_link_request" || strOpType == "bdap_new_link_accept") {
            if (vvch.size() < 1)
                return state.Invalid(false, REJECT_INVALID, "bdap-txn-pubkey-parameter-not-found");
            if (vvch.size() > 3)
                return state.Invalid(false, REJECT_INVALID, "bdap-txn-too-many-parameters");
            //check for duplicate pubkeys
            std::vector<unsigned char> vchPubKey = vvch[0];
            if (LinkPubKeyExistsInMemPool(pool, vchPubKey, strOpType, strErrorMessage))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-pubkey-txn-already-in-mempool");

            if (LinkPubKeyExists(vchPubKey))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-duplicate-pubkey");

            CDomainEntry prevEntry;
            if (GetDomainEntryPubKey(vchPubKey, prevEntry))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-duplicate-pubkey-entry");

            if (vvch.size() > 1) {
                std::vector<unsigned char> vchSharedPubKey = vvch[1];
                if (LinkPubKeyExists(vchSharedPubKey))
                    return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-duplicate-shared-pubkey");

                if (GetDomainEntryPubKey(vchSharedPubKey, prevEntry))
                    return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-duplicate-shared-pubkey-entry");
            }
        } else if (strOpType == "bdap_move_asset") {
            if (vvch.size() != 2)
                return state.Invalid(false, REJECT_INVALID, "bdap-move-invalid-parameter-size");
            std::vector<unsigned char> vchMoveSource = vchFromString(std::string("0DYNC"));
            std::vector<unsigned char> vchMoveDestination = vchFromString(std::string("BDAP"));
            if (vvch[0] != vchMoveSource)
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-move-unknown-source");
            if (vvch[1] != vchMoveDestination)
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-move-unknown-destination");

        } else if (strOpType == "bdap_new_audit") {
            if (!sporkManager.IsSporkActive(SPORK_32_BDAP_V2))
                return state.DoS(0, false, REJECT_NONSTANDARD, "inactive-spork-bdap-v2-tx");

            if (vvch.size() < 1)
                return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-not-enough-parameters");

            if (vvch.size() > 3)
                return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-too-many-parameters");

            if (vvch[0].size() > 10)
                return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-parameter-too-long");

            if (vvch.size() > 1) {
                if (vvch.size() == 2)
                   return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-pubkey-missing");

                if (vvch[1].size() > MAX_OBJECT_FULL_PATH_LENGTH)
                    return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-fqdn-too-long");

                if (vvch[2].size() > 65)
                    return state.Invalid(false, REJECT_INVALID, "bdap-new-audit-pubkey-too-long");

                // check pubkey belongs to bdap account and signature is correct.
                CAudit audit(ptx);
                std::string errorMessage;
                if (!audit.ValidateValues(errorMessage))
                    return state.Invalid(false, REJECT_INVALID, "bdap-new-audit: " + errorMessage);

                CDomainEntry findDomainEntry;
                if (!GetDomainEntry(audit.vchOwnerFullObjectPath, findDomainEntry)) {
                    strErrorMessage = "AcceptToMemoryPoolWorker -- The entry " + stringFromVch(audit.vchOwnerFullObjectPath) + " not found.  Rejected by the tx memory pool!";
                    return state.Invalid(false, REJECT_INVALID, "bdap-account-exists " + strErrorMessage);
                }
                CPubKey pubkey(vvch[2]);
                CDebitAddress address(pubkey.GetID());
                if (findDomainEntry.GetWalletAddress().ToString() != address.ToString()) {
                        strErrorMessage = "AcceptToMemoryPoolWorker -- Public key does not match BDAP account wallet address.  Rejected by the tx memory pool!";
                        return state.Invalid(false, REJECT_INVALID, "bdap-audit-wallet-address-mismatch " + strErrorMessage);
                    }
                if (!audit.CheckSignature(pubkey.Raw())) {
                    strErrorMessage = "AcceptToMemoryPoolWorker -- Invalid signature.  Rejected by the tx memory pool!";
                    return state.Invalid(false, REJECT_INVALID, "bdap-audit-check-signature-failed " + strErrorMessage);
                }
            }
            CAudit audit;
            if (GetAuditTxId(tx.GetHash().GetHex(), audit))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-audit-already-exists");

        } else if (strOpType == "bdap_new_certificate" || strOpType == "bdap_approve_certificate") {
            if (!sporkManager.IsSporkActive(SPORK_32_BDAP_V2))
                return state.DoS(0, false, REJECT_NONSTANDARD, "inactive-spork-bdap-v2-tx");

            std::string errorPrefix = "bdap-new-certificate-";

            if (strOpType == "bdap_approve_certificate") {
                errorPrefix = "bdap-approve-certificate-";
            }

            CX509Certificate certificate(ptx);

            if (certificate.CheckIfExistsInMemPool(mempool, strErrorMessage)) {
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "already-exists-in-mempool");
            }

            if (vvch.size() < 5)
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "not-enough-parameters");

            if (vvch.size() > 6)
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "too-many-parameters");

            if (vvch.size() > 2 && vvch[2].size() > MAX_OBJECT_FULL_PATH_LENGTH)
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "subject-fqdn-too-long");

            if (vvch.size() > 4 && vvch[4].size() > MAX_OBJECT_FULL_PATH_LENGTH)
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "issuer-fqdn-too-long");

            if (vvch.size() > 3 && vvch[3].size() > MAX_KEY_LENGTH)
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "subject-pubkey-too-long");

            //If Approved check Issuer Pubkey length
            if (strOpType == "bdap_approve_certificate") {
                if (vvch.size() > 5 && vvch[5].size() > MAX_KEY_LENGTH)
                    return state.Invalid(false, REJECT_INVALID, errorPrefix + "issuer-pubkey-too-long");
            }

            // check bdap accounts and signature is correct.
            CDomainEntry findSubjectDomainEntry;
            CDomainEntry findIssuerDomainEntry;
            std::string errorMessage;

            uint32_t nMonthsValid;
            ParseUInt32(stringFromVch(vvch[1]), &nMonthsValid);

            if (certificate.IsNull()) {
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "certificate-is-empty");
            }

            //update months valid check to handle root certificates
            if (certificate.IsRootCA) {
                if (!(nMonthsValid > 0 && nMonthsValid <= MAX_CERTIFICATE_CA_MONTHS_VALID))
                    return state.Invalid(false, REJECT_INVALID, errorPrefix + "rootca-months-valid-incorrect");
            }
            else {
                if (!(nMonthsValid > 0 && nMonthsValid <= MAX_CERTIFICATE_MONTHS_VALID))
                    return state.Invalid(false, REJECT_INVALID, errorPrefix + "months-valid-incorrect");
            }

            if (!certificate.ValidatePEM(errorMessage))
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "certificate-error: " + errorMessage);

            if (!certificate.ValidateValues(errorMessage))
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "certificate-error: " + errorMessage);

            //Check Subject BDAP
            if (!GetDomainEntry(certificate.Subject, findSubjectDomainEntry)) {
                strErrorMessage = "AcceptToMemoryPoolWorker -- The entry " + stringFromVch(certificate.Subject) + " not found.  Rejected by the tx memory pool!";
                return state.Invalid(false, REJECT_INVALID, errorPrefix + "subject-account-exists " + strErrorMessage);
            }

            CharString vchSubjectPubKey = findSubjectDomainEntry.DHTPublicKey;
            CharString vchIssuerPubKey;

            //If not self signed, check issuer BDAP
            if (!certificate.SelfSignedX509Certificate()) {
                if (!GetDomainEntry(certificate.Issuer, findIssuerDomainEntry)) {
                    strErrorMessage = "AcceptToMemoryPoolWorker -- The entry " + stringFromVch(certificate.Issuer) + " not found.  Rejected by the tx memory pool!";
                    return state.Invalid(false, REJECT_INVALID, errorPrefix + "issuer-account-exists " + strErrorMessage);
                }
                vchIssuerPubKey = findIssuerDomainEntry.DHTPublicKey;

                //only check subject signature if NOT self signed and NOT approved because PEM gets modified [so check for REQUEST only]
                //Check Subject Signature
                if (!certificate.IsApproved()) {
                    if (!certificate.CheckSubjectSignature(EncodedPubKeyToBytes(vchSubjectPubKey))) {
                        strErrorMessage = "AcceptToMemoryPoolWorker -- Invalid signature.  Rejected by the tx memory pool!";
                        return state.Invalid(false, REJECT_INVALID, errorPrefix + "check-subject-signature-failed " + strErrorMessage);
                    }
                }
            }
            else {
                vchIssuerPubKey = vchSubjectPubKey;
            }

            //If Approve check Issuer Signature and if not self signed check request exists
            if (strOpType == "bdap_approve_certificate") {

                //check issuer signature
                if (!certificate.CheckIssuerSignature(EncodedPubKeyToBytes(vchIssuerPubKey))) {
                    strErrorMessage = "AcceptToMemoryPoolWorker -- Invalid signature.  Rejected by the tx memory pool!";
                    return state.Invalid(false, REJECT_INVALID, errorPrefix + "check-issuer-signature-failed " + strErrorMessage);
                }

                //if not self-signed, check that request exists
                if (!certificate.SelfSignedX509Certificate()) {
                    CX509Certificate certificateRequest;
                    if (!GetCertificateTxId(certificate.txHashRequest.ToString(), certificateRequest))
                        return state.Invalid(false, REJECT_INVALID, errorPrefix + "reqeust-not-found");
                }
            }

            CX509Certificate certificateCheck;
            if (GetCertificateTxId(tx.GetHash().GetHex(), certificateCheck))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, errorPrefix + "already-exists");

            //check if a certificate with given serial number already exists
            if (certificate.SerialNumber > 0) {
                CX509Certificate certificateSerialCheck;
                if (GetCertificateSerialNumber(std::to_string(certificate.SerialNumber), certificateSerialCheck))
                    return state.Invalid(false, REJECT_ALREADY_KNOWN, errorPrefix + "serialnumber-already-exists");

            }

        }
        // TODO (BDAP): Implement link delete
        /*
        else if (strOpType == "bdap_delete_link_request" || strOpType == "bdap_delete_link_accept") {
            if (vvch.size() < 1)
                return state.Invalid(false, REJECT_INVALID, "bdap-txn-pubkey-parameter-not-found");
            if (vvch.size() > 2)
                return state.Invalid(false, REJECT_INVALID, "bdap-txn-too-many-parameters");

            std::vector<unsigned char> vchPubKey = vvch[0];
            if (LinkPubKeyExistsInMemPool(pool, vchPubKey, strOpType, strErrorMessage))
                return state.Invalid(false, REJECT_ALREADY_KNOWN, "bdap-link-pubkey-txn-already-in-mempool");
        }
        */
        else {
            // Do not allow unknown BDAP operations
            LogPrintf("%s -- Failed, unknown operation found. opcode1 = %d, opcode2 = %d\n", __func__, op1, op2);
            return state.DoS(100, false, REJECT_INVALID, "bdap-unknown-operation");
        }
    }
    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "coinbase");

    // Don't relay version 2 transactions until CSV is active, and we can be
    // sure that such transactions will be mined (unless we're on
    // -testnet/-regtest).
    const CChainParams& chainparams = Params();
    if (fRequireStandard && tx.nVersion  > CTransaction::MAX_STANDARD_VERSION && tx.nVersion != BDAP_TX_VERSION && VersionBitsTipState(chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV) != THRESHOLD_ACTIVE) {
        return state.DoS(0, false, REJECT_NONSTANDARD, "premature-version-tx");
    }

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    std::string reason;
    if (fRequireStandard && !fIsBDAP && !IsStandardTx(tx, reason) && !fluidTransaction)
        return state.DoS(0, false, REJECT_NONSTANDARD, reason);

    // Only accept nLockTime-using transactions that can be mined in the next
    // block; we don't want our mempool filled up with transactions that can't
    // be mined yet.
    if (!CheckFinalTx(tx, STANDARD_LOCKTIME_VERIFY_FLAGS))
        return state.DoS(0, false, REJECT_NONSTANDARD, "non-final");

    // is it already in the memory pool?
    if (pool.exists(hash))
        return state.Invalid(false, REJECT_ALREADY_KNOWN, "txn-already-in-mempool");

    // If this is a Transaction Lock Request check to see if it's valid
    if (instantsend.HasTxLockRequest(hash) && !CTxLockRequest(tx).IsValid())
        return state.DoS(10, error("AcceptToMemoryPool : CTxLockRequest %s is invalid", hash.ToString()),
            REJECT_INVALID, "bad-txlockrequest");

    // Check for conflicts with a completed Transaction Lock
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        uint256 hashLocked;
        if (instantsend.GetLockedOutPointTxHash(txin.prevout, hashLocked) && hash != hashLocked)
            return state.DoS(10, error("AcceptToMemoryPool : Transaction %s conflicts with completed Transaction Lock %s", hash.ToString(), hashLocked.ToString()),
                REJECT_INVALID, "tx-txlock-conflict");
    }

    // Check for conflicts with in-memory transactions
    std::set<uint256> setConflicts;
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            auto itConflicting = pool.mapNextTx.find(txin.prevout);
            if (itConflicting != pool.mapNextTx.end()) {
                const CTransaction* ptxConflicting = itConflicting->second;
                if (!setConflicts.count(ptxConflicting->GetHash())) {
                    // InstantSend txes are not replacable
                    if (instantsend.HasTxLockRequest(ptxConflicting->GetHash())) {
                        // this tx conflicts with a Transaction Lock Request candidate
                        return state.DoS(0, error("AcceptToMemoryPool : Transaction %s conflicts with Transaction Lock Request %s", hash.ToString(), ptxConflicting->GetHash().ToString()),
                            REJECT_INVALID, "tx-txlockreq-mempool-conflict");
                    } else if (instantsend.HasTxLockRequest(hash)) {
                        // this tx is a tx lock request and it conflicts with a normal tx
                        return state.DoS(0, error("AcceptToMemoryPool : Transaction Lock Request %s conflicts with transaction %s", hash.ToString(), ptxConflicting->GetHash().ToString()),
                            REJECT_INVALID, "txlockreq-tx-mempool-conflict");
                    }
                    // Allow opt-out of transaction replacement by setting
                    // nSequence >= maxint-1 on all inputs.
                    //
                    // maxint-1 is picked to still allow use of nLockTime by
                    // non-replacable transactions. All inputs rather than just one
                    // is for the sake of multi-party protocols, where we don't
                    // want a single party to be able to disable replacement.
                    //
                    // The opt-out ignores descendants as anyone relying on
                    // first-seen mempool behavior should be checking all
                    // unconfirmed ancestors anyway; doing otherwise is hopelessly
                    // insecure.
                    bool fReplacementOptOut = true;
                    if (fEnableReplacement) {
                        BOOST_FOREACH (const CTxIn& _txin, ptxConflicting->vin) {
                            if (_txin.nSequence < std::numeric_limits<unsigned int>::max() - 1) {
                                fReplacementOptOut = false;
                                break;
                            }
                        }
                    }
                    if (fReplacementOptOut)
                        return state.Invalid(false, REJECT_CONFLICT, "txn-mempool-conflict");

                    setConflicts.insert(ptxConflicting->GetHash());
                }
            }
        }
    }

    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        LockPoints lp;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            for (size_t out = 0; out < tx.vout.size(); out++) {
                COutPoint outpoint(hash, out);
                bool had_coin_in_cache = pcoinsTip->HaveCoinInCache(outpoint);
                if (view.HaveCoin(outpoint)) {
                    if (!had_coin_in_cache) {
                        coins_to_uncache.push_back(outpoint);
                    }
                    return state.Invalid(false, REJECT_ALREADY_KNOWN, "txn-already-known");
                }
            }

            // do all inputs exist?
            BOOST_FOREACH (const CTxIn txin, tx.vin) {
                if (!pcoinsTip->HaveCoinInCache(txin.prevout)) {
                    coins_to_uncache.push_back(txin.prevout);
                }
                if (!view.HaveCoin(txin.prevout)) {
                    if (pfMissingInputs) {
                        *pfMissingInputs = true;
                    }
                    return false; // fMissingInputs and !state.IsInvalid() is used to detect this condition, don't set state.Invalid()
                }
            }

            // Bring the best block into scope
            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);

            // Only accept BIP68 sequence locked transactions that can be mined in the next
            // block; we don't want our mempool filled up with transactions that can't
            // be mined yet.
            // Must keep pool.cs for this unless we change CheckSequenceLocks to take a
            // CoinsViewCache instead of create its own
            if (!CheckSequenceLocks(tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp))
                return state.DoS(0, false, REJECT_NONSTANDARD, "non-BIP68-final");
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (fRequireStandard && !AreInputsStandard(tx, view))
            return state.Invalid(false, REJECT_NONSTANDARD, "bad-txns-nonstandard-inputs");

        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;
        CAmount nBDAPBurn = 0;
        if (tx.nVersion == BDAP_TX_VERSION) {
            // Since fees are burned, count BDAP burn funds into fee calculation
            CAmount nOpCodeAmount;
            ExtractAmountsFromTx(MakeTransactionRef(tx), nBDAPBurn, nOpCodeAmount);
            if (nBDAPBurn > 0)
                nFees += nBDAPBurn;

            LogPrint("bdap", "%s -- BDAP Burn Data Amount %d, BDAP Op Code Amount %d\n", __func__, FormatMoney(nBDAPBurn), FormatMoney(nOpCodeAmount));
        }
        // nModifiedFees includes any fee deltas from PrioritiseTransaction
        CAmount nModifiedFees = nFees;
        double nPriorityDummy = 0;
        pool.ApplyDeltas(hash, nPriorityDummy, nModifiedFees);

        CAmount inChainInputValue;
        double dPriority = view.GetPriority(tx, chainActive.Height(), inChainInputValue);

        // Keep track of transactions that spend a coinbase, which we re-scan
        // during reorgs to ensure COINBASE_MATURITY is still met.
        bool fSpendsCoinbase = false;
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            const Coin& coin = view.AccessCoin(txin.prevout);
            if (coin.IsCoinBase()) {
                fSpendsCoinbase = true;
                break;
            }
        }

        CTxMemPoolEntry entry(ptx, nFees, nAcceptTime, dPriority, chainActive.Height(),
            inChainInputValue, fSpendsCoinbase, nSigOps, lp);
        unsigned int nSize = entry.GetTxSize();

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_STANDARD_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        if ((nSigOps > MAX_STANDARD_TX_SIGOPS) || (nBytesPerSigOp && nSigOps > nSize / nBytesPerSigOp))
            return state.DoS(0, false, REJECT_NONSTANDARD, "bad-txns-too-many-sigops", false,
                strprintf("%d", nSigOps));

        CAmount mempoolRejectFee = pool.GetMinFee(GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000).GetFee(nSize);
        if (mempoolRejectFee > 0 && nModifiedFees < mempoolRejectFee) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool min fee not met", false, strprintf("%d < %d", nFees, mempoolRejectFee));
        } else if (GetBoolArg("-relaypriority", DEFAULT_RELAYPRIORITY) && nModifiedFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(entry.GetPriority(chainActive.Height() + 1))) {
            // Require that free transactions have sufficient priority to be mined in the next block.
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
        }

        // Continuously rate-limit free (really, very-low-fee) transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (fLimitFree && nModifiedFees < ::minRelayTxFee.GetFee(nSize)) {
            static CCriticalSection csFreeLimiter;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            LOCK(csFreeLimiter);

            // Use an exponentially decaying ~10-minute window:
            dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
            nLastTime = nNow;
            // -limitfreerelay unit is thousand-bytes-per-minute
            // At default rate it would take over a month to fill 1GB
            if (dFreeCount + nSize >= GetArg("-limitfreerelay", DEFAULT_LIMITFREERELAY) * 10 * 1000)
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "rate limited free transaction");
            LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
            dFreeCount += nSize;
        }

        if (nAbsurdFee && nFees - nBDAPBurn > nAbsurdFee)
            return state.Invalid(false,
                REJECT_HIGHFEE, "absurdly-high-fee",
                strprintf("%d > %d", nFees, nAbsurdFee));

        // Calculate in-mempool ancestors, up to a limit.
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT) * 1000;
        size_t nLimitDescendants = GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) * 1000;
        std::string errString;
        if (!pool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
            return state.DoS(0, false, REJECT_NONSTANDARD, "too-long-mempool-chain", false, errString);
        }

        // A transaction that spends outputs that would be replaced by it is invalid. Now
        // that we have the set of all ancestors we can detect this
        // pathological case by making sure setConflicts and setAncestors don't
        // intersect.
        BOOST_FOREACH (CTxMemPool::txiter ancestorIt, setAncestors) {
            const uint256& hashAncestor = ancestorIt->GetTx().GetHash();
            if (setConflicts.count(hashAncestor)) {
                return state.DoS(10, false,
                    REJECT_INVALID, "bad-txns-spends-conflicting-tx", false,
                    strprintf("%s spends conflicting transaction %s",
                        hash.ToString(),
                        hashAncestor.ToString()));
            }
        }

        // Check if it's economically rational to mine this transaction rather
        // than the ones it replaces.
        CAmount nConflictingFees = 0;
        size_t nConflictingSize = 0;
        uint64_t nConflictingCount = 0;
        CTxMemPool::setEntries allConflicting;

        // If we don't hold the lock allConflicting might be incomplete; the
        // subsequent RemoveStaged() and addUnchecked() calls don't guarantee
        // mempool consistency for us.
        LOCK(pool.cs);
        const bool fReplacementTransaction = setConflicts.size();
        if (fReplacementTransaction) {
            CFeeRate newFeeRate(nModifiedFees, nSize);
            std::set<uint256> setConflictsParents;
            const int maxDescendantsToVisit = 100;
            CTxMemPool::setEntries setIterConflicting;
            BOOST_FOREACH (const uint256& hashConflicting, setConflicts) {
                CTxMemPool::txiter mi = pool.mapTx.find(hashConflicting);
                if (mi == pool.mapTx.end())
                    continue;

                // Save these to avoid repeated lookups
                setIterConflicting.insert(mi);

                // Don't allow the replacement to reduce the feerate of the
                // mempool.
                //
                // We usually don't want to accept replacements with lower
                // feerates than what they replaced as that would lower the
                // feerate of the next block. Requiring that the feerate always
                // be increased is also an easy-to-reason about way to prevent
                // DoS attacks via replacements.
                //
                // The mining code doesn't (currently) take children into
                // account (CPFP) so we only consider the feerates of
                // transactions being directly replaced, not their indirect
                // descendants. While that does mean high feerate children are
                // ignored when deciding whether or not to replace, we do
                // require the replacement to pay more overall fees too,
                // mitigating most cases.
                CFeeRate oldFeeRate(mi->GetModifiedFee(), mi->GetTxSize());
                if (newFeeRate <= oldFeeRate) {
                    return state.DoS(0, false,
                        REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                        strprintf("rejecting replacement %s; new feerate %s <= old feerate %s",
                            hash.ToString(),
                            newFeeRate.ToString(),
                            oldFeeRate.ToString()));
                }

                BOOST_FOREACH (const CTxIn& txin, mi->GetTx().vin) {
                    setConflictsParents.insert(txin.prevout.hash);
                }

                nConflictingCount += mi->GetCountWithDescendants();
            }
            // This potentially overestimates the number of actual descendants
            // but we just want to be conservative to avoid doing too much
            // work.
            if (nConflictingCount <= maxDescendantsToVisit) {
                // If not too many to replace, then calculate the set of
                // transactions that would have to be evicted
                BOOST_FOREACH (CTxMemPool::txiter it, setIterConflicting) {
                    pool.CalculateDescendants(it, allConflicting);
                }
                BOOST_FOREACH (CTxMemPool::txiter it, allConflicting) {
                    nConflictingFees += it->GetModifiedFee();
                    nConflictingSize += it->GetTxSize();
                }
            } else {
                return state.DoS(0, false,
                    REJECT_NONSTANDARD, "too many potential replacements", false,
                    strprintf("rejecting replacement %s; too many potential replacements (%d > %d)\n",
                        hash.ToString(),
                        nConflictingCount,
                        maxDescendantsToVisit));
            }

            for (unsigned int j = 0; j < tx.vin.size(); j++) {
                // We don't want to accept replacements that require low
                // feerate junk to be mined first. Ideally we'd keep track of
                // the ancestor feerates and make the decision based on that,
                // but for now requiring all new inputs to be confirmed works.
                if (!setConflictsParents.count(tx.vin[j].prevout.hash)) {
                    // Rather than check the UTXO set - potentially expensive -
                    // it's cheaper to just check if the new input refers to a
                    // tx that's in the mempool.
                    if (pool.mapTx.find(tx.vin[j].prevout.hash) != pool.mapTx.end())
                        return state.DoS(0, false,
                            REJECT_NONSTANDARD, "replacement-adds-unconfirmed", false,
                            strprintf("replacement %s adds unconfirmed input, idx %d",
                                hash.ToString(), j));
                }
            }

            // The replacement must pay greater fees than the transactions it
            // replaces - if we did the bandwidth used by those conflicting
            // transactions would not be paid for.
            if (nModifiedFees < nConflictingFees) {
                return state.DoS(0, false,
                    REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                    strprintf("rejecting replacement %s, less fees than conflicting txs; %s < %s",
                        hash.ToString(), FormatMoney(nModifiedFees), FormatMoney(nConflictingFees)));
            }

            // Finally in addition to paying more fees than the conflicts the
            // new transaction must pay for its own bandwidth.
            CAmount nDeltaFees = nModifiedFees - nConflictingFees;
            if (nDeltaFees < ::incrementalRelayFee.GetFee(nSize)) {
                return state.DoS(0, false,
                    REJECT_INSUFFICIENTFEE, "insufficient fee", false,
                    strprintf("rejecting replacement %s, not enough additional fees to relay; %s < %s",
                        hash.ToString(),
                        FormatMoney(nDeltaFees),
                        FormatMoney(::incrementalRelayFee.GetFee(nSize))));
            }
        }

        // If we aren't going to actually accept it but just were verifying it, we are fine already
        if (fDryRun)
            return true;

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!CheckInputs(tx, state, view, true, STANDARD_SCRIPT_VERIFY_FLAGS, true))
            return false; // state filled in by CheckInputs

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true)) {
            return error("%s: BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s, %s",
                __func__, hash.ToString(), FormatStateMessage(state));
        }

        if (tx.nVersion == BDAP_TX_VERSION && !ValidateBDAPInputs(ptx, state, view, CBlock(), true, chainActive.Height())) {
            return false;
        }

        // Remove conflicting transactions from the mempool
        BOOST_FOREACH (const CTxMemPool::txiter it, allConflicting) {
            LogPrint("mempool", "replacing tx %s with %s for %s %s additional fees, %d delta bytes\n",
                it->GetTx().GetHash().ToString(),
                hash.ToString(),
                FormatMoney(nModifiedFees - nConflictingFees),
                CURRENCY_UNIT,
                (int)nSize - (int)nConflictingSize);
            if (plTxnReplaced)
                plTxnReplaced->push_back(it->GetSharedTx());
        }
        pool.RemoveStaged(allConflicting, false);

        // This transaction should only count for fee estimation if it isn't a
        // BIP 125 replacement transaction (may not be widely supported), the
        // node is not behind, and the transaction is not dependent on any other
        // transactions in the mempool.
        bool validForFeeEstimation = !fReplacementTransaction && IsCurrentForFeeEstimation() && pool.HasNoInputsOf(tx);

        // Store transaction in memory
        pool.addUnchecked(hash, entry, setAncestors, validForFeeEstimation);

        // Add memory address index
        if (fAddressIndex) {
            pool.addAddressIndex(entry, view);
        }

        // Add memory spent index
        if (fSpentIndex) {
            pool.addSpentIndex(entry, view);
        }

        // trim mempool and check if tx was trimmed
        if (!fOverrideMempoolLimit) {
            LimitMempoolSize(pool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
            if (!pool.exists(hash))
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "mempool full");
        }
    }

    if (!fDryRun)
        GetMainSignals().SyncTransaction(tx, NULL, CMainSignals::SYNC_TRANSACTION_NOT_IN_BLOCK);

    return true;
}

bool AcceptToMemoryPoolWithTime(CTxMemPool& pool, CValidationState& state, const CTransactionRef& tx, bool fLimitFree, bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced, bool fOverrideMempoolLimit, const CAmount nAbsurdFee, bool fDryRun)
{
    std::vector<COutPoint> coins_to_uncache;
    bool res = AcceptToMemoryPoolWorker(pool, state, tx, fLimitFree, pfMissingInputs, nAcceptTime, plTxnReplaced, fOverrideMempoolLimit, nAbsurdFee, coins_to_uncache, fDryRun);
    bool fluidTimestampCheck = true;

    if (!fluid.ProvisionalCheckTransaction(*tx))
        return false;

    BOOST_FOREACH (const CTxOut& txout, tx->vout) {
        if (IsTransactionFluid(txout.scriptPubKey)) {
            std::string strErrorMessage;
            if (!fluid.CheckFluidOperationScript(txout.scriptPubKey, GetTime(), strErrorMessage)) {
                fluidTimestampCheck = false;
            }
        }
    }

    if (!res || fDryRun || !fluidTimestampCheck) {
        if (!res)
            LogPrint("mempool", "%s: %s %s %s\n", __func__, tx->GetHash().ToString(), state.GetRejectReason(), state.GetDebugMessage());
        BOOST_FOREACH (const COutPoint& hashTx, coins_to_uncache)
            pcoinsTip->Uncache(hashTx);
    }
    // After we've (potentially) uncached entries, ensure our coins cache is still within its size limits
    CValidationState stateDummy;
    FlushStateToDisk(stateDummy, FLUSH_STATE_PERIODIC);
    return res;
}

bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState& state, const CTransactionRef& tx, bool fLimitFree, bool* pfMissingInputs, std::list<CTransactionRef>* plTxnReplaced, bool fOverrideMempoolLimit, const CAmount nAbsurdFee, bool fDryRun)
{
    return AcceptToMemoryPoolWithTime(pool, state, tx, fLimitFree, pfMissingInputs, GetTime(), plTxnReplaced, fOverrideMempoolLimit, nAbsurdFee, fDryRun);
}

bool GetTimestampIndex(const unsigned int& high, const unsigned int& low, std::vector<uint256>& hashes)
{
    if (!fTimestampIndex)
        return error("Timestamp index not enabled");

    if (!pblocktree->ReadTimestampIndex(high, low, hashes))
        return error("Unable to get hashes for timestamps");

    return true;
}

bool GetSpentIndex(CSpentIndexKey& key, CSpentIndexValue& value)
{
    if (!fSpentIndex)
        return false;

    if (mempool.getSpentIndex(key, value))
        return true;

    if (!pblocktree->ReadSpentIndex(key, value))
        return false;

    return true;
}

bool GetAddressIndex(uint160 addressHash, int type, std::vector<std::pair<CAddressIndexKey, CAmount> >& addressIndex, int start, int end)
{
    if (!fAddressIndex)
        return error("address index not enabled");

    if (!pblocktree->ReadAddressIndex(addressHash, type, addressIndex, start, end))
        return error("unable to get txids for address");

    return true;
}

bool GetAddressUnspent(uint160 addressHash, int type, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >& unspentOutputs)
{
    if (!fAddressIndex)
        return error("address index not enabled");

    if (!pblocktree->ReadAddressUnspentIndex(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");

    return true;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256& hash, CTransactionRef& txOut, const Consensus::Params& consensusParams, uint256& hashBlock, bool fAllowSlow)
{
    CBlockIndex* pindexSlow = NULL;

    LOCK(cs_main);

    CTransactionRef ptx = mempool.get(hash);
    if (ptx) {
        txOut = ptx;
        return true;
    }

    if (fTxIndex) {
        CDiskTxPos postx;
        if (pblocktree->ReadTxIndex(hash, postx)) {
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
                return error("%s: OpenBlockFile failed", __func__);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txOut;
            } catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }
            hashBlock = header.GetHash();
            if (txOut->GetHash() != hash)
                return error("%s: txid mismatch", __func__);
            return true;
        }
        // transaction not found in index, nothing more can be done
        return false;
    }

    if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
        const Coin& coin = AccessByTxid(*pcoinsTip, hash);
        if (!coin.IsSpent())
            pindexSlow = chainActive[coin.nHeight];
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow, consensusParams)) {
            for (const auto& tx : block.vtx) {
                if (tx->GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk: OpenBlockFile failed");

    // Write index header
    unsigned int nSize = GetSerializeSize(fileout, block);
    fileout << FLATDATA(messageStart) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk: ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk: OpenBlockFile failed for %s", pos.ToString());

    // Read block
    try {
        filein >> block;
    } catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s at %s", __func__, e.what(), pos.ToString());
    }

    // Check the header
    if (!CheckProofOfWork(block.GetHash(), block.nBits, consensusParams))
        return error("ReadBlockFromDisk: Errors in block header at %s", pos.ToString());

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos(), consensusParams))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*): GetHash() doesn't match index for %s at %s",
            pindex->ToString(), pindex->GetBlockPos().ToString());
    return true;
}

bool IsInitialBlockDownload()
{
    // Once this function has returned false, it must remain false.
    static std::atomic<bool> latchToFalse{false};
    // Optimization: pre-test latch before taking the lock.
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;

    LOCK(cs_main);
    if (latchToFalse.load(std::memory_order_relaxed))
        return false;
    if (fImporting || fReindex)
        return true;
    const CChainParams& chainParams = Params();
    if (chainActive.Tip() == NULL)
        return true;
    if (chainActive.Tip()->nChainWork < int64_t(chainParams.GetConsensus().nMinimumChainWork))
        return true;
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 96 * 6 ||
                  std::max(chainActive.Tip()->GetBlockTime(), pindexBestHeader->GetBlockTime()) < GetTime() - nMaxTipAge);
    if (!state)
        latchToFalse.store(true, std::memory_order_relaxed);
    return state;
}

CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 120 blocks (+/- 1 hour if no one mines it)
    // of our head, drop it
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 120)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6))) {
        if (!GetfLargeWorkForkFound() && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                                      pindexBestForkBase->phashBlock->ToString() + std::string("'");
                CAlert::Notify(warning);
            }
        }
        if (pindexBestForkTip && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                LogPrintf("%s: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n", __func__,
                    pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                    pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
                SetfLargeWorkForkFound(true);
            }
        } else {
            if (pindexBestInvalid->nHeight > chainActive.Height() + 40)
                LogPrintf("%s: Warning: Found invalid chain at least ~40 blocks longer than our best chain.\nChain state database corruption likely.\n", __func__);
            else
                LogPrintf("%s: Warning: Found invalid chain which has higher work (at least ~10 blocks worth of work) than our best chain.\nChain state database corruption likely.\n", __func__);
            SetfLargeWorkInvalidChainFound(true);
        }
    } else {
        SetfLargeWorkForkFound(false);
        SetfLargeWorkInvalidChainFound(false);
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger) {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition where we should warn the user about as a fork of at least 12 blocks
    // with a tip within 120 blocks (+/- 1 hour if no one mines it) of ours
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We use 12 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 12-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
        pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 12) &&
        chainActive.Height() - pindexNewForkTip->nHeight < 120) {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("%s: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
        pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
        log(pindexNew->nChainWork.getdouble()) / log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexNew->GetBlockTime()));
    CBlockIndex* tip = chainActive.Tip();
    assert(tip);
    LogPrintf("%s:  current best=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
        tip->GetBlockHash().ToString(), chainActive.Height(), log(tip->nChainWork.getdouble()) / log(2.0),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", tip->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex* pindex, const CValidationState& state)
{
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, CTxUndo& txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            txundo.vprevout.emplace_back();
            bool is_spent = inputs.SpendCoin(txin.prevout, &txundo.vprevout.back());
            assert(is_spent);
        }
    }
    // add outputs
    AddCoins(inputs, tx, nHeight);
}

void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()()
{
    const CScript& scriptSig = ptxTo->vin[nIn].scriptSig;
    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, cacheStore), &error)) {
        return false;
    }
    return true;
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
    return pindexPrev->nHeight + 1;
}

namespace Consensus
{
bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight)
{
    // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
    // for an attacker to attempt to split the network.
    if (!inputs.HaveInputs(tx))
        return state.Invalid(false, 0, "", "Inputs unavailable");

    CAmount nValueIn = 0;
    CAmount nFees = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const COutPoint& prevout = tx.vin[i].prevout;
        const Coin& coin = inputs.AccessCoin(prevout);
        assert(!coin.IsSpent());

        // If prev is coinbase, check that it's matured
        if (coin.IsCoinBase()) {
            if (nSpendHeight - coin.nHeight < COINBASE_MATURITY)
                return state.Invalid(false,
                    REJECT_INVALID, "bad-txns-premature-spend-of-coinbase",
                    strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin.nHeight));
        }

        // Check for negative or overflow input values
        nValueIn += coin.out.nValue;
        if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
    }

    if (nValueIn < tx.GetValueOut())
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
            strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())));

    // Tally transaction fees
    CAmount nTxFee = nValueIn - tx.GetValueOut();
    if (nTxFee < 0)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-negative");
    nFees += nTxFee;
    if (!MoneyRange(nFees))
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    return true;
}
} // namespace Consensus

bool CheckInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, std::vector<CScriptCheck>* pvChecks)
{
    if (!tx.IsCoinBase()) {
        if (!Consensus::CheckTxInputs(tx, state, inputs, GetSpendHeight(inputs)))
            return false;

        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint& prevout = tx.vin[i].prevout;
                const Coin& coin = inputs.AccessCoin(prevout);
                assert(!coin.IsSpent());

                // We very carefully only pass in things to CScriptCheck which
                // are clearly committed to by tx' witness hash. This provides
                // a sanity check that our caching is not introducing consensus
                // failures through additional data in, eg, the coins being
                // spent being checked as a part of CScriptCheck.
                const CScript& scriptPubKey = coin.out.scriptPubKey;
                const CAmount amount = coin.out.nValue;

                // Verify signature
                CScriptCheck check(scriptPubKey, amount, tx, i, flags, cacheStore);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check2(scriptPubKey, amount, tx, i,
                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.DoS(100, false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}

namespace
{
bool UndoWriteToDisk(const CBlockUndo& blockundo, CDiskBlockPos& pos, const uint256& hashBlock, const CMessageHeader::MessageStartChars& messageStart)
{
    // Open history file to append
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    // Write index header
    unsigned int nSize = GetSerializeSize(fileout, blockundo);
    fileout << FLATDATA(messageStart) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    pos.nPos = (unsigned int)fileOutPos;
    fileout << blockundo;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << blockundo;
    fileout << hasher.GetHash();

    return true;
}

bool UndoReadFromDisk(CBlockUndo& blockundo, const CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenUndoFile failed", __func__);

    // Read block
    uint256 hashChecksum;
    CHashVerifier<CAutoFile> verifier(&filein); // We need a CHashVerifier as reserializing may lose data
    try {
        verifier << hashBlock;
        verifier >> blockundo;
        filein >> hashChecksum;
    } catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    if (hashChecksum != verifier.GetHash())
        return error("%s: Checksum mismatch", __func__);

    return true;
}

/** Abort with a message */
bool AbortNode(const std::string& strMessage, const std::string& userMessage = "")
{
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occurred, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool AbortNode(CValidationState& state, const std::string& strMessage, const std::string& userMessage = "")
{
    AbortNode(strMessage, userMessage);
    return state.Error(strMessage);
}

} // namespace

enum DisconnectResult {
    DISCONNECT_OK,      // All good.
    DISCONNECT_UNCLEAN, // Rolled back, but UTXO set was inconsistent with block.
    DISCONNECT_FAILED   // Something else went wrong.
};

/**
 * Restore the UTXO in a Coin at a given COutPoint
 * @param undo The Coin to be restored.
 * @param view The coins view to which to apply the changes.
 * @param out The out point that corresponds to the tx input.
 * @return A DisconnectResult as an int
 */
int ApplyTxInUndo(Coin&& undo, CCoinsViewCache& view, const COutPoint& out)
{
    bool fClean = true;

    if (view.HaveCoin(out))
        fClean = false; // overwriting transaction output

    if (undo.nHeight == 0) {
        // Missing undo metadata (height and coinbase). Older versions included this
        // information only in undo records for the last spend of a transactions'
        // outputs. This implies that it must be present for some other output of the same tx.
        const Coin& alternate = AccessByTxid(view, out.hash);
        if (!alternate.IsSpent()) {
            undo.nHeight = alternate.nHeight;
            undo.fCoinBase = alternate.fCoinBase;
        } else {
            return DISCONNECT_FAILED; // adding output for transaction without known metadata
        }
    }
    view.AddCoin(out, std::move(undo), undo.fCoinBase);

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}

/** Undo the effects of this block (with given index) on the UTXO set represented by coins.
 *  When UNCLEAN or FAILED is returned, view is left in an indeterminate state. */
static DisconnectResult DisconnectBlock(const CBlock& block, CValidationState& state, const CBlockIndex* pindex, CCoinsViewCache& view, int nCheckLevel)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());
    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull()) {
        error("DisconnectBlock(): no undo data available");
        return DISCONNECT_FAILED;
    }
    if (!UndoReadFromDisk(blockUndo, pos, pindex->pprev->GetBlockHash())) {
        error("DisconnectBlock(): failure reading undo data");
        return DISCONNECT_FAILED;
    }

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size()) {
        error("DisconnectBlock(): block and undo data inconsistent");
        return DISCONNECT_FAILED;
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressUnspentIndex;
    std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> > spentIndex;

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction& tx = *block.vtx[i];
        uint256 hash = tx.GetHash();
        bool is_coinbase = tx.IsCoinBase();
        bool fIsBDAP = tx.nVersion == BDAP_TX_VERSION;
        if (fIsBDAP && !fReindex && nCheckLevel >= 4) {
            LogPrintf("%s -- BDAP tx found. Hash %s\n", __func__, hash.ToString());
            // get BDAP object
            CScript scriptBDAPOp;
            std::vector<std::vector<unsigned char>> vvchOpParameters;
            int op1, op2;
            CTransactionRef ptx = MakeTransactionRef(tx);
            if (GetBDAPOpScript(ptx, scriptBDAPOp, vvchOpParameters, op1, op2)) {
                LogPrintf("%s -- Found new BDAP object, op1 %d, op2 %d\n", __func__, op1, op2);
                std::string strErrorMessage;
                std::string strOpType = GetBDAPOpTypeString(op1, op2);
                if (strOpType == "bdap_new_account") {
                    CDomainEntry domainEntry(ptx);
                    LogPrintf("%s -- Found new BDAP account %s. Running undo.\n", __func__, domainEntry.GetFullObjectPath());
                    if (!UndoAddDomainEntry(domainEntry)) {
                        LogPrintf("%s -- Failed to undo new BDAP account transaction %s. Disconnect %s transaction failed.\n", __func__, domainEntry.GetFullObjectPath(), hash.ToString());
                    }
                }
                else if (strOpType == "bdap_update_account") {
                    CDomainEntry domainEntry(ptx);
                    if (!UndoUpdateDomainEntry(domainEntry)) {
                        LogPrintf("%s -- Failed to undo update BDAP account transaction %s. Disconnect %s transaction failed.\n", __func__, domainEntry.GetFullObjectPath(), hash.ToString());
                    }
                }
                else if (strOpType == "bdap_delete_account") {
                    CDomainEntry domainEntry(ptx);
                    if (!UndoDeleteDomainEntry(domainEntry)) {
                        LogPrintf("%s -- Failed to undo delete BDAP account transaction %s. Disconnect %s transaction failed.\n", __func__, domainEntry.GetFullObjectPath(), hash.ToString());
                    }
                }
                else if (strOpType == "bdap_new_link_request" || strOpType == "bdap_new_link_accept") {
                    std::vector<unsigned char> vchPubKey, vchSharedPubKey;
                    if (vvchOpParameters.size() > 0)
                        vchPubKey = vvchOpParameters[0];

                    if (vvchOpParameters.size() > 1)
                        vchSharedPubKey = vvchOpParameters[0];

                    LogPrintf("%s -- Found new BDAP link (pubkey %s, sharedpubkey %s). Running undo.\n", __func__, stringFromVch(vchPubKey), stringFromVch(vchSharedPubKey));
                    if (!UndoLinkData(vchPubKey, vchSharedPubKey)) {
                        LogPrintf("%s -- Failed to undo link transaction. Disconnect %s transaction failed.\n", __func__, hash.ToString());
                    }
                }
                else if (strOpType == "bdap_new_audit") {
                    CAudit audit(ptx);
                    if (!UndoAddAudit(audit)) {
                        LogPrintf("%s -- Failed to undo add BDAP audit transaction %s. Disconnect %s transaction failed.\n", __func__, audit.ToString(), hash.ToString());
                    }
                }
                else if (strOpType == "bdap_new_certificate" || strOpType == "bdap_approve_certificate") {
                    CX509Certificate certificate(ptx);
                    if (!UndoAddCertificate(certificate)) {
                        LogPrintf("%s -- Failed to undo add BDAP certificate transaction %s. Disconnect %s transaction failed.\n", __func__, certificate.ToString(), hash.ToString());
                    }
                }
                else {
                    LogPrintf("%s -- Failed to undo unknown BDAP transaction (op1 = %d, op2 = %d). Nothing to undo for %s transaction.\n", __func__, op1, op2, hash.ToString());
                }
            }
        }
        if (fAddressIndex) {
            for (unsigned int k = tx.vout.size(); k-- > 0;) {
                const CTxOut& out = tx.vout[k];

                if (out.scriptPubKey.IsPayToScriptHash()) {
                    // Remove BDAP portion of the script
                    CScript scriptPubKey;
                    CScript scriptPubKeyOut;
                    if (RemoveBDAPScript(out.scriptPubKey, scriptPubKeyOut)) {
                        scriptPubKey = scriptPubKeyOut;
                    } else {
                        scriptPubKey = out.scriptPubKey;
                    }

                    std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);

                    // undo receiving activity
                    addressIndex.push_back(std::make_pair(CAddressIndexKey(2, uint160(hashBytes), pindex->nHeight, i, hash, k, false), out.nValue));

                    // undo unspent index
                    addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(2, uint160(hashBytes), hash, k), CAddressUnspentValue()));

                } else if (out.scriptPubKey.IsPayToPublicKeyHash()) {
                    // Remove BDAP portion of the script
                    CScript scriptPubKey;
                    CScript scriptPubKeyOut;
                    if (RemoveBDAPScript(out.scriptPubKey, scriptPubKeyOut)) {
                        scriptPubKey = scriptPubKeyOut;
                    } else {
                        scriptPubKey = out.scriptPubKey;
                    }

                    std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 3, scriptPubKey.begin() + 23);

                    // undo receiving activity
                    addressIndex.push_back(std::make_pair(CAddressIndexKey(1, uint160(hashBytes), pindex->nHeight, i, hash, k, false), out.nValue));

                    // undo unspent index
                    addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(1, uint160(hashBytes), hash, k), CAddressUnspentValue()));

                } else {
                    continue;
                }
            }
        }

        // Check that all outputs are available and match the outputs in the block itself
        // exactly.
        for (size_t o = 0; o < tx.vout.size(); o++) {
            if (!tx.vout[o].scriptPubKey.IsUnspendable()) {
                COutPoint out(hash, o);
                Coin coin;
                bool is_spent = view.SpendCoin(out, &coin);
                if (!is_spent || tx.vout[o] != coin.out || pindex->nHeight != coin.nHeight || is_coinbase != coin.fCoinBase) {
                    fClean = false; // transaction output mismatch
                }
            }
        }

        // restore inputs
        if (i > 0) { // not coinbases
            CTxUndo& txundo = blockUndo.vtxundo[i - 1];
            if (txundo.vprevout.size() != tx.vin.size()) {
                error("DisconnectBlock(): transaction and undo data inconsistent");
                return DISCONNECT_FAILED;
            }
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint& out = tx.vin[j].prevout;
                int undoHeight = txundo.vprevout[j].nHeight;
                int res = ApplyTxInUndo(std::move(txundo.vprevout[j]), view, out);
                if (res == DISCONNECT_FAILED)
                    return DISCONNECT_FAILED;
                fClean = fClean && res != DISCONNECT_UNCLEAN;

                const CTxIn input = tx.vin[j];

                if (fSpentIndex) {
                    // undo and delete the spent index
                    spentIndex.push_back(std::make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue()));
                }

                if (fAddressIndex) {
                    const Coin& coin = view.AccessCoin(tx.vin[j].prevout);
                    const CTxOut& prevout = coin.out;
                    if (prevout.scriptPubKey.IsPayToScriptHash()) {
                        // Remove BDAP portion of the script
                        CScript scriptPubKey;
                        CScript scriptPubKeyOut;
                        if (RemoveBDAPScript(prevout.scriptPubKey, scriptPubKeyOut)) {
                            scriptPubKey = scriptPubKeyOut;
                        } else {
                            scriptPubKey = prevout.scriptPubKey;
                        }

                        std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
                        // undo spending activity
                        addressIndex.push_back(std::make_pair(CAddressIndexKey(2, uint160(hashBytes), pindex->nHeight, i, hash, j, true), prevout.nValue * -1));
                        // restore unspent index
                        addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(2, uint160(hashBytes), input.prevout.hash, input.prevout.n), CAddressUnspentValue(prevout.nValue, scriptPubKey, undoHeight)));

                    } else if (prevout.scriptPubKey.IsPayToPublicKeyHash()) {
                        // Remove BDAP portion of the script
                        CScript scriptPubKey;
                        CScript scriptPubKeyOut;
                        if (RemoveBDAPScript(prevout.scriptPubKey, scriptPubKeyOut)) {
                            scriptPubKey = scriptPubKeyOut;
                        } else {
                            scriptPubKey = prevout.scriptPubKey;
                        }

                        std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 3, scriptPubKey.begin() + 23);
                        // undo spending activity
                        addressIndex.push_back(std::make_pair(CAddressIndexKey(1, uint160(hashBytes), pindex->nHeight, i, hash, j, true), prevout.nValue * -1));
                        // restore unspent index
                        addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(1, uint160(hashBytes), input.prevout.hash, input.prevout.n), CAddressUnspentValue(prevout.nValue, scriptPubKey, undoHeight)));
                    } else {
                        continue;
                    }
                }
            }
            // At this point, all of txundo.vprevout should have been moved out.
        }
    }


    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (fAddressIndex) {
        if (!pblocktree->EraseAddressIndex(addressIndex)) {
            AbortNode(state, "Failed to delete address index");
            return DISCONNECT_FAILED;
        }
        if (!pblocktree->UpdateAddressUnspentIndex(addressUnspentIndex)) {
            AbortNode(state, "Failed to write address unspent index");
            return DISCONNECT_FAILED;
        }
    }

    return fClean ? DISCONNECT_OK : DISCONNECT_UNCLEAN;
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE* fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck()
{
    RenameThread("cash-scriptch");
    scriptcheckqueue.Thread();
}

// Protected by cs_main
VersionBitsCache versionbitscache;

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params, bool fAssumeMasternodeIsUpgraded)
{
    LOCK(cs_main);
    int32_t nVersion = VERSIONBITS_TOP_BITS;

    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        Consensus::DeploymentPos pos = Consensus::DeploymentPos(i);
        ThresholdState state = VersionBitsState(pindexPrev, params, pos, versionbitscache);
        const struct BIP9DeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
        if (vbinfo.check_mn_protocol && state == THRESHOLD_STARTED && !fAssumeMasternodeIsUpgraded) {
            CScript payee;
            masternode_info_t mnInfo;
            if (!mnpayments.GetBlockPayee(pindexPrev->nHeight + 1, payee)) {
                // no votes for this block
                continue;
            }
            if (!mnodeman.GetMasternodeInfo(payee, mnInfo)) {
                // unknown masternode
                continue;
            }
        }
        if (state == THRESHOLD_LOCKED_IN || state == THRESHOLD_STARTED) {
            nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
        }
    }

    return nVersion;
}

bool GetBlockHash(uint256& hashRet, int nBlockHeight)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL)
        return false;
    if (nBlockHeight < -1 || nBlockHeight > chainActive.Height())
        return false;
    if (nBlockHeight == -1)
        nBlockHeight = chainActive.Height();
    hashRet = chainActive[nBlockHeight]->GetBlockHash();
    return true;
}

/**
 * Threshold condition checker that triggers when unknown versionbits are seen on the network.
 */
class WarningBitsConditionChecker : public AbstractThresholdConditionChecker
{
private:
    int bit;

public:
    WarningBitsConditionChecker(int bitIn) : bit(bitIn) {}

    int64_t BeginTime(const Consensus::Params& params) const override { return 0; }
    int64_t EndTime(const Consensus::Params& params) const override { return std::numeric_limits<int64_t>::max(); }
    int Period(const Consensus::Params& params, int nHeight) const override { return params.GetCurrentMinerConfirmationWindow(nHeight); }
    int Threshold(const Consensus::Params& params, int nHeight) const override { return params.GetCurrentRuleChangeActivationThreshold(nHeight); }

    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const override
    {
        return ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) &&
               ((pindex->nVersion >> bit) & 1) != 0 &&
               ((ComputeBlockVersion(pindex->pprev, params) >> bit) & 1) == 0;
    }
};

// Protected by cs_main
static ThresholdConditionCache warningcache[VERSIONBITS_NUM_BITS];

static int64_t nTimeCheck = 0;
static int64_t nTimeForks = 0;
static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

/** Apply the effects of this block (with given index) on the UTXO set represented by coins.
 *  Validity checks that depend on the UTXO set are also done; ConnectBlock()
 *  can fail if those validity checks fail (among other reasons). */

/** Apply the effects of this block (with given index) on the UTXO set represented by coins.
 *  Validity checks that depend on the UTXO set are also done; ConnectBlock()
 *  can fail if those validity checks fail (among other reasons). */
static bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck = false)
{
    AssertLockHeld(cs_main);

    int64_t nTimeStart = GetTimeMicros();

    // Check it again in case a previous version let a bad block in
    if (!CheckBlock(block, state, chainparams.GetConsensus(), !fJustCheck, !fJustCheck))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256() : pindex->pprev->GetBlockHash();
    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHash() == chainparams.GetConsensus().hashGenesisBlock) {
        if (!fJustCheck)
            view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    bool fScriptChecks = true;
    if (!hashAssumeValid.IsNull()) {
        // We've been configured with the hash of a block which has been externally verified to have a valid history.
        // A suitable default value is included with the software and updated from time to time.  Because validity
        //  relative to a piece of software is an objective fact these defaults can be easily reviewed.
        // This setting doesn't force the selection of any particular chain but makes validating some faster by
        //  effectively caching the result of part of the verification.
        BlockMap::const_iterator it = mapBlockIndex.find(hashAssumeValid);
        if (it != mapBlockIndex.end()) {
            if (it->second->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->GetAncestor(pindex->nHeight) == pindex &&
                pindexBestHeader->nChainWork >= int64_t(chainparams.GetConsensus().nMinimumChainWork)) {

                // This block is a member of the assumed verified chain and an ancestor of the best header.
                // The equivalent time check discourages hashpower from extorting the network via DOS attack
                // into accepting an invalid block through telling users they must manually set assumevalid.
                // Requiring a software change or burying the invalid block, regardless of the setting, makes
                // it hard to hide the implication of the demand.  This also avoids having release candidates
                // that are hardly doing any signature verification at all in testing without having to
                // artificially set the default assumed verified block further back.
                // The test against nMinimumChainWork prevents the skipping when denied access to any chain at
                // least as good as the expected chain.
                fScriptChecks = (GetBlockProofEquivalentTime(*pindexBestHeader, *pindex, *pindexBestHeader, chainparams.GetConsensus()) <= 60 * 60 * 24 * 7 * 2);
            }
        }
    }

    int64_t nTime1 = GetTimeMicros();
    nTimeCheck += nTime1 - nTimeStart;
    LogPrint("bench", "    - Sanity checks: %.2fms [%.2fs]\n", 0.001 * (nTime1 - nTimeStart), nTimeCheck * 0.000001);

    // make sure old budget is the real one
    if (pindex->nHeight == chainparams.GetConsensus().nSuperblockStartBlock &&
        chainparams.GetConsensus().nSuperblockStartHash != uint256() &&
        block.GetHash() != chainparams.GetConsensus().nSuperblockStartHash)
        return state.DoS(100, error("ConnectBlock(): invalid superblock start"),
            REJECT_INVALID, "bad-sb-start");

    // BIP16 didn't become active until Apr 1 2012
    int64_t nBIP16SwitchTime = 1333238400;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    int nLockTimeFlags = 0;

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;
    flags |= SCRIPT_VERIFY_DERSIG;
    flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    nLockTimeFlags |= LOCKTIME_VERIFY_SEQUENCE;

    if (VersionBitsState(pindex->pprev, chainparams.GetConsensus(), Consensus::DEPLOYMENT_BIP147, versionbitscache) == THRESHOLD_ACTIVE) {
        flags |= SCRIPT_VERIFY_NULLDUMMY;
    }

    int64_t nTime2 = GetTimeMicros();
    nTimeForks += nTime2 - nTime1;
    LogPrint("bench", "    - Fork checks: %.2fms [%.2fs]\n", 0.001 * (nTime2 - nTime1), nTimeForks * 0.000001);

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    std::vector<uint256> vOrphanErase;
    std::vector<int> prevheights;
    CAmount nFees = 0;
    int nInputs = 0;
    unsigned int nSigOps = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressUnspentIndex;
    std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> > spentIndex;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        const uint256 txhash = tx.GetHash();

        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("ConnectBlock(): too many sigops"),
                REJECT_INVALID, "bad-blk-sigops");

        if (!tx.IsCoinBase()) {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock(): inputs missing/spent"),
                    REJECT_INVALID, "bad-txns-inputs-missingorspent");

            // Check that transaction is BIP68 final
            // BIP68 lock checks (as opposed to nLockTime checks) must
            // be in ConnectBlock because they require the UTXO set
            prevheights.resize(tx.vin.size());
            for (size_t j = 0; j < tx.vin.size(); j++) {
                prevheights[j] = view.AccessCoin(tx.vin[j].prevout).nHeight;
            }

            if (!SequenceLocks(tx, nLockTimeFlags, &prevheights, *pindex)) {
                return state.DoS(100, error("%s: contains a non-BIP68-final transaction", __func__),
                    REJECT_INVALID, "bad-txns-nonfinal");
            }
            if (fAddressIndex || fSpentIndex) {
                for (size_t j = 0; j < tx.vin.size(); j++) {
                    const CTxIn input = tx.vin[j];
                    const Coin& coin = view.AccessCoin(tx.vin[j].prevout);
                    const CTxOut& prevout = coin.out;
                    uint160 hashBytes;
                    int addressType;

                    if (prevout.scriptPubKey.IsPayToScriptHash()) {
                        hashBytes = uint160(std::vector<unsigned char>(prevout.scriptPubKey.begin() + 2, prevout.scriptPubKey.begin() + 22));
                        addressType = 2;
                    } else if (prevout.scriptPubKey.IsPayToPublicKeyHash()) {
                        hashBytes = uint160(std::vector<unsigned char>(prevout.scriptPubKey.begin() + 3, prevout.scriptPubKey.begin() + 23));
                        addressType = 1;
                    } else {
                        hashBytes.SetNull();
                        addressType = 0;
                    }

                    if (fAddressIndex && addressType > 0) {
                        // record spending activity
                        addressIndex.push_back(std::make_pair(CAddressIndexKey(addressType, hashBytes, pindex->nHeight, i, txhash, j, true), prevout.nValue * -1));

                        // remove address from unspent index
                        addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(addressType, hashBytes, input.prevout.hash, input.prevout.n), CAddressUnspentValue()));
                    }

                    if (fSpentIndex) {
                        // add the spent index to determine the txid and input that spent an output
                        // and to find the amount and address from an input
                        spentIndex.push_back(std::make_pair(CSpentIndexKey(input.prevout.hash, input.prevout.n), CSpentIndexValue(txhash, j, pindex->nHeight, prevout.nValue, addressType, hashBytes)));
                    }
                }
            }

            if (fStrictPayToScriptHash) {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += GetP2SHSigOpCount(tx, view);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return state.DoS(100, error("ConnectBlock(): too many sigops"),
                        REJECT_INVALID, "bad-blk-sigops");
            }

            nFees += view.GetValueIn(tx) - tx.GetValueOut();

            std::vector<CScriptCheck> vChecks;
            bool fCacheResults = fJustCheck; /* Don't cache results if we're actually connecting blocks (still consult the cache, though) */
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, fCacheResults, nScriptCheckThreads ? &vChecks : NULL))
                return error("ConnectBlock(): CheckInputs on %s failed with %s",
                    tx.GetHash().ToString(), FormatStateMessage(state));
            control.Add(vChecks);
        }

        if (fAddressIndex) {
            for (unsigned int k = 0; k < tx.vout.size(); k++) {
                const CTxOut& out = tx.vout[k];

                if (out.scriptPubKey.IsPayToScriptHash()) {
                    // Remove BDAP portion of the script
                    CScript scriptPubKey;
                    CScript scriptPubKeyOut;
                    if (RemoveBDAPScript(out.scriptPubKey, scriptPubKeyOut)) {
                        scriptPubKey = scriptPubKeyOut;
                    } else {
                        scriptPubKey = out.scriptPubKey;
                    }

                    std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
                    // record receiving activity
                    addressIndex.push_back(std::make_pair(CAddressIndexKey(2, uint160(hashBytes), pindex->nHeight, i, txhash, k, false), out.nValue));
                    // record unspent output
                    addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(2, uint160(hashBytes), txhash, k), CAddressUnspentValue(out.nValue, scriptPubKey, pindex->nHeight)));
                } else if (out.scriptPubKey.IsPayToPublicKeyHash()) {
                    // Remove BDAP portion of the script
                    CScript scriptPubKey;
                    CScript scriptPubKeyOut;
                    if (RemoveBDAPScript(out.scriptPubKey, scriptPubKeyOut)) {
                        scriptPubKey = scriptPubKeyOut;
                    } else {
                        scriptPubKey = out.scriptPubKey;
                    }

                    std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 3, scriptPubKey.begin() + 23);
                    // record receiving activity
                    addressIndex.push_back(std::make_pair(CAddressIndexKey(1, uint160(hashBytes), pindex->nHeight, i, txhash, k, false), out.nValue));
                    // record unspent output
                    addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(1, uint160(hashBytes), txhash, k), CAddressUnspentValue(out.nValue, scriptPubKey, pindex->nHeight)));
                } else {
                    continue;
                }
            }
        }

        CCoinsViewCache viewCoinCache(pcoinsTip);
        CTransactionRef ptx = MakeTransactionRef(tx);

        if (tx.nVersion == BDAP_TX_VERSION && !ValidateBDAPInputs(ptx, state, viewCoinCache, block, fJustCheck, pindex->nHeight)) {
            return error("ConnectBlock(): ValidateBDAPInputs on block %s failed\n", block.GetHash().ToString());
        }

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    int64_t nTime3 = GetTimeMicros();
    nTimeConnect += nTime3 - nTime2;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime3 - nTime2), 0.001 * (nTime3 - nTime2) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime3 - nTime2) / (nInputs - 1), nTimeConnect * 0.000001);

    // 0DYNC : MODIFIED TO CHECK MASTERNODE PAYMENTS AND SUPERBLOCKS

    // It's possible that we simply don't have enough data and this could fail
    // (i.e. block itself could be a correct one and we need to store it),
    // that's why this is in ConnectBlock. Could be the other way around however -
    // the peer who sent us this block is missing some data and wasn't able
    // to recognize that block is actually invalid.
    // TODO: resync data (both ways?) and try to reprocess this block later.
    bool fMasternodePaid = false;

    if (chainActive.Height() > Params().GetConsensus().nMasternodePaymentsStartBlock) {
        fMasternodePaid = true;
    } else if (chainActive.Height() <= Params().GetConsensus().nMasternodePaymentsStartBlock) {
        fMasternodePaid = false;
    }

    // BEGIN FLUID
    CAmount nExpectedBlockValue;
    std::string strError = "";
    {
        CBlockIndex* prevIndex = pindex->pprev;
        CAmount newMiningReward = GetFluidMiningReward(pindex->nHeight);
        CAmount newMasternodeReward = 0;
        if (fMasternodePaid)
            newMasternodeReward = GetFluidMasternodeReward(pindex->nHeight);

        CAmount newMintIssuance = 0;
        CDebitAddress mintAddress;
        if (prevIndex->nHeight + 1 >= fluid.FLUID_ACTIVATE_HEIGHT) {
            CFluidMint fluidMint;
            if (GetMintingInstructions(pindex->nHeight, fluidMint)) {
                newMintIssuance = fluidMint.MintAmount;
                mintAddress = fluidMint.GetDestinationAddress();
                LogPrintf("ConnectBlock, GetMintingInstructions MintAmount = %u\n", fluidMint.MintAmount);
            }
        }
        
        if (chainActive.Height() > Params().GetConsensus().nFeeRewardStart) {
            nExpectedBlockValue = nFees + newMintIssuance + newMiningReward + newMasternodeReward;
        } else if (chainActive.Height() <= Params().GetConsensus().nFeeRewardStart) {
            nExpectedBlockValue = newMintIssuance + newMiningReward + newMasternodeReward;
        }

        if (!IsBlockValueValid(block, pindex->nHeight, nExpectedBlockValue, strError)) {
            return state.DoS(0, error("ConnectBlock(0DYNC): %s", strError), REJECT_INVALID, "bad-cb-amount");
        }
        if (!IsBlockPayeeValid(*block.vtx[0], pindex->nHeight, nExpectedBlockValue)) {
            mapRejectedBlocks.insert(std::make_pair(block.GetHash(), GetTime()));
            return state.DoS(0, error("ConnectBlock(0DYNC): couldn't find Masternode or Superblock payments"),
                REJECT_INVALID, "bad-cb-payee");
        }
    }
    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        CScript scriptFluid;
        if (IsTransactionFluid(tx, scriptFluid)) {
            int OpCode = GetFluidOpCode(scriptFluid);
            if (OpCode == OP_REWARD_MASTERNODE) {
                CFluidMasternode fluidMasternode(scriptFluid);
                fluidMasternode.nHeight = pindex->nHeight;
                fluidMasternode.txHash = tx.GetHash();
                if (CheckFluidMasternodeDB()) {
                    if (!CheckSignatureQuorum(fluidMasternode.FluidScript, strError)) {
                        return state.DoS(0, error("ConnectBlock(0DYNC): %s", strError), REJECT_INVALID, "invalid-fluid-masternode-address-signature");
                    }
                    pFluidMasternodeDB->AddFluidMasternodeEntry(fluidMasternode, OP_REWARD_MASTERNODE);
                }
            } else if (OpCode == OP_REWARD_MINING) {
                CFluidMining fluidMining(scriptFluid);
                fluidMining.nHeight = pindex->nHeight;
                fluidMining.txHash = tx.GetHash();
                if (CheckFluidMiningDB()) {
                    if (!CheckSignatureQuorum(fluidMining.FluidScript, strError)) {
                        return state.DoS(0, error("ConnectBlock(0DYNC): %s", strError), REJECT_INVALID, "invalid-fluid-mining-address-signature");
                    }
                    pFluidMiningDB->AddFluidMiningEntry(fluidMining, OP_REWARD_MINING);
                }
            } else if (OpCode == OP_MINT) {
                CFluidMint fluidMint(scriptFluid);
                fluidMint.nHeight = pindex->nHeight;
                fluidMint.txHash = tx.GetHash();
                if (CheckFluidMintDB()) {
                    if (!CheckSignatureQuorum(fluidMint.FluidScript, strError)) {
                        return state.DoS(0, error("ConnectBlock(0DYNC): %s", strError), REJECT_INVALID, "invalid-fluid-mint-address-signature");
                    }
                    pFluidMintDB->AddFluidMintEntry(fluidMint, OP_MINT);
                }
            } else if (OpCode == OP_BDAP_REVOKE) {
                if (!CheckSignatureQuorum(FluidScriptToCharVector(scriptFluid), strError))
                    return state.DoS(0, error("%s: %s", __func__, strError), REJECT_INVALID, "invalid-fluid-ban-address-signature");

                //if (!sporkManager.IsSporkActive(SPORK_30_ACTIVATE_BDAP))
                //    return state.DoS(0, error("%s: BDAP spork is inactive.", __func__), REJECT_INVALID, "bdap-spork-inactive");

                std::vector<CDomainEntry> vBanAccounts;
                if (!fluid.CheckAccountBanScript(scriptFluid, tx.GetHash(), pindex->nHeight, vBanAccounts, strError))
                    return state.DoS(0, error("%s -- CheckAccountBanScript failed: %s", __func__, strError), REJECT_INVALID, "fluid-ban-script-invalid");

                int64_t nTimeStamp;
                std::vector<std::vector<unsigned char>> vSovereignAddresses;
                if (fluid.ExtractTimestampWithAddresses("OP_BDAP_REVOKE", scriptFluid, nTimeStamp, vSovereignAddresses)) {
                    for (const CDomainEntry& entry : vBanAccounts) {
                        LogPrintf("%s -- Fluid command banning account %s\n", __func__, entry.GetFullObjectPath());
                        if (!DeleteDomainEntry(entry))
                            LogPrintf("%s -- Error deleting account %s\n", __func__, entry.GetFullObjectPath());

                        CBanAccount banAccount(scriptFluid, entry.vchFullObjectPath(), nTimeStamp, vSovereignAddresses, tx.GetHash(), pindex->nHeight);
                        AddBanAccountEntry(banAccount);
                    }
                }
            } else {
                std::string strFluidOpScript = ScriptToAsmStr(scriptFluid);
                std::string strOperationCode = GetRidOfScriptStatement(strFluidOpScript, 0);
                return state.DoS(100, error("%s -- Invalid fluid operation code %s (%d)", __func__, strOperationCode, OpCode), REJECT_INVALID, "invalid-fluid-operation-code");
            }
        }
    }
    // END FLUID

    if (!control.Wait())
        return state.DoS(100, false);

    int64_t nTime4 = GetTimeMicros();
    nTimeVerify += nTime4 - nTime2;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime4 - nTime2), nInputs <= 1 ? 0 : 0.001 * (nTime4 - nTime2) / (nInputs - 1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos _pos;
            if (!FindUndoPos(state, pindex->nFile, _pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock(): FindUndoPos failed");
            if (!UndoWriteToDisk(blockundo, _pos, pindex->pprev->GetBlockHash(), chainparams.MessageStart()))
                return AbortNode(state, "Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = _pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return AbortNode(state, "Failed to write transaction index");

    if (fAddressIndex) {
        if (!pblocktree->WriteAddressIndex(addressIndex)) {
            return AbortNode(state, "Failed to write address index");
        }

        if (!pblocktree->UpdateAddressUnspentIndex(addressUnspentIndex)) {
            return AbortNode(state, "Failed to write address unspent index");
        }
    }

    if (fSpentIndex)
        if (!pblocktree->UpdateSpentIndex(spentIndex))
            return AbortNode(state, "Failed to write transaction index");

    if (fTimestampIndex)
        if (!pblocktree->WriteTimestampIndex(CTimestampIndexKey(pindex->nTime, pindex->GetBlockHash())))
            return AbortNode(state, "Failed to write timestamp index");

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime5 = GetTimeMicros();
    nTimeIndex += nTime5 - nTime4;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime5 - nTime4), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0]->GetHash();


    int64_t nTime6 = GetTimeMicros();
    nTimeCallbacks += nTime6 - nTime5;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime6 - nTime5), nTimeCallbacks * 0.000001);

    return true;
}

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed depending on the mode we're called with
 * if they're too large, if it's been a while since the last write,
 * or always and in all cases if we're in prune mode and are deleting files.
 */
bool static FlushStateToDisk(CValidationState& state, FlushStateMode mode, int nManualPruneHeight)
{
    int64_t nMempoolUsage = mempool.DynamicMemoryUsage();
    const CChainParams& chainparams = Params();
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    static int64_t nLastFlush = 0;
    static int64_t nLastSetChain = 0;
    std::set<int> setFilesToPrune;
    bool fFlushForPrune = false;
    try {
        if (fPruneMode && (fCheckForPruning || nManualPruneHeight > 0) && !fReindex) {
            if (nManualPruneHeight > 0) {
                FindFilesToPruneManual(setFilesToPrune, nManualPruneHeight);
            } else {
                FindFilesToPrune(setFilesToPrune, chainparams.PruneAfterHeight());
                fCheckForPruning = false;
            }
            if (!setFilesToPrune.empty()) {
                fFlushForPrune = true;
                if (!fHavePruned) {
                    pblocktree->WriteFlag("prunedblockfiles", true);
                    fHavePruned = true;
                }
            }
        }
        int64_t nNow = GetTimeMicros();
        // Avoid writing/flushing immediately after startup.
        if (nLastWrite == 0) {
            nLastWrite = nNow;
        }
        if (nLastFlush == 0) {
            nLastFlush = nNow;
        }
        if (nLastSetChain == 0) {
            nLastSetChain = nNow;
        }
        int64_t nMempoolSizeMax = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
        int64_t cacheSize = pcoinsTip->DynamicMemoryUsage() * DB_PEAK_USAGE_FACTOR;
        int64_t nTotalSpace = nCoinCacheUsage + std::max<int64_t>(nMempoolSizeMax - nMempoolUsage, 0);
        // The cache is large and we're within 10% and 10 MiB of the limit, but we have time now (not in the middle of a block processing).
        bool fCacheLarge = mode == FLUSH_STATE_PERIODIC && cacheSize > std::min(std::max(nTotalSpace / 2, nTotalSpace - MIN_BLOCK_COINSDB_USAGE * 1024 * 1024),
                                                                                std::max((9 * nTotalSpace) / 10, nTotalSpace - MAX_BLOCK_COINSDB_USAGE * 1024 * 1024));
        // The cache is over the limit, we have to write now.
        bool fCacheCritical = mode == FLUSH_STATE_IF_NEEDED && cacheSize > nTotalSpace;
        // It's been a while since we wrote the block index to disk. Do this frequently, so we don't need to redownload after a crash.
        bool fPeriodicWrite = mode == FLUSH_STATE_PERIODIC && nNow > nLastWrite + (int64_t)DATABASE_WRITE_INTERVAL * 1000000;
        // It's been very long since we flushed the cache. Do this infrequently, to optimize cache usage.
        bool fPeriodicFlush = mode == FLUSH_STATE_PERIODIC && nNow > nLastFlush + (int64_t)DATABASE_FLUSH_INTERVAL * 1000000;
        // Combine all conditions that result in a full cache flush.
        bool fDoFullFlush = (mode == FLUSH_STATE_ALWAYS) || fCacheLarge || fCacheCritical || fPeriodicFlush || fFlushForPrune;
        // Write blocks and block index to disk.
        if (fDoFullFlush || fPeriodicWrite) {
            // Depend on nMinDiskSpace to ensure we can write block index
            if (!CheckDiskSpace(0))
                return state.Error("out of disk space");
            // First make sure all block and undo data is flushed to disk.
            FlushBlockFile();
            // Then update all block file information (which may refer to block and undo files).
            {
                std::vector<std::pair<int, const CBlockFileInfo*> > vFiles;
                vFiles.reserve(setDirtyFileInfo.size());
                for (std::set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end();) {
                    vFiles.push_back(std::make_pair(*it, &vinfoBlockFile[*it]));
                    setDirtyFileInfo.erase(it++);
                }
                std::vector<const CBlockIndex*> vBlocks;
                vBlocks.reserve(setDirtyBlockIndex.size());
                for (std::set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end();) {
                    vBlocks.push_back(*it);
                    setDirtyBlockIndex.erase(it++);
                }
                if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                    return AbortNode(state, "Files to write to block index database");
                }
            }
            // Finally remove any pruned files
            if (fFlushForPrune)
                UnlinkPrunedFiles(setFilesToPrune);
            nLastWrite = nNow;
        }
        // Flush best chain related state. This can only be done if the blocks / block index write was also done.
        if (fDoFullFlush) {
            // Typical Coin structures on disk are around 48 bytes in size.
            // Pushing a new one to the database can cause it to be written
            // twice (once in the log, and once in the tables). This is already
            // an overestimation, as most will delete an existing entry or
            // overwrite one. Still, use a conservative safety factor of 2.
            if (!CheckDiskSpace(48 * 2 * 2 * pcoinsTip->GetCacheSize()))
                return state.Error("out of disk space");
            // Flush the chainstate (which may refer to block index entries).
            if (!pcoinsTip->Flush())
                return AbortNode(state, "Failed to write to coin database");
            nLastFlush = nNow;
        }
        if (fDoFullFlush || ((mode == FLUSH_STATE_ALWAYS || mode == FLUSH_STATE_PERIODIC) && nNow > nLastSetChain + (int64_t)DATABASE_WRITE_INTERVAL * 1000000)) {
            // Update best block in wallet (so we can detect restored wallets).
            GetMainSignals().SetBestChain(chainActive.GetLocator());
            nLastSetChain = nNow;
        }
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk()
{
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush()
{
    CValidationState state;
    fCheckForPruning = true;
    FlushStateToDisk(state, FLUSH_STATE_NONE);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex* pindexNew, const CChainParams& chainParams)
{
    chainActive.SetTip(pindexNew);

    // New best block
    mempool.AddTransactionsUpdated(1);

    {
        LOCK(g_best_block_mutex);
        g_best_block = pindexNew->GetBlockHash();
        g_best_block_cv.notify_all();
    }

    static bool fWarned = false;
    std::vector<std::string> warningMessages;
    if (!IsInitialBlockDownload()) {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int bit = 0; bit < VERSIONBITS_NUM_BITS; bit++) {
            WarningBitsConditionChecker checker(bit);
            ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
            if (state == THRESHOLD_ACTIVE || state == THRESHOLD_LOCKED_IN) {
                if (state == THRESHOLD_ACTIVE) {
                    std::string strWarning = strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
                    SetMiscWarning(strWarning);
                    if (!fWarned) {
                        CAlert::Notify(strWarning);
                        fWarned = true;
                    }
                } else {
                    warningMessages.push_back(strprintf("unknown new rules are about to activate (versionbit %i)", bit));
                }
            }
        }
        // Check the version of the last 100 blocks to see if we need to upgrade:
        for (int i = 0; i < 100 && pindex != NULL; i++) {
            int32_t nExpectedVersion = ComputeBlockVersion(pindex->pprev, chainParams.GetConsensus());
            if (pindex->nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && (pindex->nVersion & ~nExpectedVersion) != 0)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            warningMessages.push_back(strprintf("%d of last 100 blocks have unexpected version", nUpgraded));
        if (nUpgraded > 100 / 2) {
            std::string strWarning = _("Warning: Unknown block versions being mined! It's possible unknown rules are in effect");
            // notify GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            SetMiscWarning(strWarning);
            if (!fWarned) {
                CAlert::Notify(strWarning);
                fWarned = true;
            }
        }
    }
    LogPrint("validation", "%s: new best=%s height=%d version=0x%08x log2_work=%.8f tx=%lu date='%s' progress=%f cache=%.1fMiB(%utxo)\n", __func__,
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), chainActive.Tip()->nVersion,
        log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        GuessVerificationProgress(chainParams.TxData(), chainActive.Tip()), pcoinsTip->DynamicMemoryUsage() * (1.0 / (1 << 20)), pcoinsTip->GetCacheSize());
    if (!warningMessages.empty())
        LogPrintf("%s -- warning='%s'\n", __func__, boost::algorithm::join(warningMessages, ", "));
}

/** Disconnect chainActive's tip. You probably want to call mempool.removeForReorg and manually re-limit mempool size after this, with cs_main held. */
bool static DisconnectTip(CValidationState& state, const CChainParams& chainparams)
{

    CBlockIndex* pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    // Read block from disk.
    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete, chainparams.GetConsensus()))
        return AbortNode(state, "Failed to read block");
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (DisconnectBlock(block, state, pindexDelete, view, 4) != DISCONNECT_OK)
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        bool flushed = view.Flush();
        assert(flushed);
    }
    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;
    // Resurrect mempool transactions from the disconnected block.
    std::vector<uint256> vHashUpdate;
    for (const auto& it : block.vtx) {
        const CTransaction& tx = *it;
        // ignore validation errors in resurrected transactions
        CValidationState stateDummy;
        if (tx.IsCoinBase() || !AcceptToMemoryPool(mempool, stateDummy, it, false, NULL, NULL, true)) {
            mempool.removeRecursive(tx, MemPoolRemovalReason::REORG);
        } else if (mempool.exists(tx.GetHash())) {
            vHashUpdate.push_back(tx.GetHash());
        }
    }
    // AcceptToMemoryPool/addUnchecked all assume that new mempool entries have
    // no in-mempool children, which is generally not true when adding
    // previously-confirmed transactions back to the mempool.
    // UpdateTransactionsFromBlock finds descendants of any transactions in this
    // block that were added back and cleans up the mempool state.
    mempool.UpdateTransactionsFromBlock(vHashUpdate);
    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev, chainparams);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    for (const auto& tx : block.vtx) {
        GetMainSignals().SyncTransaction(*tx, pindexDelete->pprev, CMainSignals::SYNC_TRANSACTION_NOT_IN_BLOCK);
    }
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/**
 * Used to track blocks whose transactions were applied to the UTXO state as a
 * part of a single ActivateBestChainStep call.
 */
struct ConnectTrace {
    std::vector<std::pair<CBlockIndex*, std::shared_ptr<const CBlock> > > blocksConnected;
};

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 *
 * The block is always added to connectTrace (either after loading from disk or by copying
 * pblock) - if that is not intended, care must be taken to remove the last entry in
 * blocksConnected in case of failure.
 */
bool static ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace)
{
    assert(pindexNew->pprev == chainActive.Tip());
    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    if (!pblock) {
        std::shared_ptr<CBlock> pblockNew = std::make_shared<CBlock>();
        connectTrace.blocksConnected.emplace_back(pindexNew, pblockNew);
        if (!ReadBlockFromDisk(*pblockNew, pindexNew, chainparams.GetConsensus()))
            return AbortNode(state, "Failed to read block");
    } else {
        connectTrace.blocksConnected.emplace_back(pindexNew, pblock);
    }
    const CBlock& blockConnecting = *connectTrace.blocksConnected.back().second;
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros();
    nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CCoinsViewCache view(pcoinsTip);
        bool rv = ConnectBlock(blockConnecting, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(blockConnecting, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip(): ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        nTime3 = GetTimeMicros();
        nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        bool flushed = view.Flush();
        assert(flushed);
    }
    int64_t nTime4 = GetTimeMicros();
    nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;
    int64_t nTime5 = GetTimeMicros();
    nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);
    // Remove conflicting transactions from the mempool.;
    mempool.removeForBlock(blockConnecting.vtx, pindexNew->nHeight);
    // Update chainActive & related variables.
    UpdateTip(pindexNew, chainparams);

    int64_t nTime6 = GetTimeMicros();
    nTimePostConnect += nTime6 - nTime5;
    nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

bool DisconnectBlocks(int blocks)
{
    LOCK(cs_main);

    CValidationState state;
    const CChainParams& chainparams = Params();

    LogPrintf("DisconnectBlocks -- Got command to replay %d blocks\n", blocks);
    for (int i = 0; i < blocks; i++) {
        if (!DisconnectTip(state, chainparams) || !state.IsValid()) {
            return false;
        }
    }

    return true;
}

void ReprocessBlocks(int nBlocks)
{
    LOCK(cs_main);

    std::map<uint256, int64_t>::iterator it = mapRejectedBlocks.begin();
    while (it != mapRejectedBlocks.end()) {
        //use a window twice as large as is usual for the nBlocks we want to reset
        if ((*it).second > GetTime() - (nBlocks * 60 * 5)) {
            BlockMap::iterator mi = mapBlockIndex.find((*it).first);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                CBlockIndex* pindex = (*mi).second;
                LogPrintf("ReprocessBlocks -- %s\n", (*it).first.ToString());

                ResetBlockFailureFlags(pindex);
            }
        }
        ++it;
    }

    DisconnectBlocks(nBlocks);

    CValidationState state;
    ActivateBestChain(state, Params());
}

/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain()
{
    do {
        CBlockIndex* pindexNew = NULL;

        // Find the best candidate header.
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex* pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);

            // Pruned nodes may have entries in setBlockIndexCandidates for
            // which block files have been deleted.  Remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
                // Candidate chain is not usable (either invalid or missing data)
                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex* pindexFailed = pindexNew;
                // Remove the entire chain from the set.
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
                        // If we're missing data, then add back to mapBlocksUnlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setBlockIndexCandidates again.
                        mapBlocksUnlinked.insert(std::make_pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while (true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates()
{
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace)
{
    AssertLockHeld(cs_main);
    const CBlockIndex* pindexOldTip = chainActive.Tip();
    const CBlockIndex* pindexFork = chainActive.FindFork(pindexMostWork);

    // Disconnect active blocks which are no longer in the best chain.
    bool fBlocksDisconnected = false;
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state, chainparams))
            return false;
        fBlocksDisconnected = true;
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex* pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH (CBlockIndex* pindexConnect, vpindexToConnect) {
            if (!ConnectTip(state, chainparams, pindexConnect, pindexConnect == pindexMostWork ? pblock : std::shared_ptr<const CBlock>(), connectTrace)) {
                if (state.IsInvalid()) {
                    // The block violates a consensus rule.
                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
                    // A system error occurred (disk space, database error, ...).
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                    // We're in a better position than we were. Return temporarily to release the lock.
                    fContinue = false;
                    break;
                }
            }
        }
    }

    if (fBlocksDisconnected) {
        mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
        LimitMempoolSize(mempool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);
    }
    mempool.check(pcoinsTip);

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

static void NotifyHeaderTip()
{
    bool fNotify = false;
    bool fInitialBlockDownload = false;
    static CBlockIndex* pindexHeaderOld = NULL;
    CBlockIndex* pindexHeader = NULL;
    {
        LOCK(cs_main);
        pindexHeader = pindexBestHeader;

        if (pindexHeader != pindexHeaderOld) {
            fNotify = true;
            fInitialBlockDownload = IsInitialBlockDownload();
            pindexHeaderOld = pindexHeader;
        }
    }
    // Send block tip changed notifications without cs_main
    if (fNotify) {
        uiInterface.NotifyHeaderTip(fInitialBlockDownload, pindexHeader);
        GetMainSignals().NotifyHeaderTip(pindexHeader, fInitialBlockDownload);
    }
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock)
{
    // Note that while we're often called here from ProcessNewBlock, this is
    // far from a guarantee. Things in the P2P/RPC will often end up calling
    // us in the middle of ProcessNewBlock - do not assume pblock is set
    // sanely for performance or correctness!
    CBlockIndex* pindexMostWork = NULL;
    CBlockIndex* pindexNewTip = NULL;
    do {
        boost::this_thread::interruption_point();
        if (ShutdownRequested())
            break;

        const CBlockIndex* pindexFork;
        ConnectTrace connectTrace;
        bool fInitialDownload;
        {
            LOCK(cs_main);
            { // TODO: Tempoarily ensure that mempool removals are notified before
                // connected transactions.  This shouldn't matter, but the abandoned
                // state of transactions in our wallet is currently cleared when we
                // receive another notification and there is a race condition where
                // notification of a connected conflict might cause an outside process
                // to abandon a transaction and then have it inadvertantly cleared by
                // the notification that the conflicted transaction was evicted.
                MemPoolConflictRemovalTracker mrt(mempool);
                CBlockIndex* pindexOldTip = chainActive.Tip();
                if (pindexMostWork == NULL) {
                    pindexMostWork = FindMostWorkChain();
                }

                // Whether we have anything to do at all.
                if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                    return true;

                bool fInvalidFound = false;
                std::shared_ptr<const CBlock> nullBlockPtr;
                if (!ActivateBestChainStep(state, chainparams, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : nullBlockPtr, fInvalidFound, connectTrace))
                    return false;

                if (fInvalidFound) {
                    // Wipe cache, we may need another branch now.
                    pindexMostWork = NULL;
                }
                pindexNewTip = chainActive.Tip();
                pindexFork = chainActive.FindFork(pindexOldTip);
                fInitialDownload = IsInitialBlockDownload();

                // throw all transactions though the signal-interface

            } // MemPoolConflictRemovalTracker destroyed and conflict evictions are notified

            // Transactions in the connnected block are notified
            for (const auto& pair : connectTrace.blocksConnected) {
                assert(pair.second);
                const CBlock& block = *(pair.second);
                for (unsigned int i = 0; i < block.vtx.size(); i++)
                    GetMainSignals().SyncTransaction(*block.vtx[i], pair.first, i);
            }
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main

        // Notify external listeners about the new tip.
        GetMainSignals().UpdatedBlockTip(pindexNewTip, pindexFork, fInitialDownload);

        // Always notify the UI if a new block tip was connected
        if (pindexFork != pindexNewTip) {
            uiInterface.NotifyBlockTip(fInitialDownload, pindexNewTip);
        }
    } while (pindexNewTip != pindexMostWork);
    CheckBlockIndex(chainparams.GetConsensus());

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex* pindex)
{
    {
        LOCK(cs_main);
        if (pindex->nChainWork < chainActive.Tip()->nChainWork) {
            // Nothing to do, this block is not at the tip.
            return true;
        }
        if (chainActive.Tip()->nChainWork > nLastPreciousChainwork) {
            // The chain has been extended since the last call, reset the counter.
            nBlockReverseSequenceId = -1;
        }
        nLastPreciousChainwork = chainActive.Tip()->nChainWork;
        setBlockIndexCandidates.erase(pindex);
        pindex->nSequenceId = nBlockReverseSequenceId;
        if (nBlockReverseSequenceId > std::numeric_limits<int32_t>::min()) {
            // We can't keep reducing the counter if somebody really wants to
            // call preciousblock 2**31-1 times on the same set of tips...
            nBlockReverseSequenceId--;
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && pindex->nChainTx) {
            setBlockIndexCandidates.insert(pindex);
            PruneBlockIndexCandidates();
        }
    }

    return ActivateBestChain(state, params);
}

bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    if (pindex == pindexBestHeader) {
        pindexBestInvalid = pindexBestHeader;
        pindexBestHeader = pindexBestHeader->pprev;
    }

    while (chainActive.Contains(pindex)) {
        CBlockIndex* pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state, chainparams)) {
            mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
            return false;
        }
        if (pindexWalk == pindexBestHeader) {
            pindexBestInvalid = pindexBestHeader;
            pindexBestHeader = pindexBestHeader->pprev;
        }
    }

    LimitMempoolSize(mempool, GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add it again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    mempool.removeForReorg(pcoinsTip, chainActive.Tip()->nHeight + 1, STANDARD_LOCKTIME_VERIFY_FLAGS);
    uiInterface.NotifyBlockTip(IsInitialBlockDownload(), pindex->pprev);
    return true;
}

bool ResetBlockFailureFlags(CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlockHeader& block)
{
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);
    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();
    }
    pindexNew->nTimeMax = (pindexNew->pprev ? std::max(pindexNew->pprev->nTimeMax, pindexNew->nTime) : pindexNew->nTime);
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock& block, CValidationState& state, CBlockIndex* pindexNew, const CDiskBlockPos& pos)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        std::deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex* pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState& state, CDiskBlockPos& pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    if ((int)nFile != nLastBlockFile) {
        if (!fKnown) {
            LogPrintf("Leaving block file %i: %s\n", nLastBlockFile, vinfoBlockFile[nLastBlockFile].ToString());
        }
        FlushBlockFile(!fKnown);
        nLastBlockFile = nFile;
    }

    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (fPruneMode)
                fCheckForPruning = true;
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE* file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            } else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (fPruneMode)
            fCheckForPruning = true;
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE* file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        } else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW)
{
    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(), block.nBits, consensusParams))
        return state.DoS(50, false, REJECT_INVALID, "high-hash", false, "proof of work failed");

    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW, bool fCheckMerkleRoot)
{
    // These are checks that are independent of context.

    if (block.fChecked)
        return true;

    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(block, state, consensusParams, fCheckPOW))
        return false;

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = BlockMerkleRoot(block, &mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("CheckBlock(): hashMerkleRoot mismatch"),
                REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("CheckBlock(): duplicate transaction"),
                REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // Size limits
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-length", false, "size limits failed");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false, "first tx is not coinbase");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i]->IsCoinBase())
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-multiple", false, "more than one coinbase");


    // CASH : CHECK TRANSACTIONS FOR INSTANTSEND

    if (sporkManager.IsSporkActive(SPORK_3_INSTANTSEND_BLOCK_FILTERING)) {
        // We should never accept block which conflicts with completed transaction lock,
        // that's why this is in CheckBlock unlike coinbase payee/amount.
        // Require other nodes to comply, send them some data in case they are missing it.
        for (const auto& tx : block.vtx) {
            // skip coinbase, it has no inputs
            if (tx->IsCoinBase())
                continue;
            // LOOK FOR TRANSACTION LOCK IN OUR MAP OF OUTPOINTS
            for (const auto& txin : tx->vin) {
                uint256 hashLocked;
                if (instantsend.GetLockedOutPointTxHash(txin.prevout, hashLocked) && hashLocked != tx->GetHash()) {
                    // The node which relayed this will have to swtich later,
                    // relaying instantsend data won't help it.
                    LOCK(cs_main);
                    mapRejectedBlocks.insert(std::make_pair(block.GetHash(), GetTime()));
                    return state.DoS(0, error("CheckBlock(0DYNC): transaction %s conflicts with transaction lock %s", tx->GetHash().ToString(), hashLocked.ToString()),
                        REJECT_INVALID, "conflict-tx-lock");
                }
            }
        }
    } else {
        LogPrintf("CheckBlock(0DYNC): spork is off, skipping transaction locking checks\n");
    }

    // END CASH

    // Check transactions
    for (const auto& tx : block.vtx) {
        if (!CheckTransaction(*tx, state))
            return state.Invalid(false, state.GetRejectCode(), state.GetRejectReason(),
                strprintf("Transaction check failed (tx hash %s) %s", tx->GetHash().ToString(), state.GetDebugMessage()));

        for (const auto& txout : tx->vout) {
            if (IsTransactionFluid(txout.scriptPubKey)) {
                std::string strErrorMessage;
                if (!fluid.CheckFluidOperationScript(txout.scriptPubKey, block.nTime, strErrorMessage)) {
                    return error("CheckBlock(): %s, Block %s failed with %s",
                        strErrorMessage,
                        tx->GetHash().ToString(),
                        FormatStateMessage(state));
                }
            }
        }
        if (!fluid.CheckTransactionToBlock(*tx, block))
            return error("CheckBlock(): Fluid transaction violated filtration rules, offender %s", tx->GetHash().ToString());
    }

    unsigned int nSigOps = 0;
    for (const auto& tx : block.vtx) {
        nSigOps += GetLegacySigOpCount(*tx);
    }
    // sigops limits (relaxed)
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return state.DoS(100, false, REJECT_INVALID, "bad-blk-sigops", false, "out-of-bounds SigOpCount");

    if (fCheckPOW && fCheckMerkleRoot)
        block.fChecked = true;

    return true;
}

static bool CheckIndexAgainstCheckpoint(const CBlockIndex* pindexPrev, CValidationState& state, const CChainParams& chainparams, const uint256& hash)
{
    if (*pindexPrev->phashBlock == chainparams.GetConsensus().hashGenesisBlock)
        return true;

    int nHeight = pindexPrev->nHeight + 1;
    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(chainparams.Checkpoints());
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s: forked chain older than last checkpoint (height %d)", __func__, nHeight));

    return true;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, int64_t nAdjustedTime)
{
    int nHeight = (pindexPrev->nHeight + 1);
    uint256 hash = block.GetHash();

    if (hash == Params().GetConsensus().hashGenesisBlock)
        return true;

    if (block.nBits != GetNextWorkRequired(pindexPrev, block, consensusParams)) {
        return state.DoS(100, error("%s : incorrect proof of work at %d", __func__, nHeight),
            REJECT_INVALID, "bad-diffbits");
    }

    // Check timestamp against prev
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.Invalid(error("%s: block's timestamp is too early", __func__),
            REJECT_INVALID, "time-too-old");

    // Check timestamp
    if (block.GetBlockTime() > nAdjustedTime + MAX_FUTURE_BLOCK_TIME)
        return state.Invalid(false, REJECT_INVALID, "time-too-new", "block timestamp too far in the future");

    return true;
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;

    // Start enforcing BIP113 (Median Time Past) using versionbits logic.
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        nLockTimeFlags |= LOCKTIME_MEDIAN_TIME_PAST;
    }

    int64_t nLockTimeCutoff = (nLockTimeFlags & LOCKTIME_MEDIAN_TIME_PAST) ? pindexPrev->GetMedianTimePast() : block.GetBlockTime();

    // Check that all transactions are finalized and not over-sized
    // Also count sigops
    for (const auto& tx : block.vtx) {
        if (pindexPrev != NULL) {
            if (!fluid.CheckTransactionToBlock(*tx, pindexPrev->GetBlockHeader()))
                return state.DoS(10, error("%s: contains an invalid fluid transaction", __func__), REJECT_INVALID, "invalid-fluid-txns");
        }
        if (!IsFinalTx(*tx, nHeight, nLockTimeCutoff)) {
            return state.DoS(10, error("%s: contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }
    }

    // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height
    // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
    if (block.nVersion >= 2 && IsSuperMajority(2, pindexPrev, consensusParams.nMajorityEnforceBlockUpgrade, consensusParams)) {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0]->vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0]->vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s: block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }

    // If Fluid transaction present, has it been adhered to?
    CDebitAddress mintAddress;
    CAmount fluidIssuance;

    if (fluid.GetMintingInstructions(pindexPrev, mintAddress, fluidIssuance)) {
        bool found = false;

        CScript script;
        assert(mintAddress.IsValid());
        if (!mintAddress.IsScript()) {
            script = GetScriptForDestination(mintAddress.Get());
        } else {
            CScriptID scriptID = boost::get<CScriptID>(mintAddress.Get());
            script = CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
        }

        for (const CTxOut& output : block.vtx[0]->vout) {
            if (output.scriptPubKey == script) {
                if (output.nValue == fluidIssuance) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            return state.DoS(100, error("%s: fluid issuance not complied to", __func__), REJECT_INVALID, "cb-no-fluid-mint");
        }
    }

    return true;
}

static bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex)
{
    AssertLockHeld(cs_main);
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex* pindex = NULL;

    // TODO : ENABLE BLOCK CACHE IN SPECIFIC CASES
    if (hash != chainparams.GetConsensus().hashGenesisBlock) {
        if (miSelf != mapBlockIndex.end() && !miSelf->second) {
            mapBlockIndex.erase(hash);
        }
        if (miSelf != mapBlockIndex.end() && miSelf->second) {
            // Block header is already known.
            pindex = miSelf->second;
            if (ppindex)
                *ppindex = pindex;
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return state.Invalid(error("%s: block %s is marked invalid", __func__, hash.ToString()), 0, "duplicate");
            return true;
        }

        if (!CheckBlockHeader(block, state, chainparams.GetConsensus()))
            return error("%s: Consensus::CheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));

        // Get prev block index
        CBlockIndex* pindexPrev = NULL;
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(10, error("%s: prev block not found", __func__), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(100, error("%s: prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");

        assert(pindexPrev);
        if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, hash))
            return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, state.GetRejectReason().c_str());

        if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
            return error("%s: Consensus::ContextualCheckBlockHeader: %s, %s", __func__, hash.ToString(), FormatStateMessage(state));
    }
    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    CheckBlockIndex(chainparams.GetConsensus());

    // Notify external listeners about accepted block header
    GetMainSignals().AcceptedBlockHeader(pindex);

    return true;
}

// Exposed wrapper for AcceptBlockHeader
bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& headers, CValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex)
{
    {
        LOCK(cs_main);
        for (const CBlockHeader& header : headers) {
            CBlockIndex* pindex = NULL; // Use a temp pindex instead of ppindex to avoid a const_cast
            if (!AcceptBlockHeader(header, state, chainparams, &pindex)) {
                return false;
            }
            if (ppindex) {
                *ppindex = pindex;
            }
        }
    }
    NotifyHeaderTip();
    return true;
}

/** Store block on disk. If dbp is non-NULL, the file is known to already reside on disk */
static bool AcceptBlock(const std::shared_ptr<const CBlock>& pblock, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock)
{
    const CBlock& block = *pblock;

    if (fNewBlock)
        *fNewBlock = false;
    AssertLockHeld(cs_main);

    CBlockIndex* pindexDummy = NULL;
    CBlockIndex*& pindex = ppindex ? *ppindex : pindexDummy;

    if (!AcceptBlockHeader(block, state, chainparams, &pindex))
        return false;

    // Try to process all requested blocks that we don't have, but only
    // process an unrequested block if it's new and has enough work to
    // advance our tip, and isn't too many blocks ahead.
    bool fAlreadyHave = pindex->nStatus & BLOCK_HAVE_DATA;
    bool fHasMoreWork = (chainActive.Tip() ? pindex->nChainWork > chainActive.Tip()->nChainWork : true);
    // Blocks that are too out-of-order needlessly limit the effectiveness of
    // pruning, because pruning will not delete block files that contain any
    // blocks which are too close in height to the tip.  Apply this test
    // regardless of whether pruning is enabled; it should generally be safe to
    // not process unrequested blocks.
    bool fTooFarAhead = (pindex->nHeight > int(chainActive.Height() + MIN_BLOCKS_TO_KEEP));

    // TODO: deal better with return value and error conditions for duplicate
    // and unrequested blocks.
    if (fAlreadyHave)
        return true;
    if (!fRequested) { // If we didn't ask for it:
        if (pindex->nTx != 0)
            return true; // This is a previously-processed block that was pruned
        if (!fHasMoreWork)
            return true; // Don't process less-work chains
        if (fTooFarAhead)
            return true; // Block height is too high
    }
    if (fNewBlock)
        *fNewBlock = true;

    if (!CheckBlock(block, state, chainparams.GetConsensus()) ||
        !ContextualCheckBlock(block, state, chainparams.GetConsensus(), pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return error("%s: %s", __func__, FormatStateMessage(state));
    }

    // Header is valid/has work, merkle tree is good...RELAY NOW
    // (but if it does not build on our best tip, let the SendMessages loop relay it)
    if (!IsInitialBlockDownload() && chainActive.Tip() == pindex->pprev)
        GetMainSignals().NewPoWValidBlock(pindex, pblock);

    int nHeight = pindex->nHeight;

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize + 8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock(): FindBlockPos failed");
        if (dbp == NULL)
            if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
                AbortNode(state, "Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock(): ReceivedBlockTransactions failed");
    } catch (const std::runtime_error& e) {
        return AbortNode(state, std::string("System error: ") + e.what());
    }

    if (fCheckForPruning)
        FlushStateToDisk(state, FLUSH_STATE_NONE); // we just allocated more disk space for block files

    return true;
}

static bool IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned nRequired, const Consensus::Params& consensusParams)
{
    unsigned int nFound = 0;
    for (int i = 0; i < consensusParams.nMajorityWindow && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool* fNewBlock)
{
    {
        CBlockIndex* pindex = NULL;
        if (fNewBlock)
            *fNewBlock = false;
        CValidationState state;
        // Ensure that CheckBlock() passes before calling AcceptBlock, as
        // belt-and-suspenders.
        bool ret = CheckBlock(*pblock, state, chainparams.GetConsensus());

        LOCK(cs_main);

        if (ret) {
            // Store to disk
            ret = AcceptBlock(pblock, state, chainparams, &pindex, fForceProcessing, NULL, fNewBlock);
        }
        CheckBlockIndex(chainparams.GetConsensus());
        if (!ret) {
            GetMainSignals().BlockChecked(*pblock, state);
            return error("%s: AcceptBlock FAILED", __func__);
        }
    }

    NotifyHeaderTip();

    CValidationState state; // Only used to report errors, not invalidity - ignore it
    if (!ActivateBestChain(state, chainparams, pblock))
        return error("%s: ActivateBestChain failed", __func__);

    LogPrint("validation", "%s : ACCEPTED\n", __func__);
    return true;
}

bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev && pindexPrev == chainActive.Tip());
    if (fCheckpointsEnabled && !CheckIndexAgainstCheckpoint(pindexPrev, state, chainparams, block.GetHash()))
        return error("%s: CheckIndexAgainstCheckpoint(): %s", __func__, state.GetRejectReason().c_str());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev, GetAdjustedTime()))
        return error("%s: Consensus::ContextualCheckBlockHeader: %s", __func__, FormatStateMessage(state));
    if (!CheckBlock(block, state, chainparams.GetConsensus(), fCheckPOW, fCheckMerkleRoot))
        return error("%s: Consensus::CheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ContextualCheckBlock(block, state, chainparams.GetConsensus(), pindexPrev))
        return error("%s: Consensus::ContextualCheckBlock: %s", __func__, FormatStateMessage(state));
    if (!ConnectBlock(block, state, &indexDummy, viewNew, chainparams, true))
        return false;
    assert(state.IsValid());

    return true;
}

/**
 * BLOCK PRUNING CODE
 */

/* Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    BOOST_FOREACH (const CBlockFileInfo& file, vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/* Prune a block file (modify associated database entries)*/
void PruneOneBlockFile(const int fileNumber)
{
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it) {
        CBlockIndex* pindex = it->second;
        if (pindex->nFile == fileNumber) {
            pindex->nStatus &= ~BLOCK_HAVE_DATA;
            pindex->nStatus &= ~BLOCK_HAVE_UNDO;
            pindex->nFile = 0;
            pindex->nDataPos = 0;
            pindex->nUndoPos = 0;
            setDirtyBlockIndex.insert(pindex);

            // Prune from mapBlocksUnlinked -- any block we prune would have
            // to be downloaded again in order to consider its chain, at which
            // point it would be considered as a candidate for
            // mapBlocksUnlinked or setBlockIndexCandidates.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator _it = range.first;
                range.first++;
                if (_it->second == pindex) {
                    mapBlocksUnlinked.erase(_it);
                }
            }
        }
    }

    vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
}


void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune)
{
    for (std::set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        CDiskBlockPos pos(*it, 0);
        boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
        boost::filesystem::remove(GetBlockPosFilename(pos, "rev"));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/* Calculate the block/rev files to delete based on height specified by user with RPC command pruneblockchain */
void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight)
{
    assert(fPruneMode && nManualPruneHeight > 0);

    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL)
        return;

    // last block to prune is the lesser of (user-specified height, MIN_BLOCKS_TO_KEEP from the tip)
    unsigned int nLastBlockWeCanPrune = std::min((unsigned)nManualPruneHeight, chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP);
    int count = 0;
    for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
        if (vinfoBlockFile[fileNumber].nSize == 0 || vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
            continue;
        PruneOneBlockFile(fileNumber);
        setFilesToPrune.insert(fileNumber);
        count++;
    }
    LogPrintf("Prune (Manual): prune_height=%d removed %d blk/rev pairs\n", nLastBlockWeCanPrune, count);
}

/* This function is called from the RPC code for pruneblockchain */
void PruneBlockFilesManual(int nManualPruneHeight)
{
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_NONE, nManualPruneHeight);
}

/* Calculate the block/rev files that should be deleted to remain under target*/
void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL || nPruneTarget == 0) {
        return;
    }
    if ((uint64_t)chainActive.Tip()->nHeight <= nPruneAfterHeight) {
        return;
    }

    unsigned int nLastBlockWeCanPrune = chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP;
    uint64_t nCurrentUsage = CalculateCurrentUsage();
    // We don't check to prune until after we've allocated new space for files
    // So we should leave a buffer under our target to account for another allocation
    // before the next pruning.
    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count = 0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

            if (nCurrentUsage + nBuffer < nPruneTarget) // are we below our target?
                break;

            // don't prune files that could have a block within MIN_BLOCKS_TO_KEEP of the main chain's tip but keep scanning
            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
                continue;

            PruneOneBlockFile(fileNumber);
            // Queue up the files for removal
            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint("prune", "Prune: target=%dMiB actual=%dMiB diff=%dMiB max_prune_height=%d removed %d blk/rev pairs\n",
        nPruneTarget / 1024 / 1024, nCurrentUsage / 1024 / 1024,
        ((int64_t)nPruneTarget - (int64_t)nCurrentUsage) / 1024 / 1024,
        nLastBlockWeCanPrune, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = boost::filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos& pos, const char* prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos& pos, const char* prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash.IsNull())
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw std::runtime_error("LoadBlockIndex(): new CBlockIndex failed");
    mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB(const CChainParams& chainparams)
{
    if (!pblocktree->LoadBlockIndexGuts(InsertBlockIndex))
        return false;

    boost::this_thread::interruption_point();

    // Calculate nChainWork
    std::vector<std::pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH (const PAIRTYPE(uint256, CBlockIndex*) & item, mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(std::make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH (const PAIRTYPE(int, CBlockIndex*) & item, vSortedByHeight) {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        pindex->nTimeMax = (pindex->pprev ? std::max(pindex->pprev->nTimeMax, pindex->nTime) : pindex->nTime);
        // We can link the chain of blocks for which we've received transactions at some point.
        // Pruned nodes may have deleted the block.
        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    std::set<int> setBlkDataFiles;
    BOOST_FOREACH (const PAIRTYPE(uint256, CBlockIndex*) & item, mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++) {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    // Check whether we have ever pruned block & undo files
    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s: transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");

    // Check whether we have an address index
    pblocktree->ReadFlag("addressindex", fAddressIndex);
    LogPrintf("%s: address index %s\n", __func__, fAddressIndex ? "enabled" : "disabled");

    // Check whether we have a timestamp index
    pblocktree->ReadFlag("timestampindex", fTimestampIndex);
    LogPrintf("%s: timestamp index %s\n", __func__, fTimestampIndex ? "enabled" : "disabled");

    // Check whether we have a spent index
    pblocktree->ReadFlag("spentindex", fSpentIndex);
    LogPrintf("%s: spent index %s\n", __func__, fSpentIndex ? "enabled" : "disabled");

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();

    LogPrintf("%s: hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        GuessVerificationProgress(chainparams.TxData(), chainActive.Tip()));

    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(const CChainParams& chainparams, CCoinsView* coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    int reportDone = 0;
    LogPrintf("[0%%]...");
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev) {
        boost::this_thread::interruption_point();
        int percentageDone = std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100))));
        if (reportDone < percentageDone / 10) {
            // report every 10% step
            LogPrintf("[%d%%]...", percentageDone);
            reportDone = percentageDone / 10;
        }
        uiInterface.ShowProgress(_("Verifying blocks..."), percentageDone);
        if (pindex->nHeight < chainActive.Height() - nCheckDepth)
            break;
        if (fPruneMode && !(pindex->nStatus & BLOCK_HAVE_DATA)) {
            // If pruning, only go back as far as we have data.
            LogPrintf("VerifyDB(): block verification stopping at height %d (pruning, no data)\n", pindex->nHeight);
            break;
        }
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
            return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state, chainparams.GetConsensus()))
            return error("%s: *** found bad block at %d, hash=%s (%s)\n", __func__,
                pindex->nHeight, pindex->GetBlockHash().ToString(), FormatStateMessage(state));
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!UndoReadFromDisk(undo, pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB(): *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage) {
            DisconnectResult res = DisconnectBlock(block, state, pindex, coins, nCheckLevel);
            if (res == DISCONNECT_FAILED) {
                return error("VerifyDB(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
            pindexState = pindex->pprev;
            if (res == DISCONNECT_UNCLEAN) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else {
                nGoodTransactions += block.vtx.size();
            }
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex* pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
                return error("VerifyDB(): *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins, chainparams))
                return error("VerifyDB(): *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        }
    }

    LogPrintf("[DONE].\n");
    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

void UnloadBlockIndex()
{
    LOCK(cs_main);
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    mempool.clear();
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    versionbitscache.Clear();
    for (int b = 0; b < VERSIONBITS_NUM_BITS; b++) {
        warningcache[b].clear();
    }

    BOOST_FOREACH (BlockMap::value_type& entry, mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;
}

bool LoadBlockIndex(const CChainParams& chainparams)
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB(chainparams))
        return false;
    return true;
}

static bool AddGenesisBlock(const CChainParams& chainparams, const CBlock& block, CValidationState& state)
{
    // Start new block file
    unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
    CDiskBlockPos blockPos;
    if (!FindBlockPos(state, blockPos, nBlockSize + 8, 0, block.GetBlockTime()))
        return error("%s: FindBlockPos failed", __func__);
    if (!WriteBlockToDisk(block, blockPos, chainparams.MessageStart()))
        return error("%s: writing genesis block to disk failed", __func__);
    CBlockIndex* pindex = AddToBlockIndex(block);
    if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
        return error("%s: genesis block not accepted", __func__);
    return true;
}


bool InitBlockIndex(const CChainParams& chainparams)
{
    LOCK(cs_main);

    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
    fTxIndex = GetBoolArg("-txindex", DEFAULT_TXINDEX);
    pblocktree->WriteFlag("txindex", fTxIndex);

    // Use the provided setting for -addressindex in the new database
    fAddressIndex = GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX);
    pblocktree->WriteFlag("addressindex", fAddressIndex);

    // Use the provided setting for -timestampindex in the new database
    fTimestampIndex = GetBoolArg("-timestampindex", DEFAULT_TIMESTAMPINDEX);
    pblocktree->WriteFlag("timestampindex", fTimestampIndex);

    fSpentIndex = GetBoolArg("-spentindex", DEFAULT_SPENTINDEX);
    pblocktree->WriteFlag("spentindex", fSpentIndex);

    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CValidationState state;

            if (!AddGenesisBlock(chainparams, chainparams.GenesisBlock(), state))
                return false;

            // Force a chainstate write so that when we VerifyDB in a moment, it doesn't check stale data
            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        } catch (const std::runtime_error& e) {
            return error("%s: failed to initialize block database: %s", __func__, e.what());
        }
    }

    return true;
}

bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos* dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2 * MAX_BLOCK_SIZE, MAX_BLOCK_SIZE + 8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++;         // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[CMessageHeader::MESSAGE_START_SIZE];
                blkdat.FindByte(chainparams.MessageStart()[0]);
                nRewind = blkdat.GetPos() + 1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, chainparams.MessageStart(), CMessageHeader::MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SIZE)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
                CBlock& block = *pblock;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHash();
                if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                        block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    LOCK(cs_main);
                    CValidationState state;
                    if (AcceptBlock(pblock, state, chainparams, NULL, true, dbp, NULL))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrint("reindex", "Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                // Activate the genesis block so normal node progress can continue
                if (hash == chainparams.GetConsensus().hashGenesisBlock) {
                    CValidationState state;
                    if (!ActivateBestChain(state, chainparams)) {
                        break;
                    }
                }

                NotifyHeaderTip();

                // Recursively process earlier encountered successors of this block
                std::deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        std::shared_ptr<CBlock> pblockrecursive = std::make_shared<CBlock>();
                        if (ReadBlockFromDisk(*pblockrecursive, it->second, chainparams.GetConsensus())) {
                            LogPrint("reindex", "%s: Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                head.ToString());
                            LOCK(cs_main);
                            CValidationState dummy;
                            if (AcceptBlock(pblockrecursive, dummy, chainparams, NULL, true, &it->second, NULL)) {
                                nLoaded++;
                                queue.push_back(pblockrecursive->GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                        NotifyHeaderTip();
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s: Deserialize or I/O error - %s\n", __func__, e.what());
            }
        }
    } catch (const std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void static CheckBlockIndex(const Consensus::Params& consensusParams)
{
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

    // During a reindex, we read the genesis block and call CheckBlockIndex before ActivateBestChain,
    // so we have the genesis block in mapBlockIndex but no active chain.  (A few of the tests when
    // iterating the block tree require that chainActive has been initialized.)
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    // Build forward-pointing map of the entire block tree.
    std::multimap<CBlockIndex*, CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(std::make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex* pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL;              // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL;              // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNeverProcessed = NULL;       // Oldest ancestor of pindex for which nTx == 0.
    CBlockIndex* pindexFirstNotTreeValid = NULL;         // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotTransactionsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TRANSACTIONS (regardless of being valid or not).
    CBlockIndex* pindexFirstNotChainValid = NULL;        // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL;      // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID)
            pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA))
            pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == NULL && pindex->nTx == 0)
            pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE)
            pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTransactionsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS)
            pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN)
            pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS)
            pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHash() == consensusParams.hashGenesisBlock); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis());                            // The current active chain's genesis block must be this block.
        }
        if (pindex->nChainTx == 0)
            assert(pindex->nSequenceId <= 0); // nSequenceId can't be set positive for blocks that aren't linked (negative is used for preciousblock)
        // VALID_TRANSACTIONS is equivalent to nTx > 0 for all nodes (whether or not pruning has occurred).
        // HAVE_DATA is only equivalent to nTx > 0 (or VALID_TRANSACTIONS) if no pruning has occurred.
        if (!fHavePruned) {
            // If we've never pruned, then HAVE_DATA should be equivalent to nTx > 0
            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {
            // If we have pruned, then we can only say that HAVE_DATA implies nTx > 0
            if (pindex->nStatus & BLOCK_HAVE_DATA)
                assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO)
            assert(pindex->nStatus & BLOCK_HAVE_DATA);
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); // This is pruning-independent.
        // All parents having had data (at some point) is equivalent to all parents being VALID_TRANSACTIONS, which is equivalent to nChainTx being set.
        assert((pindexFirstNeverProcessed != NULL) == (pindex->nChainTx == 0)); // nChainTx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexFirstNotTransactionsValid != NULL) == (pindex->nChainTx == 0));
        assert(pindex->nHeight == nHeight);                                               // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork); // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight)));     // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL);                                          // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE)
            assert(pindexFirstNotTreeValid == NULL); // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN)
            assert(pindexFirstNotChainValid == NULL); // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS)
            assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == NULL) {
            if (pindexFirstInvalid == NULL) {
                // If this block sorts at least as good as the current tip and
                // is valid and we have all data for its parents, it must be in
                // setBlockIndexCandidates.  chainActive.Tip() must also be there
                // even if some data has been pruned.
                if (pindexFirstMissing == NULL || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
                // If some parent is missing, then it could be that this block was in
                // setBlockIndexCandidates but had to be removed because of the missing data.
                // In this case it must be in mapBlocksUnlinked -- see test below.
            }
        } else { // If this block sorts worse than the current tip or some ancestor's block has never been seen, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed != NULL && pindexFirstInvalid == NULL) {
            // If this block has block data available, some parent was never received, and has no invalid parents, it must be in mapBlocksUnlinked.
            assert(foundInUnlinked);
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA))
            assert(!foundInUnlinked); // Can't be in mapBlocksUnlinked if we don't HAVE_DATA
        if (pindexFirstMissing == NULL)
            assert(!foundInUnlinked); // We aren't missing data for any parent -- cannot be in mapBlocksUnlinked.
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == NULL && pindexFirstMissing != NULL) {
            // We HAVE_DATA for this block, have received data for all parents at some point, but we're currently missing data for some parent.
            assert(fHavePruned); // We must have pruned.
            // This block may have entered mapBlocksUnlinked if:
            //  - it has a descendant that at some point had more work than the
            //    tip, and
            //  - we tried switching to that descendant but were missing
            //    data for some intermediate block between chainActive and the
            //    tip.
            // So if this block is itself better than chainActive.Tip() and it wasn't in
            // setBlockIndexCandidates, then it must be in mapBlocksUnlinked.
            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == NULL) {
                    assert(foundInUnlinked);
                }
            }
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid)
                pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing)
                pindexFirstMissing = NULL;
            if (pindex == pindexFirstNeverProcessed)
                pindexFirstNeverProcessed = NULL;
            if (pindex == pindexFirstNotTreeValid)
                pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotTransactionsValid)
                pindexFirstNotTransactionsValid = NULL;
            if (pindex == pindexFirstNotChainValid)
                pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid)
                pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

std::string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}

CBlockFileInfo* GetBlockFileInfo(size_t n)
{
    return &vinfoBlockFile.at(n);
}

ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    AssertLockHeld(cs_main);
    return VersionBitsState(chainActive.Tip(), params, pos, versionbitscache);
}

int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsStateSinceHeight(chainActive.Tip(), params, pos, versionbitscache);
}

static const uint64_t MEMPOOL_DUMP_VERSION = 1;

bool LoadMempool(void)
{
    int64_t nExpiryTimeout = GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60;
    FILE* filestr = fopen((GetDataDir() / "mempool.dat").string().c_str(), "r");
    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("Failed to open mempool file from disk. Continuing anyway.\n");
        return false;
    }

    int64_t count = 0;
    int64_t skipped = 0;
    int64_t failed = 0;
    int64_t nNow = GetTime();

    try {
        uint64_t version;
        file >> version;
        if (version != MEMPOOL_DUMP_VERSION) {
            return false;
        }
        uint64_t num;
        file >> num;
        double prioritydummy = 0;
        while (num--) {
            CTransactionRef tx;
            int64_t nTime;
            int64_t nFeeDelta;
            file >> tx;
            file >> nTime;
            file >> nFeeDelta;

            CAmount amountdelta = nFeeDelta;
            if (amountdelta) {
                mempool.PrioritiseTransaction(tx->GetHash(), tx->GetHash().ToString(), prioritydummy, amountdelta);
            }
            CValidationState state;
            if (nTime + nExpiryTimeout > nNow) {
                LOCK(cs_main);
                AcceptToMemoryPoolWithTime(mempool, state, tx, true, NULL, nTime);
                if (state.IsValid()) {
                    ++count;
                } else {
                    ++failed;
                }
            } else {
                ++skipped;
            }
            if (ShutdownRequested())
                return false;
        }
        std::map<uint256, CAmount> mapDeltas;
        file >> mapDeltas;

        for (const auto& i : mapDeltas) {
            mempool.PrioritiseTransaction(i.first, i.first.ToString(), prioritydummy, i.second);
        }
    } catch (const std::exception& e) {
        LogPrintf("Failed to deserialize mempool data on disk: %s. Continuing anyway.\n", e.what());
        return false;
    }

    LogPrintf("Imported mempool transactions from disk: %i successes, %i failed, %i expired\n", count, failed, skipped);
    return true;
}

void DumpMempool(void)
{
    int64_t start = GetTimeMicros();

    std::map<uint256, CAmount> mapDeltas;
    std::vector<TxMempoolInfo> vinfo;

    {
        LOCK(mempool.cs);
        for (const auto& i : mempool.mapDeltas) {
            mapDeltas[i.first] = i.second.second;
        }
        vinfo = mempool.infoAll();
    }

    int64_t mid = GetTimeMicros();

    try {
        FILE* filestr = fopen((GetDataDir() / "mempool.dat.new").string().c_str(), "w");
        if (!filestr) {
            return;
        }

        CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);

        uint64_t version = MEMPOOL_DUMP_VERSION;
        file << version;

        file << (uint64_t)vinfo.size();
        for (const auto& i : vinfo) {
            file << *(i.tx);
            file << (int64_t)i.nTime;
            file << (int64_t)i.nFeeDelta;
            mapDeltas.erase(i.tx->GetHash());
        }

        file << mapDeltas;
        FileCommit(file.Get());
        file.fclose();
        RenameOver(GetDataDir() / "mempool.dat.new", GetDataDir() / "mempool.dat");
        int64_t last = GetTimeMicros();
        LogPrintf("Dumped mempool: %gs to copy, %gs to dump\n", (mid - start) * 0.000001, (last - mid) * 0.000001);
    } catch (const std::exception& e) {
        LogPrintf("Failed to dump mempool: %s. Continuing anyway.\n", e.what());
    }
}

//! Guess how far we are in the verification process at the given block index
double GuessVerificationProgress(const ChainTxData& data, CBlockIndex* pindex)
{
    if (pindex == NULL)
        return 0.0;

    int64_t nNow = time(NULL);

    double fTxTotal;

    if (pindex->nChainTx <= data.nTxCount) {
        fTxTotal = data.nTxCount + (nNow - data.nTime) * data.dTxRate;
    } else {
        fTxTotal = pindex->nChainTx + (nNow - pindex->GetBlockTime()) * data.dTxRate;
    }

    return pindex->nChainTx / fTxTotal;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup()
    {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();
    }
} instance_of_cmaincleanup;
