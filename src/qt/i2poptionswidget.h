// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef I2POPTIONSWIDGET_H
#define I2POPTIONSWIDGET_H

#include <QDialog>

class MonitoredDataMapper;

namespace Ui {
class I2POptionsWidget;
}

class ClientModel;

class I2POptionsWidget : public QDialog
{
    Q_OBJECT

public:
    explicit I2POptionsWidget(QWidget *parent = 0);
    ~I2POptionsWidget();
    void UpdateParameters();

    void setMapper(MonitoredDataMapper& mapper);
    void reject();

private:
    Ui::I2POptionsWidget *ui;

private Q_SLOTS:
//    void ShowCurrentI2PAddress();
//    void GenerateNewI2PAddress();
    void WriteToDataFile();
    void settingsModified();
    void onCloseDialog();


Q_SIGNALS:
//    void settingsChanged();
    void dialogIsClosing();
};

#endif // I2POPTIONSWIDGET_H
