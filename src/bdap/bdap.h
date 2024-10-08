// Copyright (c) 2019-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_BDAP_H
#define CASH_BDAP_H

#include <cstdint>
#include <string>
#include <vector>

namespace BDAP {
    enum ObjectType {
        BDAP_DEFAULT_TYPE = 0,
        BDAP_USER = 1,
        BDAP_GROUP = 2,
        BDAP_DEVICE = 3,
        BDAP_DOMAIN = 4,
        BDAP_ORGANIZATIONAL_UNIT = 5,
        BDAP_CERTIFICATE = 6,
        BDAP_AUDIT = 7,
        BDAP_SIDECHAIN = 8,
        BDAP_SIDECHAIN_CHECKPOINT = 9,
        BDAP_LINK_REQUEST = 10,
        BDAP_LINK_ACCEPT = 11,
        BDAP_IDENTITY = 12,
        BDAP_IDENTITY_VERIFICATION = 13
    };
}

typedef std::vector<unsigned char> CharString;
typedef std::vector<CharString> vchCharString;
typedef std::pair<uint32_t, CharString> CheckPoint;
typedef std::vector<CheckPoint> vCheckPoints; // << height, block hash >>

static constexpr unsigned int ACTIVATE_BDAP_HEIGHT        = 10; // TODO: Change for mainnet or spork activate (???)
static constexpr unsigned int MAX_OBJECT_NAME_LENGTH      = 63;
static constexpr unsigned int MAX_OBJECT_FULL_PATH_LENGTH = (MAX_OBJECT_NAME_LENGTH * 3) + 2; // domain + ou + object name + 2 dot chars
static constexpr unsigned int MAX_ALGORITHM_TYPE_LENGTH   = 32;
static constexpr unsigned int MAX_DATA_DESCRIPTION_LENGTH = 128;
static constexpr unsigned int MAX_COMMON_NAME_LENGTH      = 95;
static constexpr unsigned int MAX_ORG_NAME_LENGTH         = 95;
static constexpr unsigned int MAX_WALLET_ADDRESS_LENGTH   = 105; // Stealth addresses are 102 chars in length plus 3 for a prefix. Regular addresses are 34 chars.
static constexpr unsigned int MAX_RESOURCE_POINTER_LENGTH = 127;
static constexpr unsigned int MAX_KEY_LENGTH              = 156;
static constexpr unsigned int MAX_DESCRIPTION_LENGTH      = 256;
static constexpr unsigned int MAX_CERTIFICATE_FILENAME    = 256;
static constexpr unsigned int MAX_CERTIFICATE_LENGTH      = 512;
static constexpr unsigned int MAX_CERTIFICATE_NAME        = 63;
static constexpr unsigned int MAX_CERTIFICATE_CATEGORY    = 32;
static constexpr unsigned int MAX_CERTIFICATE_FINGERPRINT = 32;
static constexpr unsigned int MAX_CERTIFICATE_PEM_LENGTH  = 3600;
static constexpr unsigned int MAX_CERTIFICATE_EXTENSION_RECORDS = 10;
static constexpr unsigned int MAX_CERTIFICATE_EXTENSION_LENGTH  = 100;
static constexpr unsigned int MAX_CERTIFICATE_KEY_LENGTH        = 512;
static constexpr unsigned int MAX_CERTIFICATE_SIGNATURE_LENGTH  = 512;
static constexpr unsigned int MAX_CERTIFICATE_MONTHS_VALID      = 12;
static constexpr unsigned int MAX_CERTIFICATE_CA_MONTHS_VALID   = 120;
static constexpr unsigned int MAX_OID_LENGTH              = 128;
static constexpr unsigned int MAX_SIGNATURE_LENGTH        = 65; // https://bitcoin.stackexchange.com/questions/12554/why-the-signature-is-always-65-13232-bytes-long
static constexpr unsigned int MAX_PRIVATE_DATA_LENGTH     = 512; // Pay per byte for hosting on chain
static constexpr unsigned int MAX_NUMBER_CHECKPOINTS      = 25; // Pay per byte for hosting on chain
static constexpr unsigned int MAX_CHECKPOINT_HASH_LENGTH  = 64;
static constexpr unsigned int DHT_HEX_PUBLIC_KEY_LENGTH   = 64; // Ed25519 pubkeys are 32 bytes and 64 bytes when hex encoded.
static constexpr unsigned int MAX_BDAP_LINK_MESSAGE       = 256;
static constexpr unsigned int MAX_BDAP_SIGNATURE_PROOF    = 90; // TODO (bdap): Update to 65 or use MAX_SIGNATURE_LENGTH when you start a new chain.
static constexpr unsigned int MAX_BDAP_LINK_DATA_SIZE     = 1592;
static constexpr uint64_t DEFAULT_LINK_EXPIRE_TIME        = 1861920000;
static constexpr int32_t DEFAULT_REGISTRATION_MONTHS      = 12; // 1 year
static constexpr bool ENFORCE_BDAP_CREDIT_USE             = false; // TODO: Change to true before release
static constexpr uint32_t MAX_REGISTRATION_MONTHS         = 1200; // 100 years
static constexpr unsigned int MAX_BDAP_AUDIT_HASH_SIZE    = 64;
static const std::string DEFAULT_PUBLIC_DOMAIN            = "bdap.io";
static const std::string DEFAULT_PUBLIC_OU                = "public";
static const std::string DEFAULT_ADMIN_OU                 = "admin";
static const std::string DEFAULT_ORGANIZATION_NAME        = "Zero Dynamics";
/*
BDAP Root OID: 2.16.840.1.114564
{joint-iso-ccitt(2) country(16) US(840) organization(1) BDAP(114564)}
*/
static const std::string DEFAULT_OID_PREFIX               = "2.16.840.1.114564";

inline const CharString ConvertConstantToCharString (const std::string strConvert)
{
    CharString vchConvert(strConvert.begin(), strConvert.end());
    return vchConvert;
};

static const CharString vchDefaultDomainName = ConvertConstantToCharString(DEFAULT_PUBLIC_DOMAIN);
static const CharString vchDefaultPublicOU = ConvertConstantToCharString(DEFAULT_PUBLIC_OU);
static const CharString vchDefaultAdminOU = ConvertConstantToCharString(DEFAULT_ADMIN_OU);
static const CharString vchDefaultOrganizationName = ConvertConstantToCharString(DEFAULT_ORGANIZATION_NAME);
static const CharString vchDefaultOIDPrefix = ConvertConstantToCharString(DEFAULT_OID_PREFIX);

#endif // CASH_BDAP_H
