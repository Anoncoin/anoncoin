// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETRESTARTCOREDIALOG_H
#define WALLETRESTARTCOREDIALOG_H

#include <QDialog>

namespace Ui {
class WalletRestartCoreDialog;
}

class WalletRestartCoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletRestartCoreDialog(QWidget *parent = 0);
    ~WalletRestartCoreDialog();

private:
    Ui::WalletRestartCoreDialog *ui;
};

#endif // WALLETRESTARTCOREDIALOG_H
