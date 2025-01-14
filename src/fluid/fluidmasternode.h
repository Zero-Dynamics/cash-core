// Copyright (c) 2019-2021 Duality Blockchain Solutions Developers

#ifndef FLUID_MASTERNODE_H
#define FLUID_MASTERNODE_H

#include "amount.h"
#include "dbwrapper.h"
#include "serialize.h"

#include "sync.h"
#include "uint256.h"

class CScript;
class CTransaction;

class CFluidMasternode
{
public:
    static const int CURRENT_VERSION = 1;
    int nVersion;
    std::vector<unsigned char> FluidScript;
    CAmount MasternodeReward;
    int64_t nTimeStamp;
    std::vector<std::vector<unsigned char> > SovereignAddresses;
    uint256 txHash;
    unsigned int nHeight;

    CFluidMasternode()
    {
        SetNull();
    }

    CFluidMasternode(const CTransaction& tx)
    {
        SetNull();
        UnserializeFromTx(tx);
    }

    CFluidMasternode(const CScript& fluidScript)
    {
        SetNull();
        UnserializeFromScript(fluidScript);
    }

    inline void SetNull()
    {
        nVersion = CFluidMasternode::CURRENT_VERSION;
        FluidScript.clear();
        MasternodeReward = -1;
        nTimeStamp = 0;
        SovereignAddresses.clear();
        txHash.SetNull();
        nHeight = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(FluidScript);
        READWRITE(MasternodeReward);
        READWRITE(VARINT(nTimeStamp));
        READWRITE(SovereignAddresses);
        READWRITE(txHash);
        READWRITE(VARINT(nHeight));
    }

    inline friend bool operator==(const CFluidMasternode& a, const CFluidMasternode& b)
    {
        return (a.FluidScript == b.FluidScript && a.MasternodeReward == b.MasternodeReward && a.nTimeStamp == b.nTimeStamp);
    }

    inline friend bool operator!=(const CFluidMasternode& a, const CFluidMasternode& b)
    {
        return !(a == b);
    }

    friend bool operator<(const CFluidMasternode& a, const CFluidMasternode& b)
    {
        return (a.nTimeStamp < b.nTimeStamp);
    }

    friend bool operator>(const CFluidMasternode& a, const CFluidMasternode& b)
    {
        return (a.nTimeStamp > b.nTimeStamp);
    }

    inline CFluidMasternode operator=(const CFluidMasternode& b)
    {
        FluidScript = b.FluidScript;
        MasternodeReward = b.MasternodeReward;
        nTimeStamp = b.nTimeStamp;
        SovereignAddresses.clear(); //clear out previous entries
        for (const std::vector<unsigned char>& vchAddress : b.SovereignAddresses) {
            SovereignAddresses.push_back(vchAddress);
        }
        txHash = b.txHash;
        nHeight = b.nHeight;
        return *this;
    }

    inline bool IsNull() const { return (nTimeStamp == 0); }
    bool UnserializeFromTx(const CTransaction& tx);
    bool UnserializeFromScript(const CScript& fluidScript);
    void Serialize(std::vector<unsigned char>& vchData);
};

static CCriticalSection cs_fluid_masternode;

class CFluidMasternodeDB : public CDBWrapper
{
public:
    CFluidMasternodeDB(size_t nCacheSize, bool fMemory, bool fWipe, bool obfuscate);
    bool AddFluidMasternodeEntry(const CFluidMasternode& entry, const int op);
    bool GetLastFluidMasternodeRecord(CFluidMasternode& returnEntry, const int nHeight);
    bool GetAllFluidMasternodeRecords(std::vector<CFluidMasternode>& entries);
    bool IsEmpty();
    bool RecordExists(const std::vector<unsigned char>& vchFluidScript);
};

bool GetFluidMasternodeData(const CScript& scriptPubKey, CFluidMasternode& entry);
bool GetFluidMasternodeData(const CTransaction& tx, CFluidMasternode& entry, int& nOut);
bool CheckFluidMasternodeDB();

extern CFluidMasternodeDB* pFluidMasternodeDB;

#endif // FLUID_MASTERNODE_H
