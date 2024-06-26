// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_NETWORKSTYLE_H
#define CASH_QT_NETWORKSTYLE_H

#include <QIcon>
#include <QPixmap>
#include <QString>

/* Coin network-specific GUI style information */
class NetworkStyle
{
public:
    /** Get style associated with provided BIP70 network id, or 0 if not known */
    static const NetworkStyle* instantiate(const QString& networkId);

    const QString& getAppName() const { return appName; }
    const QIcon& getAppIcon() const { return appIcon; }
    const QPixmap& getSplashImage() const { return splashImage; }
    const QIcon& getTrayAndWindowIcon() const { return trayAndWindowIcon; }
    const QString& getTitleAddText() const { return titleAddText; }

private:
    NetworkStyle(const QString& appName, const int iconColorHueShift, const int iconColorSaturationReduction, const char* titleAddText);

    QString appName;
    QIcon appIcon;
    QPixmap splashImage;
    QIcon trayAndWindowIcon;
    QString titleAddText;

    void rotateColors(QImage& img, const int iconColorHueShift, const int iconColorSaturationReduction);
};

#endif // CASH_QT_NETWORKSTYLE_H
