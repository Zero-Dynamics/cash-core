// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_SIGNVERIFYMESSAGEDIALOG_H
#define CASH_QT_SIGNVERIFYMESSAGEDIALOG_H

#include <QDialog>

class PlatformStyle;
class WalletModel;

namespace Ui
{
class SignVerifyMessageDialog;
}

class SignVerifyMessageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignVerifyMessageDialog(const PlatformStyle* platformStyle, QWidget* parent);
    ~SignVerifyMessageDialog();

    void setModel(WalletModel* model);
    void setAddress_SM(const QString& address);
    void setAddress_VM(const QString& address);

    void showTab_SM(bool fShow);
    void showTab_VM(bool fShow);

protected:
    bool eventFilter(QObject* object, QEvent* event);

private:
    Ui::SignVerifyMessageDialog* ui;
    WalletModel* model;
    const PlatformStyle* platformStyle;

private Q_SLOTS:
    /* sign message */
    void on_addressBookButton_SM_clicked();
    void on_pasteButton_SM_clicked();
    void on_signMessageButton_SM_clicked();
    void on_copySignatureButton_SM_clicked();
    void on_clearButton_SM_clicked();
    /* verify message */
    void on_addressBookButton_VM_clicked();
    void on_verifyMessageButton_VM_clicked();
    void on_clearButton_VM_clicked();
};

#endif // CASH_QT_SIGNVERIFYMESSAGEDIALOG_H
