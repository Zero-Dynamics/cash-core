// Copyright (c) 2009-2021 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_CASHCONSENSUS_H
#define CASH_CASHCONSENSUS_H

#if defined(BUILD_CASH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/cash-config.h"
#if defined(_WIN32)
#if defined(DLL_EXPORT)
#if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL
#endif
#endif
#elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif
#elif defined(MSC_VER) && !defined(STATIC_LIBCASHCONSENSUS)
#define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
#define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CASHCONSENSUS_API_VER 0

typedef enum cashconsensus_error_t {
    cashconsensus_ERR_OK = 0,
    cashconsensus_ERR_TX_INDEX,
    cashconsensus_ERR_TX_SIZE_MISMATCH,
    cashconsensus_ERR_TX_DESERIALIZE,
    cashconsensus_ERR_INVALID_FLAGS,
} cashconsensus_error;

/** Script verification flags */
enum {
    cashconsensus_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    cashconsensus_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0),  // evaluate P2SH (BIP16) subscripts
    cashconsensus_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2),  // enforce strict DER (BIP66) compliance
    cashconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY           = (1U << 4),  // enforce NULLDUMMY (BIP147)
    cashconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),  // enable CHECKLOCKTIMEVERIFY (BIP65)
    cashconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10), // enable CHECKSEQUENCEVERIFY (BIP112)
    cashconsensus_SCRIPT_FLAGS_VERIFY_ALL = cashconsensus_SCRIPT_FLAGS_VERIFY_P2SH | cashconsensus_SCRIPT_FLAGS_VERIFY_DERSIG |
                                               cashconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY | cashconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
                                               cashconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not NULL, err will contain an error/success code for the operation
EXPORT_SYMBOL int cashconsensus_verify_script(const unsigned char* scriptPubKey, unsigned int scriptPubKeyLen, const unsigned char* txTo, unsigned int txToLen, unsigned int nIn, unsigned int flags, cashconsensus_error* err);

EXPORT_SYMBOL unsigned int cashconsensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // CASH_CASHCONSENSUS_H
