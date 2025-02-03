// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2017 The Dash Core Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_MASTERNODE_PAYMENTS_H
#define CASH_MASTERNODE_PAYMENTS_H

#include "core_io.h"
#include "masternode.h"
#include "key.h"
#include "net_processing.h"
#include "util.h"
#include "utilstrencodings.h"

class CMasternodeBlockPayees;
class CMasternodePayments;
class CMasternodePaymentVote;

static const int MNPAYMENTS_SIGNATURES_REQUIRED = 10;
static const int MNPAYMENTS_SIGNATURES_TOTAL = 20;

//! minimum peer version that can receive and send masternode payment messages,
//  vote for masternode and be elected as a payment winner
// V1 - Last protocol version before update
// V2 - Newest protocol version
static const int MIN_MASTERNODE_PAYMENT_PROTO_VERSION_1 = 71150;
static const int MIN_MASTERNODE_PAYMENT_PROTO_VERSION_2 = 71200; // Only MasterNodes > v1.2.0.0 will get paid after Spork 10 activation

extern CCriticalSection cs_vecPayees;
extern CCriticalSection cs_mapMasternodeBlocks;
extern CCriticalSection cs_mapMasternodePayeeVotes;

extern CMasternodePayments mnpayments;

/// TODO: all 4 functions do not belong here really, they should be refactored/moved somewhere (main.cpp ?)
bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string& strErrorRet);
bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);
void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutMasternodeRet, std::vector<CTxOut>& voutSuperblockRet);
std::string GetRequiredPaymentsString(int nBlockHeight);

class CMasternodePayee
{
private:
    CScript scriptPubKey;
    std::vector<uint256> vecVoteHashes;

public:
    CMasternodePayee() : scriptPubKey(),
                     vecVoteHashes()
    {
    }

    CMasternodePayee(CScript payee, uint256 hashIn) : scriptPubKey(payee),
                                                  vecVoteHashes()
    {
        vecVoteHashes.push_back(hashIn);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(*(CScriptBase*)(&scriptPubKey));
        READWRITE(vecVoteHashes);
    }

    CScript GetPayee() const { return scriptPubKey; }

    void AddVoteHash(uint256 hashIn) { vecVoteHashes.push_back(hashIn); }
    std::vector<uint256> GetVoteHashes() const { return vecVoteHashes; }
    int GetVoteCount() const { return vecVoteHashes.size(); }
};

// Keep track of votes for payees from Masternodes
class CMasternodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CMasternodePayee> vecPayees;

    CMasternodeBlockPayees() : nBlockHeight(0),
                           vecPayees()
    {
    }
    CMasternodeBlockPayees(int nBlockHeightIn) : nBlockHeight(nBlockHeightIn),
                                             vecPayees()
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nBlockHeight);
        READWRITE(vecPayees);
    }

    void AddPayee(const CMasternodePaymentVote& vote);
    bool GetBestPayee(CScript& payeeRet) const;
    bool HasPayeeWithVotes(const CScript& payeeIn, int nVotesReq) const;

    bool IsTransactionValid(const CTransaction& txNew, int nHeight) const;

    std::string GetRequiredPaymentsString() const;
};

// vote for the winning payment
class CMasternodePaymentVote
{
public:
    COutPoint masternodeOutpoint;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CMasternodePaymentVote() : masternodeOutpoint(),
                           nBlockHeight(0),
                           payee(),
                           vchSig()
    {
    }

    CMasternodePaymentVote(COutPoint outpoint, int nBlockHeight, CScript payee) : masternodeOutpoint(outpoint),
                                                                              nBlockHeight(nBlockHeight),
                                                                              payee(payee),
                                                                              vchSig()
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = s.GetVersion();
        if (nVersion == 70900 && (s.GetType() & SER_NETWORK)) {
            // converting from/to old format
            CTxIn vinMasternode{};
            if (ser_action.ForRead()) {
                READWRITE(vinMasternode);
                masternodeOutpoint = vinMasternode.prevout;
            } else {
                vinMasternode = CTxIn(masternodeOutpoint);
                READWRITE(vinMasternode);
            }
        } else {
            // using new format directly
            READWRITE(masternodeOutpoint);
        }
        READWRITE(nBlockHeight);
        READWRITE(*(CScriptBase*)(&payee));
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

    uint256 GetHash() const;
    uint256 GetSignatureHash() const;

    bool Sign();
    bool CheckSignature(const CPubKey& pubKeyMasternode, int nValidationHeight, int& nDos) const;

    bool IsValid(CNode* pnode, int nValidationHeight, std::string& strError, CConnman& connman) const;
    void Relay(CConnman& connman) const;

    bool IsVerified() const { return !vchSig.empty(); }
    void MarkAsNotVerified() { vchSig.clear(); }

    std::string ToString() const;
};

//
// Masternode Payments Class
// Keeps track of who should get paid for which blocks
//

class CMasternodePayments
{
private:
    // Masternode count times nStorageCoeff payments blocks should be stored ...
    const float nStorageCoeff;
    // ... but at least nMinBlocksToStore (payments blocks)
    const int nMinBlocksToStore;

    // Keep track of current block height
    int nCachedBlockHeight;

public:
    std::map<uint256, CMasternodePaymentVote> mapMasternodePaymentVotes;
    std::map<int, CMasternodeBlockPayees> mapMasternodeBlocks;
    std::map<COutPoint, int> mapMasternodesLastVote;
    std::map<COutPoint, int> mapMasternodesDidNotVote;

    CMasternodePayments() : nStorageCoeff(1.25), nMinBlocksToStore(5000) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(mapMasternodePaymentVotes);
        READWRITE(mapMasternodeBlocks);
    }

    void Clear();

    bool AddOrUpdatePaymentVote(const CMasternodePaymentVote& vote);
    bool HasVerifiedPaymentVote(const uint256& hashIn) const;
    bool ProcessBlock(int nBlockHeight, CConnman& connman);
    void CheckBlockVotes(int nBlockHeight);

    void Sync(CNode* node, CConnman& connman) const;
    void RequestLowDataPaymentBlocks(CNode* pnode, CConnman& connman) const;
    void CheckAndRemove();

    bool GetBlockPayee(int nBlockHeight, CScript& payeeRet) const;
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight) const;
    bool IsScheduled(const masternode_info_t& mnInfo, int nNotBlockHeight) const;

    bool UpdateLastVote(const CMasternodePaymentVote& vote);

    int GetMinMasternodePaymentsProto() const;
    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman);
    std::string GetRequiredPaymentsString(int nBlockHeight) const;
    void FillBlockPayee(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutMasternodeRet) const;
    std::string ToString() const;

    int GetBlockCount() const { return mapMasternodeBlocks.size(); }
    int GetVoteCount() const { return mapMasternodePaymentVotes.size(); }

    bool IsEnoughData() const;
    int GetStorageLimit() const;

    void UpdatedBlockTip(const CBlockIndex* pindex, CConnman& connman);

    void DoMaintenance();
};

#endif // CASH_MASTERNODE_PAYMENTS_H
