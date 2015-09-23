// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "anoncoinunits.h"

#include <QStringList>

AnoncoinUnits::AnoncoinUnits(QObject *parent):
        QAbstractListModel(parent),
        unitlist(availableUnits())
{
}

QList<AnoncoinUnits::Unit> AnoncoinUnits::availableUnits()
{
    QList<AnoncoinUnits::Unit> unitlist;
    unitlist.append(ANC);
    unitlist.append(mANC);
    unitlist.append(uANC);
    return unitlist;
}

bool AnoncoinUnits::valid(int unit)
{
    switch(unit)
    {
    case ANC:
    case mANC:
    case uANC:
        return true;
    default:
        return false;
    }
}

QString AnoncoinUnits::id(int unit)
{
    switch(unit)
    {
    case ANC: return QString("anc");
    case mANC: return QString("manc");
    case uANC: return QString("uanc");
    default: return QString("???");
    }
}

QString AnoncoinUnits::name(int unit)
{
    switch(unit)
    {
    case ANC: return QString("ANC");
    case mANC: return QString("mANC");
    case uANC: return QString("μANC");
    default: return QString("???");
    }
}

QString AnoncoinUnits::description(int unit)
{
    switch(unit)
    {
    case ANC: return QString("Anoncoins");
    case mANC: return QString("Milli-Anoncoins (1 / 1" THIN_SP_UTF8 "000)");
    case uANC: return QString("Micro-Anoncoins (1 / 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000)");
    default: return QString("???");
    }
}

qint64 AnoncoinUnits::factor(int unit)
{
    switch(unit)
    {
    case ANC:  return 100000000;
    case mANC: return 100000;
    case uANC: return 100;
    default:   return 100000000;
    }
}

int AnoncoinUnits::decimals(int unit)
{
    switch(unit)
    {
    case ANC: return 8;
    case mANC: return 5;
    case uANC: return 2;
    default: return 0;
    }
}

QString AnoncoinUnits::format(int unit, const qint64& n, bool fPlus, SeparatorStyle separators)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if(!valid(unit))
        return QString(); // Refuse to format invalid unit
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');
    return quotient_str + QString(".") + remainder_str;
}

// TODO: Review all remaining calls to AnoncoinUnits::formatWithUnit to
// TODO: determine whether the output is used in a plain text context
// TODO: or an HTML context (and replace with
// TODO: AnoncoinUnits::formatHtmlWithUnit in the latter case). Hopefully
// TODO: there aren't instances where the result could be used in
// TODO: either context.

// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString AnoncoinUnits::formatWithUnit(int unit, const qint64& amount, bool plussign, SeparatorStyle separators)
{
    return format(unit, amount, plussign, separators) + QString(" ") + name(unit);
}

QString AnoncoinUnits::formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}


bool AnoncoinUnits::parse(int unit, const QString &value, qint64 *val_out)
{
    if(!valid(unit) || value.isEmpty())
        return false; // Refuse to parse invalid unit or empty string
    int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if(parts.size() > 2)
    {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > num_decimals)
    {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if(str.size() > 18)
    {
        return false; // Longer numbers will exceed 63 bits
    }
    qint64 retvalue(str.toLongLong(&ok));
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

QString AnoncoinUnits::getAmountColumnTitle(int unit)
{
    QString amountTitle = QObject::tr("Amount");
    if (AnoncoinUnits::valid(unit))
    {
        amountTitle += " ("+AnoncoinUnits::name(unit) + ")";
    }
    return amountTitle;
}

int AnoncoinUnits::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant AnoncoinUnits::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if(row >= 0 && row < unitlist.size())
    {
        Unit unit = unitlist.at(row);
        switch(role)
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}

CAmount AnoncoinUnits::maxMoney()
{
    return MAX_MONEY;
}

qint64 AnoncoinUnits::maxAmount(int unit)
{
    switch(unit)
    {
    case ANC:  return Q_INT64_C(3105156);
    case mANC: return Q_INT64_C(31051560000);
    case uANC: return Q_INT64_C(31051560000000);
    default:   return 0;
    }
}

int AnoncoinUnits::amountDigits(int unit)
{
    switch(unit)
    {
    case ANC: return 7;     // 3,105,156 (# digits, without commas)
    case mANC: return 10;   // 3,105,156,000
    case uANC: return 13;   // 3,105,156,000,000
    default: return 0;
    }
}

