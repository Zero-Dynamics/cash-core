// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_RPCPROTOCOL_H
#define CASH_RPCPROTOCOL_H

#include <univalue.h>

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <boost/filesystem.hpp>

//! HTTP status codes
enum HTTPStatusCode {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_BAD_METHOD = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE = 503,
};

//! Cash RPC error codes
enum RPCErrorCode {
    //! Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS = -32602,
    RPC_INTERNAL_ERROR = -32603,
    RPC_PARSE_ERROR = -32700,

    //! General application defined errors
    RPC_MISC_ERROR = -1,               //! std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE = -2,   //! Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR = -3,               //! Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY = -5,   //! Invalid address or key
    RPC_OUT_OF_MEMORY = -7,            //! Ran out of memory during operation
    RPC_INVALID_PARAMETER = -8,        //! Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR = -20,          //! Database error
    RPC_SPORK_INACTIVE = -21,          //! Required spork inactivate
    RPC_DESERIALIZATION_ERROR = -22,   //! Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR = -25,            //! General error during transaction or block submission
    RPC_VERIFY_REJECTED = -26,         //! Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN = -27, //! Transaction already in chain
    RPC_IN_WARMUP = -28,               //! Client still warming up

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    RPC_CLIENT_NOT_CONNECTED = -9,         //! Cash is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD = -10,  //! Still downloading initial blocks
    RPC_CLIENT_NODE_ALREADY_ADDED = -23,   //! Node is already added
    RPC_CLIENT_NODE_NOT_ADDED = -24,       //! Node has not been added before
    RPC_CLIENT_NODE_NOT_CONNECTED = -29,   //! Node to disconnect not found in connected nodes
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -30, //! Invalid IP/Subnet
    RPC_CLIENT_P2P_DISABLED = -31,         //!< No valid connection manager instance found

    //! Wallet errors
    RPC_WALLET_ERROR = -4,                 //! Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS = -6,    //! Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11, //! Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT = -12,      //! Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED = -13,        //! Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, //! The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE = -15,      //! Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED = -16,    //! Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED = -17,     //! Wallet is already unlocked
    RPC_WALLET_NEEDS_UPGRADING = -18,      //! Wallet needs upgrading
    RPC_WALLET_PRIV_KEY_NOT_FOUND = -19,   //! Can not get the private key from the local wallet
    //! BDAP errors
    RPC_BDAP_ERROR = -300,                 //! Unspecified BDAP error
    RPC_BDAP_SPORK_INACTIVE = -301,        //! BDAP spork is not active
    RPC_BDAP_DB_ERROR = -302,              //! BDAP database error
    RPC_BDAP_LINK_MNGR_ERROR = -303,       //! BDAP link manager error
    RPC_BDAP_ACCOUNT_NOT_FOUND = -304,     //! BDAP account not found
    RPC_BDAP_FEE_UNKNOWN = -305,           //! BDAP fee can not be calculated
    RPC_BDAP_AUDIT_INVALID = -306,         //! Invalid audit data
    RPC_BDAP_INVALID_SIGNATURE = -307,     //! Invalid signature
    RPC_BDAP_CERTIFICATE_INVALID = -308,   //! Invalid certificate data
    RPC_BDAP_SELF_SIGNED_CERTIFICATE_NOT_ALLOWED = -309,   //! Self signed certificate not allowed
    RPC_BDAP_CERTIFICATE_EXPORT_ERROR = -310,   //! Could not export certificate

    //! DHT errors
    RPC_DHT_ERROR = -400,                  //! Unspecified problem with the DHT
    RPC_DHT_NOT_STARTED = -401,            //! DHT session not started
    RPC_DHT_GET_KEY_FAILED = -402,         //! Get DHT private key failed
    RPC_DHT_GET_FAILED = -403,             //! Get DHT data failed
    RPC_DHT_PUT_FAILED = -404,             //! Put DHT data failed
    RPC_DHT_INVALID_HEADER = -405,         //! Invalid DHT header information
    RPC_DHT_INVALID_RECORD = -406,         //! Invalid DHT record information
    RPC_DHT_RECORD_LOCKED = -407,          //! DHT record locked
    RPC_DHT_PUBKEY_MISMATCH = -408,        //! DHT public key mismatch
    //! Fluid errors
    RPC_FLUID_ERROR = -500,                //! Unspecified fluid error
    RPC_FLUID_INVALID_TIMESTAMP = -501,    //! Invalid fluid timestamp
};

UniValue JSONRPCRequestObj(const std::string& strMethod, const UniValue& params, const UniValue& id);
UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id);
std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id);
UniValue JSONRPCError(int code, const std::string& message);

/** Get name of RPC authentication cookie file */
boost::filesystem::path GetAuthCookieFile();
/** Generate a new RPC authentication cookie and write it to disk */
bool GenerateAuthCookie(std::string* cookie_out);
/** Read the RPC authentication cookie from disk */
bool GetAuthCookie(std::string* cookie_out);
/** Delete RPC authentication cookie from disk */
void DeleteAuthCookie();

#endif // CASH_RPCPROTOCOL_H
