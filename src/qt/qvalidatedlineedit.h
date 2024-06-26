// Copyright (c) 2016-2021 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2021 The Dash Core Developers
// Copyright (c) 2009-2021 The Bitcoin Developers
// Copyright (c) 2009-2021 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CASH_QT_QVALIDATEDLINEEDIT_H
#define CASH_QT_QVALIDATEDLINEEDIT_H

#include <QLineEdit>

/** Line edit that can be marked as "invalid" to show input validation feedback. When marked as invalid,
   it will get a red background until it is focused.
 */
class QValidatedLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit QValidatedLineEdit(QWidget* parent);
    void clear();
    void setCheckValidator(const QValidator* v);
    bool isValid();

protected:
    void focusInEvent(QFocusEvent* evt);
    void focusOutEvent(QFocusEvent* evt);

private:
    bool valid;
    const QValidator* checkValidator;

public Q_SLOTS:
    void setValid(bool valid);
    void setEnabled(bool enabled);

Q_SIGNALS:
    void validationDidChange(QValidatedLineEdit* validatedLineEdit);

private Q_SLOTS:
    void markValid();
    void checkValidity();
};

#endif // CASH_QT_QVALIDATEDLINEEDIT_H
