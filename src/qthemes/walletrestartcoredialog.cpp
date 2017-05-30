// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletrestartcoredialog.h"
#include "ui_walletrestartcoredialog.h"

WalletRestartCoreDialog::WalletRestartCoreDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WalletRestartCoreDialog)
{
    ui->setupUi(this);
}

WalletRestartCoreDialog::~WalletRestartCoreDialog()
{
    delete ui;
}
