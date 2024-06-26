// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_PRIVATESENDCONFIG_H
#define CASH_QT_PRIVATESENDCONFIG_H

#include <QDialog>

namespace Ui
{
class PrivateSendConfig;
}
class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class PrivateSendConfig : public QDialog
{
    Q_OBJECT

public:
    PrivateSendConfig(QWidget* parent = 0);
    ~PrivateSendConfig();

    void setModel(WalletModel* model);


private:
    Ui::PrivateSendConfig* ui;
    WalletModel* model;
    void configure(bool enabled, int coins, int rounds);

private Q_SLOTS:

    void clickBasic();
    void clickHigh();
    void clickMax();
};

#endif // CASH_QT_PRIVATESENDCONFIG_H
