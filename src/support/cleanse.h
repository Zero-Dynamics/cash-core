// Copyright (c) 2009-2021 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_SUPPORT_CLEANSE_H
#define CASH_SUPPORT_CLEANSE_H

#include <stdlib.h>

// Attempt to overwrite data in the specified memory span.
void memory_cleanse(void* ptr, size_t len);

#endif // CASH_SUPPORT_CLEANSE_H
