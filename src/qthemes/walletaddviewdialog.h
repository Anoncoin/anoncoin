// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETADDVIEWDIALOG_H
#define WALLETADDVIEWDIALOG_H

#include <QDialog>

namespace Ui {
class WalletAddViewDialog;
}

class WalletAddViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletAddViewDialog(QWidget *parent = 0);
    ~WalletAddViewDialog();

private:
    Ui::WalletAddViewDialog *ui;
};

#endif // WALLETADDVIEWDIALOG_H
