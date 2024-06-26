// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_ZMQ_ZMQCONFIG_H
#define CASH_ZMQ_ZMQCONFIG_H

#if defined(HAVE_CONFIG_H)
#include "config/cash-config.h"
#endif

#include <stdarg.h>
#include <string>

#if ENABLE_ZMQ
#include <zmq.h>
#endif

#include "primitives/block.h"
#include "primitives/transaction.h"

#include "governance-object.h"
#include "governance-vote.h"

#include "instantsend.h"

void zmqError(const char *str);

#endif // CASH_ZMQ_ZMQCONFIG_H
