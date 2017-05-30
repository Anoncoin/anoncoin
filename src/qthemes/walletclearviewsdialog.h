// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETCLEARVIEWSDIALOG_H
#define WALLETCLEARVIEWSDIALOG_H

#include <QDialog>

namespace Ui {
class WalletClearViewsDialog;
}

class WalletClearViewsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletClearViewsDialog(QWidget *parent = 0);
    ~WalletClearViewsDialog();

private:
    Ui::WalletClearViewsDialog *ui;
};

#endif // WALLETCLEARVIEWSDIALOG_H
