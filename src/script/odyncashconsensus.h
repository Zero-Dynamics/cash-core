// Copyright (c) 2009-2021 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ODYNCASH_ODYNCASHCONSENSUS_H
#define ODYNCASH_ODYNCASHCONSENSUS_H

#if defined(BUILD_ODYNCASH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/odyncash-config.h"
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
#elif defined(MSC_VER) && !defined(STATIC_LIBODYNCASHCONSENSUS)
#define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
#define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ODYNCASHCONSENSUS_API_VER 0

typedef enum odyncashconsensus_error_t {
    odyncashconsensus_ERR_OK = 0,
    odyncashconsensus_ERR_TX_INDEX,
    odyncashconsensus_ERR_TX_SIZE_MISMATCH,
    odyncashconsensus_ERR_TX_DESERIALIZE,
    odyncashconsensus_ERR_INVALID_FLAGS,
} odyncashconsensus_error;

/** Script verification flags */
enum {
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0),  // evaluate P2SH (BIP16) subscripts
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2),  // enforce strict DER (BIP66) compliance
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY           = (1U << 4),  // enforce NULLDUMMY (BIP147)
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),  // enable CHECKLOCKTIMEVERIFY (BIP65)
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10), // enable CHECKSEQUENCEVERIFY (BIP112)
    odyncashconsensus_SCRIPT_FLAGS_VERIFY_ALL = odyncashconsensus_SCRIPT_FLAGS_VERIFY_P2SH | odyncashconsensus_SCRIPT_FLAGS_VERIFY_DERSIG |
                                               odyncashconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY | odyncashconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
                                               odyncashconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not NULL, err will contain an error/success code for the operation
EXPORT_SYMBOL int odyncashconsensus_verify_script(const unsigned char* scriptPubKey, unsigned int scriptPubKeyLen, const unsigned char* txTo, unsigned int txToLen, unsigned int nIn, unsigned int flags, odyncashconsensus_error* err);

EXPORT_SYMBOL unsigned int odyncashconsensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // ODYNCASH_ODYNCASHCONSENSUS_H
