// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef I2POPTIONSWIDGET_H
#define I2POPTIONSWIDGET_H

#include <QWidget>

class MonitoredDataMapper;

namespace Ui {
class I2POptionsWidget;
}

class ClientModel;

class I2POptionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit I2POptionsWidget(QWidget *parent = 0);
    ~I2POptionsWidget();

    void setMapper(MonitoredDataMapper& mapper);
    void setModel(ClientModel* model);

private:
    Ui::I2POptionsWidget *ui;
    ClientModel* clientModel;

private slots:
    void ShowCurrentI2PAddress();
    void GenerateNewI2PAddress();

signals:
    void settingsChanged();
};

#endif // I2POPTIONSWIDGET_H
