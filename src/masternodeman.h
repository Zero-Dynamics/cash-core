// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2017 The Dash Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_MASTERNODEMAN_H
#define CASH_MASTERNODEMAN_H

#include "masternode.h"
#include "sync.h"

class CMasternodeMan;
class CConnman;

extern CMasternodeMan mnodeman;

class CMasternodeMan
{
public:
    typedef std::pair<arith_uint256, const CMasternode*> score_pair_t;
    typedef std::vector<score_pair_t> score_pair_vec_t;
    typedef std::pair<int, const CMasternode> rank_pair_t;
    typedef std::vector<rank_pair_t> rank_pair_vec_t;

private:
    static const std::string SERIALIZATION_VERSION_STRING;

    static const int PSEG_UPDATE_SECONDS = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS;

    static const int MIN_POSE_PROTO_VERSION = 70600;
    static const int MAX_POSE_CONNECTIONS = 10;
    static const int MAX_POSE_RANK = 10;
    static const int MAX_POSE_BLOCKS = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED = 10;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS = 3 * 60 * 60;
    // the minimun active Masternodes before using InstandSend
    static const int INSTANTSEND_MIN_ACTIVE_MASTERNODE_COUNT = 25;
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block height
    int nCachedBlockHeight;

    // map to hold all MNs
    std::map<COutPoint, CMasternode> mapMasternodes;
    // who's asked for the Masternode list and the last time
    std::map<CService, int64_t> mAskedUsForMasternodeList;
    // who we asked for the Masternode list and the last time
    std::map<CService, int64_t> mWeAskedForMasternodeList;
    // which Masternodes we've asked for
    std::map<COutPoint, std::map<CService, int64_t> > mWeAskedForMasternodeListEntry;

    // who we asked for the masternode verification
    std::map<CService, CMasternodeVerification> mWeAskedForVerification;

    // these maps are used for Masternode recovery from MASTERNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair<int64_t, std::set<CService> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CMasternodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list<std::pair<CService, uint256> > listScheduledMnbRequestConnections;
    std::map<CService, std::pair<int64_t, std::set<uint256> > > mapPendingMNB;
    std::map<CService, std::pair<int64_t, CMasternodeVerification> > mapPendingMNV;
    CCriticalSection cs_mapPendingMNV;

    /// Set when Masternodes are added, cleared when CGovernanceManager is notified
    bool fMasternodesAdded;

    /// Set when Masternodes are removed, cleared when CGovernanceManager is notified
    bool fMasternodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastSentinelPingTime;

    friend class CMasternodeSync;
    /// Find an entry
    CMasternode* Find(const COutPoint& outpoint);

    bool GetMasternodeScores(const uint256& nBlockHash, score_pair_vec_t& vecMasternodeScoresRet, int nMinProtocol = 0);

    void SyncSingle(CNode* pnode, const COutPoint& outpoint, CConnman& connman);
    void SyncAll(CNode* pnode, CConnman& connman);

    void PushPsegInvs(CNode* pnode, const CMasternode& mn);

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CMasternodeBroadcast> > mapSeenMasternodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CMasternodePing> mapSeenMasternodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CMasternodeVerification> mapSeenMasternodeVerification;
    // keep track of psq count to prevent Masternodes from gaming privatesend queue
    int64_t nPsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        LOCK(cs);
        std::string strVersion;
        if (ser_action.ForRead()) {
            READWRITE(strVersion);
        } else {
            strVersion = SERIALIZATION_VERSION_STRING;
            READWRITE(strVersion);
        }

        READWRITE(mapMasternodes);
        READWRITE(mAskedUsForMasternodeList);
        READWRITE(mWeAskedForMasternodeList);
        READWRITE(mWeAskedForMasternodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastSentinelPingTime);
        READWRITE(nPsqCount);

