// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_DEBITADDRESSVALIDATOR_H
#define CASH_QT_DEBITADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class DebitAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit DebitAddressEntryValidator(QObject* parent);

    State validate(QString& input, int& pos) const;
};

/** Cash debit address widget validator, checks for a valid Cash debit address.
 */
class DebitAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit DebitAddressCheckValidator(QObject* parent);

    State validate(QString& input, int& pos) const;
};

#endif // CASH_QT_DEBITADDRESSVALIDATOR_H
