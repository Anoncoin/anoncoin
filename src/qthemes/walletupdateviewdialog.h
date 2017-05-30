// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WALLETUPDATEVIEWDIALOG_H
#define WALLETUPDATEVIEWDIALOG_H

#include <QDialog>

namespace Ui {
class WalletUpdateViewDialog;
}

class WalletUpdateViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletUpdateViewDialog(QWidget *parent = 0);
    ~WalletUpdateViewDialog();

private:
    Ui::WalletUpdateViewDialog *ui;
};

#endif // WALLETUPDATEVIEWDIALOG_H