        READWRITE(mapSeenMasternodeBroadcast);
        READWRITE(mapSeenMasternodePing);
        if (ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CMasternodeMan();

    /// Add an entry
    bool Add(CMasternode& mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode* pnode, const COutPoint& outpoint, CConnman& connman);
    void AskForMnb(CNode* pnode, const uint256& hash);

    bool PoSeBan(const COutPoint& outpoint);
    bool AllowMixing(const COutPoint& outpoint);
    bool DisallowMixing(const COutPoint& outpoint);

    /// Check all Masternodes
    void Check();

    /// Check all Masternode and remove inactive
    void CheckAndRemove(CConnman& connman);
    /// This is dummy overload to be used for dumping/loading mncache.dat
    void CheckAndRemove() {}

    /// Clear Masternode vector
    void Clear();

    /// Count Masternodes filtered by nProtocolVersion.
    /// Masternode nProtocolVersion should match or be above the one specified in param here.
    int CountMasternodes(int nProtocolVersion = -1);
    /// Count enabled Masternodes filtered by nProtocolVersion.
    /// Masternode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Masternodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void PsegUpdate(CNode* pnode, CConnman& connman);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const COutPoint& outpoint, CMasternode& masternodeRet);
    bool Has(const COutPoint& outpoint);

    bool GetMasternodeInfo(const COutPoint& outpoint, masternode_info_t& mnInfoRet);
    bool GetMasternodeInfo(const CPubKey& pubKeyMasternode, masternode_info_t& mnInfoRet);
    bool GetMasternodeInfo(const CScript& payee, masternode_info_t& mnInfoRet);

    /// Find an entry in the Masternode list that is next to be paid
    bool GetNextMasternodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCountRet, masternode_info_t& mnInfoRet);
    /// Same as above but use current block height
    bool GetNextMasternodeInQueueForPayment(bool fFilterSigTime, int& nCountRet, masternode_info_t& mnInfoRet);

    /// Find a random entry
    masternode_info_t FindRandomNotInVec(const std::vector<COutPoint>& vecToExclude, int nProtocolVersion = -1);

    std::map<COutPoint, CMasternode> GetFullMasternodeMap() { return mapMasternodes; }

    bool GetMasternodeRanks(rank_pair_vec_t& vecMasternodeRanksRet, int nBlockHeight = -1, int nMinProtocol = 0);
    bool GetMasternodeRank(const COutPoint& outpoint, int& nRankRet, int nBlockHeight = -1, int nMinProtocol = 0);

    void ProcessMasternodeConnections(CConnman& connman);
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();
    void ProcessPendingMnbRequests(CConnman& connman);

    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);

    void DoFullVerificationStep(CConnman& connman);
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<const CMasternode*>& vSortedByAddr, CConnman& connman);
    void ProcessPendingMnvRequests(CConnman& connman);
    void SendVerifyReply(CNode* pnode, CMasternodeVerification& mnv, CConnman& connman);
    void ProcessVerifyReply(CNode* pnode, CMasternodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CMasternodeVerification& mnv);

    /// Return the number of (unique) Masternodes
    int size() { return mapMasternodes.size(); }

    std::string ToString() const;

    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateMasternodeList(CNode* pfrom, CMasternodeBroadcast mnb, int& nDos, CConnman& connman);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid(const CBlockIndex* pindex);

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;
    }

    bool IsSentinelPingActive();
    void UpdateLastSentinelPingTime();
    bool AddGovernanceVote(const COutPoint& outpoint, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckMasternode(const CPubKey& pubKeyMasternode, bool fForce);

    bool IsMasternodePingedWithin(const COutPoint& outpoint, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetMasternodeLastPing(const COutPoint& outpoint, const CMasternodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex* pindex);

    void WarnMasternodeDaemonUpdates();

    /**
     * Called to notify CGovernanceManager that the Masternode index has been updated.
     * Must be called while not holding the CMasternodeMan::cs mutex
     */
    void NotifyMasternodeUpdates(CConnman& connman);

    void DoMaintenance(CConnman& connman);

    bool EnoughActiveForInstandSend() { return (CountEnabled() >= INSTANTSEND_MIN_ACTIVE_MASTERNODE_COUNT); }

};

#endif // CASH_MASTERNODEMAN_H
