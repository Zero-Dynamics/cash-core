// Copyright (c) 2019 Duality Blockchain Solutions Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DYNAMIC_BDAP_LINKMANAGER_H
#define DYNAMIC_BDAP_LINKMANAGER_H

#include "bdap/linkstorage.h"
#include "uint256.h"

#include <array>
#include <map>
#include <queue>
#include <string>
#include <vector>

class CLinkRequest;
class CLinkAccept;

namespace BDAP {

    enum LinkState : std::uint8_t
    {
        unknown_state = 0,
        pending_state = 1,
        complete_state = 2,
        deleted_state = 3
    };
}

class CLink {
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    uint256 LinkID;
    bool fRequestFromMe;
    bool fAcceptFromMe;
    uint8_t nLinkState;
    std::vector<unsigned char> RequestorFullObjectPath; // Requestor's BDAP object path
    std::vector<unsigned char> RecipientFullObjectPath; // Recipient's BDAP object path
    std::vector<unsigned char> RequestorPubKey; // ed25519 public key new/unique for this link
    std::vector<unsigned char> RecipientPubKey; // ed25519 public key new/unique for this link
    std::vector<unsigned char> SharedRequestPubKey; // ed25519 shared public key. RequestorPubKey + Recipient's BDAP DHT PubKey
    std::vector<unsigned char> SharedLinkPubKey; // ed25519 shared public key. RecipientPubKey + RequestorPubKey's BDAP DHT PubKey
    std::vector<unsigned char> LinkMessage; // Link message to recipient

    uint64_t nHeightRequest;
    uint64_t nExpireTimeRequest;
    uint256 txHashRequest;

    uint64_t nHeightAccept;
    uint64_t nExpireTimeAccept;
    uint256 txHashAccept;

    CLink() {
        SetNull();
    }

    inline void SetNull()
    {
        nVersion = CLink::CURRENT_VERSION;
        LinkID.SetNull();
        nLinkState = 0;
        fRequestFromMe = false;
        fAcceptFromMe = false;
        RequestorFullObjectPath.clear();
        RecipientFullObjectPath.clear();
        RequestorPubKey.clear();
        RecipientPubKey.clear();
        SharedRequestPubKey.clear();
        SharedLinkPubKey.clear();
        LinkMessage.clear();
        nHeightRequest = 0;
        nExpireTimeRequest = 0;
        txHashRequest.SetNull();
        nHeightAccept = 0;
        nExpireTimeAccept = 0;
        txHashAccept.SetNull();
    }

    inline friend bool operator==(const CLink &a, const CLink &b) {
        return (a.txHashRequest == b.txHashRequest && a.txHashAccept == b.txHashAccept);
    }

    inline friend bool operator!=(const CLink &a, const CLink &b) {
        return !(a == b);
    }

    inline CLink operator=(const CLink &b) {
        nVersion = b.nVersion;
        LinkID = b.LinkID;
        nLinkState = b.nLinkState;
        fRequestFromMe = b.fRequestFromMe;
        fAcceptFromMe = b.fAcceptFromMe;
        RequestorFullObjectPath = b.RequestorFullObjectPath;
        RecipientFullObjectPath = b.RecipientFullObjectPath;
        RequestorPubKey = b.RequestorPubKey;
        RecipientPubKey = b.RecipientPubKey;
        SharedRequestPubKey = b.SharedRequestPubKey;
        SharedLinkPubKey = b.SharedLinkPubKey;
        LinkMessage = b.LinkMessage;
        nHeightRequest = b.nHeightRequest;
        nExpireTimeRequest = b.nExpireTimeRequest;
        txHashRequest = b.txHashRequest;
        nHeightAccept = b.nHeightAccept;
        nExpireTimeAccept = b.nExpireTimeAccept;
        txHashAccept = b.txHashAccept;
        return *this;
    }
 
    std::string RequestorFQDN() const;
    std::string RecipientFQDN() const;
    std::string RequestorPubKeyString() const;
    std::string RecipientPubKeyString() const;

    inline bool IsNull() const { return (nLinkState == 0 && RequestorFullObjectPath.empty() && RecipientFullObjectPath.empty()); }

    std::string LinkState() const;
    std::string ToString() const;
};

class CLinkManager {
private:
    std::queue<CLinkStorage> linkQueue;
    std::map<uint256, CLink> m_Links;

public:
    CLinkManager() {
        SetNull();
    }

    inline void SetNull()
    {
        std::queue<CLinkStorage> emptyQueue;
        linkQueue = emptyQueue;
        m_Links.clear();
    }

    std::size_t QueueSize() const { return linkQueue.size(); }
    std::size_t LinkCount() const { return m_Links.size(); }

    bool ProcessLink(const CLinkStorage& storage, const bool fStoreInQueueOnly = false);
    void ProcessQueue();

    bool FindLink(const uint256& id, CLink& link);
    bool ListMyPendingRequests(std::vector<CLink>& vchLinks);
    bool ListMyPendingAccepts(std::vector<CLink>& vchLinks);
    bool ListMyCompleted(std::vector<CLink>& vchLinks);

private:
    bool IsLinkFromMe(const std::vector<unsigned char>& vchLinkPubKey);
    bool IsLinkForMe(const std::vector<unsigned char>& vchLinkPubKey, const std::vector<unsigned char>& vchSharedPubKey);
    bool GetLinkPrivateKey(const std::vector<unsigned char>& vchSenderPubKey, const std::vector<unsigned char>& vchSharedPubKey, std::array<char, 32>& sharedSeed, std::string& strErrorMessage);
};

uint256 GetLinkID(const CLinkRequest& request);
uint256 GetLinkID(const CLinkAccept& accept);
uint256 GetLinkID(const std::string& account1, const std::string& account2);

extern CLinkManager* pLinkManager;

#endif // DYNAMIC_BDAP_LINKMANAGER_H