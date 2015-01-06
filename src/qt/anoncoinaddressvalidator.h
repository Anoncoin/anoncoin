// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOINADDRESSVALIDATOR_H
#define ANONCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class AnoncoinAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit AnoncoinAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Anoncoin address widget validator, checks for a valid anoncoin address.
 */
class AnoncoinAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit AnoncoinAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // ANONCOINADDRESSVALIDATOR_H
