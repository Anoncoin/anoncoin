// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletclearviewsdialog.h"
#include "ui_walletclearviewsdialog.h"

WalletClearViewsDialog::WalletClearViewsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WalletClearViewsDialog)
{
    ui->setupUi(this);
}

WalletClearViewsDialog::~WalletClearViewsDialog()
{
    delete ui;
}
