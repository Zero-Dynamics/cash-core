// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_GUICONSTANTS_H
#define CASH_QT_GUICONSTANTS_H

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 250;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* CashGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#E16082"

/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(162, 162, 162)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(128, 0, 0)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(136, 136, 136)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)
/* Transaction list -- TX status decoration - offline */
#define COLOR_TX_STATUS_OFFLINE QColor(192, 192, 192)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(18, 0, 6)
/* Transaction list -- TX status decoration - Locked by InstantSend (Dark Blue) */
#define COLOR_TX_STATUS_LOCKED QColor(13, 81, 140)
/* Transaction list -- TX status decoration - Fluid Transaction (Light Blue) */
#define COLOR_FLUID_TX QColor(0, 35, 102)
/* Transaction list -- TX status decoration - ServiceNode Reward (Purple)*/
#define COLOR_SERVICENODE_REWARD QColor(102, 2, 60)
/* Transaction list -- TX status decoration - Generated (Gold) */
#define COLOR_GENERATED QColor(207, 181, 59)
/* Transaction list -- TX status decoration - orphan (Light Gray) */
#define COLOR_ORPHAN QColor(211, 211, 211)

/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define QR_IMAGE_SIZE 300

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "Zero Dynamics"
#define QAPP_ORG_DOMAIN " "
#define QAPP_APP_NAME_DEFAULT "Cash-Qt"
#define QAPP_APP_NAME_TESTNET "Cash-Qt-testnet"

#endif // CASH_QT_GUICONSTANTS_H
