// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef I2POPTIONSDIALOG_H
#define I2POPTIONSDIALOG_H

#include <QDialog>

//class MonitoredDataMapper;

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

    //void setMapper(MonitoredDataMapper& mapper);
    void reject();

private:
    Ui::I2POptionsDialog *ui;
    bool WriteToDataFile();
    void WriteToMapArgs();

private Q_SLOTS:
//    void ShowCurrentI2PAddress();
//    void GenerateNewI2PAddress();
    void settingsModified();
    void onCloseDialog();
    void pushButtonSaveI2PSettings();
    void pushButtonLoadI2PSettings();
    void changeI2PSettingsState();

Q_SIGNALS:
//    void settingsChanged();
    void dialogIsClosing();
};

#endif // I2POPTIONSDIALOG_H
