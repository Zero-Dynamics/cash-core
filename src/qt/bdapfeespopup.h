// Copyright (c) 2019-2021 Duality Blockchain Solutions Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_BDAPFEESPOPUP_H
#define CASH_QT_BDAPFEESPOPUP_H

#include "bdap/fees.h"
#include "bdappage.h"
#include "cashunits.h"

#include <QDialog>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTranslator>

bool bdapFeesPopup(QWidget *parentDialog, const opcodetype& opCodeAction, const opcodetype& opCodeObject, BDAP::ObjectType inputAccountType, int unit, int32_t regMonths = DEFAULT_REGISTRATION_MONTHS);

#endif // CASH_QT_BDAPFEESPOPUP_H