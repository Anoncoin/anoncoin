// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef I2POPTIONSDIALOG_H
#define I2POPTIONSDIALOG_H

#include <QDialog>

namespace Ui {
class I2POptionsDialog;
}

class I2POptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit I2POptionsDialog(QWidget *parent = 0);
    ~I2POptionsDialog();
    void UpdateParameters();

    void reject();

private:
    Ui::I2POptionsDialog *ui;
    bool WriteToDataFile();
    void WriteToMapArgs();
    void EnableOrDisableGUIElements(void);

private Q_SLOTS:
    void settingsModified();
    void onI2PEnabledCheckBoxModified();
    void onI2PStaticPKCheckBoxModified();
    void onAcceptDialog();
    void onCloseDialog();
    void pushButtonApply();
    void pushButtonCancel();

Q_SIGNALS:
    void dialogIsClosing();
};

#endif // I2POPTIONSDIALOG_H
