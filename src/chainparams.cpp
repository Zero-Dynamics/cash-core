// Copyright (c) 2009-2021 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "arith_uint256.h"
#include "consensus/merkle.h"
#include "hash.h"
#include "streams.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include "uint256.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

const arith_uint256 maxUint = UintToArith256(uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, const uint32_t nTime, const uint32_t nNonce, const uint32_t nBits, const int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 1709845000 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

static void MineGenesis(CBlockHeader& genesisBlock, const uint256& powLimit, bool noProduction)
{
    if (noProduction)
        genesisBlock.nTime = std::time(0);
    genesisBlock.nNonce = 0;

    printf("NOTE: Genesis nTime = %u \n", genesisBlock.nTime);
    printf("WARN: Genesis nNonce (BLANK!) = %u \n", genesisBlock.nNonce);

    arith_uint256 besthash = maxUint;
    arith_uint256 hashTarget = UintToArith256(powLimit);
    printf("Target: %s\n", hashTarget.GetHex().c_str());
    arith_uint256 newhash = UintToArith256(genesisBlock.GetHash());
    while (newhash > hashTarget) {
        genesisBlock.nNonce++;
        if (genesisBlock.nNonce == 0) {
            printf("NONCE WRAPPED, incrementing time\n");
            ++genesisBlock.nTime;
        }
        // If nothing found after trying for a while, print status
        if ((genesisBlock.nNonce & 0xfff) == 0)
            printf("nonce %08X: hash = %s (target = %s)\n",
                genesisBlock.nNonce, newhash.ToString().c_str(),
                hashTarget.ToString().c_str());

        if (newhash < besthash) {
            besthash = newhash;
            printf("New best: %s\n", newhash.GetHex().c_str());
        }
        newhash = UintToArith256(genesisBlock.GetHash());
    }
    printf("Genesis nTime = %u \n", genesisBlock.nTime);
    printf("Genesis nNonce = %u \n", genesisBlock.nNonce);
    printf("Genesis nBits: %08x\n", genesisBlock.nBits);
    printf("Genesis Hash = %s\n", newhash.ToString().c_str());
    printf("Genesis Hash Merkle Root = %s\n", genesisBlock.hashMerkleRoot.ToString().c_str());
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Render therefore unto Caesar the things which are Caesar's;";
    const CScript genesisOutputScript = CScript() << ParseHex("") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        strNetworkID = "main";

        consensus.nRewardsStart = 20160;               // PoW Rewards begin on block 20160
        consensus.nServiceNodePaymentsStartBlock = 40320;  // ServiceNode Payments begin on block 40320
        consensus.nMinCountServiceNodesPaymentStart = 5; // ServiceNode Payments begin once 10 ServiceNodes exist or more.
        consensus.nInstantSendConfirmationsRequired = 11;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = 8766;   // actual historical value
        consensus.nBudgetPaymentsCycleBlocks = 87660; //Blocks per month
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nBudgetProposalEstablishingTime = 24 * 60 * 60;
        consensus.nSuperblockStartBlock = 8766;
        consensus.nSuperblockStartHash = uint256S("");
        consensus.nSuperblockCycle = 87660; // 2880 (Blocks per day) x 365.25 (Days per Year) / 12 = 87660
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nServiceNodeMinimumConfirmations = 15;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        consensus.nPowMaxAdjustUp = 16;
        consensus.nPowMaxAdjustDown = 24;
        consensus.nPowTargetSpacing = 30;
        consensus.nUpdateDiffAlgoHeight = 10; // Cash: Algorithm fork block
        assert(maxUint / UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 321; // 95% of nMinerConfirmationWindow
        consensus.nMinerConfirmationWindow = 40;        // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMaxReorganizationDepth = 100;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1513591800; // Dec 18th 2017 10:10:00
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1545134400;   // Dec 18th 2018 12:00:00

        // Deployment of BIP147
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 1533945600; // Aug 11th, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = 1565481600; // Aug 11th, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nWindowSize = 4032;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nThreshold = 3226; // 80% of 4032

        // Deployment of InstantSend autolocks
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nStartTime = 1533945600; // Aug 11th, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nTimeout = 1565481600; // Aug 11th, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nWindowSize = 4032;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nThreshold = 3226; // 80% of 4032

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = 0;

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S(""); //

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x5d;
        pchMessageStart[1] = 0x37;
        pchMessageStart[2] = 0x73;
        pchMessageStart[3] = 0x89;
        vAlertPubKey = ParseHex("040af09946b2f22a351ae39ee94ad15afde1f0d9ea45126359646456e60ab3fdde813ad0268383736948f58bb9846d46569a81a3c659041af14da438391ec2d1b1");
        nDefaultPort = DEFAULT_P2P_PORT;
        nPruneAfterHeight = 8766;
        startNewChain = false;
        nSwitchDifficultyBlock = 87660;

        genesis = CreateGenesisBlock(1710715047, 279676, UintToArith256(consensus.powLimit).GetCompact(), 1, (1 * COIN));
        if (startNewChain == true) {
            MineGenesis(genesis, consensus.powLimit, true);
        }

        consensus.hashGenesisBlock = genesis.GetHash();

        if (!startNewChain) {
            assert(consensus.hashGenesisBlock == uint256S("00000d4d67a6c4baa101c94ebd2e2d05e94a669a31e3fe8a34751af6a81f6c56"));
            assert(genesis.hashMerkleRoot == uint256S("2641029e7d5c403cd3c14716f29b395be8201db82168fd725292babf4a5ce11b"));
        }

        // vSeeds.push_back(CDNSSeedData("dnsseeder.network", "dyn-mainnet01.dnsseeder.network"));
        // vSeeds.push_back(CDNSSeedData("dnsseeder.network", "dyn-mainnet02.dnsseeder.network"));
        // vSeeds.push_back(CDNSSeedData("dnsseeder.network", "dyn-mainnet03.dnsseeder.network"));

        // Cash addresses start with 'C'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 28);
        // Cash script addresses start with 'M'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 50);
        // Cash private keys start with 'P'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 152);
        // Cash BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        // Cash BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();
        // Cash Stealth Address start with 'L'
        base58Prefixes[STEALTH_ADDRESS] = {0x0F};
        // Cash BIP44 coin type is '5'
        nExtCoinType = 5;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60 * 60; // fulfilled requests expire in 1 hour

        vSporkAddresses = {"CHh77xpQC3kcZbvSSPSguZ8YyLsSEYx6KY"};
        nMinSporkKeys = 1;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
                (0,         uint256S("0x00000d4d67a6c4baa101c94ebd2e2d05e94a669a31e3fe8a34751af6a81f6c56"))
                (5,         uint256S("0x00000f92caf807248ccc00ab88d1ff39fee2798a907d43279d7ec474e3bcfd1c"))                
        };

        chainTxData = ChainTxData{
            0,  // * UNIX timestamp of last known number of transactions
            0,  // * total number of transactions between genesis and that timestamp
                //   (the tx=... number in the SetBestChain debug.log lines)
            0.1 // * estimated number of transactions per second after that timestamp
        };
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams
{
public:
    CTestNetParams()
    {
        strNetworkID = "test";

        consensus.nRewardsStart = 0; // Rewards starts on block 0
        consensus.nServiceNodePaymentsStartBlock = 0;
        consensus.nMinCountServiceNodesPaymentStart = 1; // ServiceNode Payments begin once 1 ServiceNode exists or more.
        consensus.nInstantSendConfirmationsRequired = 11;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = 200;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nBudgetProposalEstablishingTime = 60 * 20;
        consensus.nSuperblockStartBlock = 0;
        consensus.nSuperblockStartHash = uint256(); // do not check this on testnet
        consensus.nSuperblockCycle = 24;            // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nServiceNodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 510;
        consensus.nMajorityRejectBlockOutdated = 750;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        consensus.nPowMaxAdjustUp = 16;
        consensus.nPowMaxAdjustDown = 24;
        consensus.nPowTargetSpacing = 30;
        consensus.nUpdateDiffAlgoHeight = 10; // Cash: Algorithm fork block
        assert(maxUint / UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 254; // 75% of nMinerConfirmationWindow
        consensus.nMinerConfirmationWindow = 30;        // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMaxReorganizationDepth = 100;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1513591800; // Dec 18th 2017 10:10:00
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1545134400;   // Dec 18th 2018 12:00:00

        // Deployment of BIP147
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 1517792400; // Feb 5th, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = 1549328400; // Feb 5th, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nThreshold = 50; // 50% of 100

        // Deployment of InstantSend autolocks
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nStartTime = 1532476800; // Jul 25th, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nTimeout = 1564012800; // Jul 25th, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nThreshold = 50; // 50% of 100

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = 210; // 210

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S(""); // 210

        pchMessageStart[0] = 0x2d;
        pchMessageStart[1] = 0x37;
        pchMessageStart[2] = 0x16;
        pchMessageStart[3] = 0x42;
        // To import alert key:  importprivkey QPgivWGaJY1dmeFiag7NZcwfhjJ6e4KnovMSxahbAXhF4o9YNH1T
        vAlertPubKey = ParseHex("0453f41764b792109ff87ae88c3bcaa8d5f7755f32d55f1db66ce9e003a3df5b44d106ac99e9ae00e3300715bc097e18779d1175da75c816b00c5a932ad88180df");
        nDefaultPort = DEFAULT_P2P_PORT + 100;
        nPruneAfterHeight = 100;
        startNewChain = false;
        nSwitchDifficultyBlock = 5000; // ~ 1 week

        genesis = CreateGenesisBlock(1710715253, 175, UintToArith256(consensus.powLimit).GetCompact(), 1, (1 * COIN));
        if (startNewChain == true) {
            MineGenesis(genesis, consensus.powLimit, true);
        }

        consensus.hashGenesisBlock = genesis.GetHash();

        if (!startNewChain) {
            assert(consensus.hashGenesisBlock == uint256S("001a3abffbef345ec744e9da493a635a18f2204c3caf2e88fe434af6c1e0190f"));
            assert(genesis.hashMerkleRoot == uint256S("2641029e7d5c403cd3c14716f29b395be8201db82168fd725292babf4a5ce11b"));
        }
        vFixedSeeds.clear();
        vSeeds.clear();
        //vSeeds.push_back(CDNSSeedData("",  ""));
        //vSeeds.push_back(CDNSSeedData("", ""));

        // Testnet Cash addresses start with 'c'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 87);
        // Testnet Cash script addresses start with '2'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        // Testnet private keys start with 'a'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 227);
        // Testnet Cash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet Cash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        // Cash Stealth Address start with 'T'
        base58Prefixes[STEALTH_ADDRESS] = {0x15};
        // Testnet Cash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes
        vSporkAddresses = {"cPZ6455uZnD6SXjkR8cyREXW4AEGd3XQrR"}; // ahA8aFhLzyd6dGumNg9PUwMW9fNouDp19dmw9EEW2UFM58d6DHSy
        nMinSporkKeys = 1;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            (0, uint256S(""))
        };

        chainTxData = ChainTxData{
            0,  // * UNIX timestamp of last known number of transactions
            0,  // * total number of transactions between genesis and that timestamp
                //   (the tx=... number in the SetBestChain debug.log lines)
            0.1 // * estimated number of transactions per second after that timestamp
        };
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams
{
public:
    CRegTestParams()
    {
        strNetworkID = "regtest";

        consensus.nRewardsStart = 0; // Rewards starts on block 0
        consensus.nServiceNodePaymentsStartBlock = 0;
        consensus.nMinCountServiceNodesPaymentStart = 1; // ServiceNode Payments begin once 1 ServiceNode exists or more.
        consensus.nInstantSendConfirmationsRequired = 11;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = 1000;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nBudgetProposalEstablishingTime = 60 * 20;
        consensus.nSuperblockStartBlock = 0;
        consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nServiceNodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        consensus.nPowMaxAdjustUp = 16;
        consensus.nPowMaxAdjustDown = 24;
        consensus.nPowTargetSpacing = 30;
        consensus.nUpdateDiffAlgoHeight = 10; // Cash: Algorithm fork block
        assert(maxUint / UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 254; // 75% of nMinerConfirmationWindow
        consensus.nMinerConfirmationWindow = 30;        // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMaxReorganizationDepth = 100;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_BIP147].nTimeout = 999999999999ULL;

        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_ISAUTOLOCKS].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = 0;

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x");

        pchMessageStart[0] = 0x2d;
        pchMessageStart[1] = 0x37;
        pchMessageStart[2] = 0x16;
        pchMessageStart[3] = 0x42;
        vAlertPubKey = ParseHex("04fdf359b342d267666089cb4630091db212223923701c322337f16db78e1ec216839dea1c6410726f428a7d65c217fb5f28701f23ec8720667835ef6929916593");
        nDefaultPort = DEFAULT_P2P_PORT + 200;
        nPruneAfterHeight = 100;
        startNewChain = false;
        nSwitchDifficultyBlock = 500;

        genesis = CreateGenesisBlock(1710715253, 6384, UintToArith256(consensus.powLimit).GetCompact(), 1, (1 * COIN));
        if (startNewChain == true) {
            MineGenesis(genesis, consensus.powLimit, true);
        }

        consensus.hashGenesisBlock = genesis.GetHash();

        if (!startNewChain) {
            assert(consensus.hashGenesisBlock == uint256S("000787843423b0f4511247c9fb613278d916d6a132dd3368597766ed0d323c07"));
            assert(genesis.hashMerkleRoot == uint256S("2641029e7d5c403cd3c14716f29b395be8201db82168fd725292babf4a5ce11b"));
        }

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = true;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;

        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"yQpLXyTyZSiMVyu3GBiwNP1AZASaRKaXdq"}; //private key: acg75cnWNuicdKTPJgqzEViZCDNTVMxPvg2FwgdPFc2fv1ic4qyd
        nMinSporkKeys = 1;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of(0, uint256S(""))};

        chainTxData = ChainTxData{
            0,  // * UNIX timestamp of last known number of transactions
            0,  // * total number of transactions between genesis and that timestamp
                //   (the tx=... number in the SetBestChain debug.log lines)
            0.1 // * estimated number of transactions per second after that timestamp
        };

        // Regtest Cash addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 140);
        // Regtest Cash script addresses start with '2'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        // Regtest private keys start with 'a' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 227);
        // Regtest Cash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Regtest Cash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        // Cash Stealth Address start with 'R'
        base58Prefixes[STEALTH_ADDRESS] = {0x13};
        // Regtest Cash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;
    }
    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};
static CRegTestParams regTestParams;


/**
 * Privatenet
 */
class CPrivateNetParams : public CChainParams
{
public:
    CPrivateNetParams()
    {
        strNetworkID = "privatenet";

        consensus.nRewardsStart = 0; // Rewards starts on block 0
        consensus.nServiceNodePaymentsStartBlock = 0;
        consensus.nMinCountServiceNodesPaymentStart = 1; // ServiceNode Payments begin once 1 ServiceNode exists or more.
        consensus.nInstantSendConfirmationsRequired = 11;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = 200;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nBudgetProposalEstablishingTime = 60 * 20;
        consensus.nSuperblockStartBlock = 0;
        consensus.nSuperblockStartHash = uint256(); // do not check this on testnet
        consensus.nSuperblockCycle = 24;            // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nServiceNodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 510;
        consensus.nMajorityRejectBlockOutdated = 750;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        consensus.nPowMaxAdjustUp = 16;
        consensus.nPowMaxAdjustDown = 24;
        consensus.nPowTargetSpacing = 30;
        consensus.nUpdateDiffAlgoHeight = 10; // Cash: Algorithm fork block
        assert(maxUint / UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 254; // 75% of nMinerConfirmationWindow
        consensus.nMinerConfirmationWindow = 30;        // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMaxReorganizationDepth = 100;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1513591800; // Dec 18th 2017 10:10:00
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1545134400;   // Dec 18th 2018 12:00:00

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = 210; // 210

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S(""); // 210

        pchMessageStart[0] = 0x2e;
        pchMessageStart[1] = 0x37;
        pchMessageStart[2] = 0x16;
        pchMessageStart[3] = 0x42;
        // To import alert key:  importprivkey 6Jjb9DG1cr71VWiwxg97zVEyZUBhFzzGhqE7GY9DrbYYM6gVgxS
        vAlertPubKey = ParseHex("04c992dd969b02d2a2c4bc52cea5a89ee72bab9958e9e7adfcaeb94700e46c11e2c18db2900dcb4597c02fe65787834839336e232873119ad44c39e853d747c878");
        nDefaultPort = DEFAULT_P2P_PORT + 300; // 33600
        nPruneAfterHeight = 100;
        startNewChain = false;
        nSwitchDifficultyBlock = 500000;

        genesis = CreateGenesisBlock(1710715258, 7819, UintToArith256(consensus.powLimit).GetCompact(), 1, (1 * COIN));
        if (startNewChain == true) {
            MineGenesis(genesis, consensus.powLimit, true);
        }

        consensus.hashGenesisBlock = genesis.GetHash();

        if (!startNewChain) {
            assert(consensus.hashGenesisBlock == uint256S("000092664700f54f20a6fcce6c34b0c718411d5784af096dec25e3bce137c34d"));
            assert(genesis.hashMerkleRoot == uint256S("2641029e7d5c403cd3c14716f29b395be8201db82168fd725292babf4a5ce11b"));
        }
        vFixedSeeds.clear();
        vSeeds.clear();
        //vSeeds.push_back(CDNSSeedData("",  ""));
        //vSeeds.push_back(CDNSSeedData("", ""));

        // Privatenet Cash addresses start with 'z'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 142);
        // Privatenet Cash script addresses start with '2'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        // Privatenet private keys start with 'a' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 227);
        // Privatenet Cash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Privatenet Cash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        // Privatenet Stealth Address start with 'P'
        base58Prefixes[STEALTH_ADDRESS] = {0x12};
        // Privatenet Cash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_privatenet, pnSeed6_privatenet + ARRAYLEN(pnSeed6_privatenet));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5 * 60; // fulfilled requests expire in 5 minutes
        // To import spork key (zFev59VAYq2J8T9zVLqq4v2AeX5r4RwAVd): importprivkey agAjA71T8gLXD2kSnvdALu3aYWM5tTvbVMhUmhmiS5Q8e2EkWfmb
        vSporkAddresses = {"zFev59VAYq2J8T9zVLqq4v2AeX5r4RwAVd"};
        nMinSporkKeys = 1;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of(0, uint256S(""))};

        chainTxData = ChainTxData{
            0,  // * UNIX timestamp of last known number of transactions
            0,  // * total number of transactions between genesis and that timestamp
                //   (the tx=... number in the SetBestChain debug.log lines)
            0.1 // * estimated number of transactions per second after that timestamp
        };
    }
};
static CPrivateNetParams privateNetParams;

static CChainParams* pCurrentParams = 0;

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
        return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
        return regTestParams;
    else if (chain == CBaseChainParams::PRIVATENET)
        return privateNetParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}
