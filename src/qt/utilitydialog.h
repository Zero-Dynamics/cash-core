// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_UTILITYDIALOG_H
#define CASH_QT_UTILITYDIALOG_H

#include <QDialog>
#include <QObject>

class ClientModel;
class CashGUI;

namespace Ui
{
class HelpMessageDialog;
}

/** "Help message" dialog box */
class HelpMessageDialog : public QDialog
{
    Q_OBJECT

public:
    enum HelpMode {
        about,
        cmdline,
        pshelp
    };

    explicit HelpMessageDialog(QWidget* parent, HelpMode helpMode);
    ~HelpMessageDialog();

    void printToConsole();
    void showOrPrint();

private:
    Ui::HelpMessageDialog* ui;
    QString text;

private Q_SLOTS:
    void on_okButton_accepted();
};


/** "Shutdown" window */
class ShutdownWindow : public QWidget
{
    Q_OBJECT

public:
    ShutdownWindow(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    static QWidget* showShutdownWindow(CashGUI* window);

protected:
    void closeEvent(QCloseEvent* event);
};

#endif // CASH_QT_UTILITYDIALOG_H
