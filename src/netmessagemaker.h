// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_NETMESSAGEMAKER_H
#define CASH_NETMESSAGEMAKER_H

#include "net.h"
#include "serialize.h"

class CNetMsgMaker
{
public:
    CNetMsgMaker(int nVersionIn) : nVersion(nVersionIn) {}

    template <typename... Args>
    CSerializedNetMsg Make(int nFlags, std::string sCommand, Args&&... args) const
    {
        CSerializedNetMsg msg;
        msg.command = std::move(sCommand);
        CVectorWriter{SER_NETWORK, nFlags | nVersion, msg.data, 0, std::forward<Args>(args)...};
        return msg;
    }

    template <typename... Args>
    CSerializedNetMsg Make(std::string sCommand, Args&&... args) const
    {
        return Make(0, std::move(sCommand), std::forward<Args>(args)...);
    }

private:
    const int nVersion;
};

#endif // CASH_NETMESSAGEMAKER_H
