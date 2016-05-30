// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Many builder specific things set in the config file, don't see a need for it here, still better to not forget to include it in your source files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include <QObject>
#include <QDialog>

#include "i2poptionswidget.h"
#include "ui_i2poptionswidget.h"

#include "optionsmodel.h"
#include "monitoreddatamapper.h"
#include "i2pshowaddresses.h"
#include "util.h"
#include "guiutil.h"

#include "i2pmanager.h"

bool changesAreDirty = false;

I2POptionsWidget::I2POptionsWidget(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::I2POptionsWidget)//,
    //clientModel(0)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonCurrentI2PAddress,  SIGNAL(clicked()), this, SLOT(ShowCurrentI2PAddress()));
    QObject::connect(ui->pushButtonSaveI2PSettings, SIGNAL(clicked()), this, SLOT(WriteToDataFile()));

    QObject::connect(ui->checkBoxAllowZeroHop         , SIGNAL(stateChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->checkBoxInboundAllowZeroHop  , SIGNAL(stateChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->checkBoxUseI2POnly           , SIGNAL(stateChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->lineEditSAMHost              , SIGNAL(textChanged(QString)), this, SLOT(settingsModified()));
    QObject::connect(ui->lineEditTunnelName           , SIGNAL(textChanged(QString)), this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxInboundBackupQuality  , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxInboundIPRestriction  , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxInboundLength         , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxInboundLengthVariance , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxInboundQuantity       , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundBackupQuantity, SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundIPRestriction , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundLength        , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundLengthVariance, SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundPriority      , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxOutboundQuantity      , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));
    QObject::connect(ui->spinBoxSAMPort               , SIGNAL(valueChanged(int))   , this, SLOT(settingsModified()));

    QObject::connect(this, SIGNAL(accepted()), this, SLOT(onCloseDialog()));
    QObject::connect(this, SIGNAL(rejected()), this, SLOT(onCloseDialog()));
    QObject::connect(this, SIGNAL(dialogIsClosing()), this, SLOT(onCloseDialog()));
}

I2POptionsWidget::~I2POptionsWidget()
{
    delete ui;
}

void I2POptionsWidget::setMapper(MonitoredDataMapper& mapper)
{
    mapper.addMapping(ui->checkBoxUseI2POnly           , OptionsModel::eI2PUseI2POnly);
    mapper.addMapping(ui->lineEditSAMHost              , OptionsModel::eI2PSAMHost);
    mapper.addMapping(ui->spinBoxSAMPort               , OptionsModel::eI2PSAMPort);
    mapper.addMapping(ui->lineEditTunnelName           , OptionsModel::eI2PSessionName);
    mapper.addMapping(ui->spinBoxInboundQuantity       , OptionsModel::I2PInboundQuantity);
    mapper.addMapping(ui->spinBoxInboundLength         , OptionsModel::I2PInboundLength);
    mapper.addMapping(ui->spinBoxInboundLengthVariance , OptionsModel::I2PInboundLengthVariance);
    mapper.addMapping(ui->spinBoxInboundBackupQuality  , OptionsModel::I2PInboundBackupQuantity);
    mapper.addMapping(ui->checkBoxInboundAllowZeroHop  , OptionsModel::I2PInboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxInboundIPRestriction  , OptionsModel::I2PInboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundQuantity      , OptionsModel::I2POutboundQuantity);
    mapper.addMapping(ui->spinBoxOutboundLength        , OptionsModel::I2POutboundLength);
    mapper.addMapping(ui->spinBoxOutboundLengthVariance, OptionsModel::I2POutboundLengthVariance);
    mapper.addMapping(ui->spinBoxOutboundBackupQuantity, OptionsModel::I2POutboundBackupQuantity);
    mapper.addMapping(ui->checkBoxAllowZeroHop         , OptionsModel::I2POutboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxOutboundIPRestriction , OptionsModel::I2POutboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundPriority      , OptionsModel::I2POutboundPriority);
}
/*
void I2POptionsWidget::setModel(ClientModel* model)
{
    clientModel = model;
}
*/

void I2POptionsWidget::settingsModified()
{
    ui->pushButtonSaveI2PSettings->setEnabled(true);

    changesAreDirty = true;
}

void I2POptionsWidget::UpdateParameters()
{
    //nothing yet...but soon will load from key files
    //ui->checkBoxAllowZeroHop->setText(ui->checkBoxAllowZeroHop->text() + QString::fromStdString("DID YA SET THE ZEROHOP?"));
}

void I2POptionsWidget::onCloseDialog()
{
    ui->pushButtonSaveI2PSettings->setEnabled(false);
    changesAreDirty = false;
}

//void I2POptionsWidget::reject(){
void I2POptionsWidget::reject()
{
    if (changesAreDirty)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, "I2P Settings",
                                                                tr("Are you sure?\n"),
                                                                QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
                                                                QMessageBox::Yes);

        if (resBtn == QMessageBox::Yes)
        {
            //emit dialogIsClosing();
            QDialog::accept();
        }
    }
    else
    {
        //emit dialogIsClosing();
        QDialog::accept();
    }
}
/*
void I2POptionsWidget::ShowCurrentI2PAddress()
{
    if (clientModel)
    {
        const QString pub = clientModel->getPublicI2PKey();
        const QString priv = clientModel->getPrivateI2PKey();
        const QString b32 = clientModel->getB32Address(pub);
        const QString configFile = QString::fromStdString(GetConfigFile().string());

        //ShowI2PAddresses i2pCurrDialog("Your current I2P-address", pub, priv, b32, configFile, this);
        //i2pCurrDialog.exec();
    }
}
*/
void I2POptionsWidget::WriteToDataFile()
{
    if(true)
    {
        ui->pushButtonSaveI2PSettings->setEnabled(false);
        QMessageBox::information(0, QObject::tr("I2P Settings Manager"),
                              QObject::tr("Successfully wrote setings to I2P data file!") );
    }
}
