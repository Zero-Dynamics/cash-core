// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_COINCONTROLDIALOG_H
#define CASH_QT_COINCONTROLDIALOG_H

#include "amount.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>

class PlatformStyle;
class WalletModel;

class CCoinControl;
class CTxMemPool;

namespace Ui
{
class CoinControlDialog;
}

#define ASYMP_UTF8 "\xE2\x89\x88"

class CCoinControlWidgetItem : public QTreeWidgetItem
{
public:
    CCoinControlWidgetItem(QTreeWidget* parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    CCoinControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    CCoinControlWidgetItem(QTreeWidgetItem* parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem& other) const;
};

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(const PlatformStyle* platformStyle, QWidget* parent = 0);
    ~CoinControlDialog();

    void setModel(WalletModel* model);

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, QDialog*);

    static QList<CAmount> payAmounts;
    static CCoinControl* coinControl;
    static bool fSubtractFeeFromAmount;

private:
    Ui::CoinControlDialog* ui;
    WalletModel* model;
    int sortColumn;
    Qt::SortOrder sortOrder;

    QMenu* contextMenu;
    QTreeWidgetItem* contextMenuItem;
    QAction* copyTransactionHashAction;
    QAction* lockAction;
    QAction* unlockAction;

    const PlatformStyle* platformStyle;

    void sortView(int, Qt::SortOrder);
    void updateView();

    enum {
        COLUMN_CHECKBOX = 0,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_PRIVATESEND_ROUNDS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_VOUT_INDEX,
        COLUMN_TXHASH

    };

    friend class CCoinControlWidgetItem;

private Q_SLOTS:
    void showMenu(const QPoint&);
    void copyAmount();
    void copyLabel();
    void copyAddress();
    void copyTransactionHash();
    void lockCoin();
    void unlockCoin();
    void clipboardQuantity();
    void clipboardAmount();
    void clipboardFee();
    void clipboardAfterFee();
    void clipboardBytes();
    void clipboardLowOutput();
    void clipboardChange();
    void radioTreeMode(bool);
    void radioListMode(bool);
    void viewItemChanged(QTreeWidgetItem*, int);
    void headerSectionClicked(int);
    void buttonBoxClicked(QAbstractButton*);
    void buttonSelectAllClicked();
    void buttonToggleLockClicked();
    void updateLabelLocked();
};

#endif // CASH_QT_COINCONTROLDIALOG_H
