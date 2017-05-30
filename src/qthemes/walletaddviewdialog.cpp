// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletaddviewdialog.h"
#include "ui_walletaddviewdialog.h"

WalletAddViewDialog::WalletAddViewDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WalletAddViewDialog)
{
    ui->setupUi(this);
}

WalletAddViewDialog::~WalletAddViewDialog()
{
    delete ui;
}
