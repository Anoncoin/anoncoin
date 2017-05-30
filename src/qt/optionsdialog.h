// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

// Many builder specific things set in the config file, for any source files where we rely on moc_xxx files being generated
// it is best to include the anoncoin-config.h in the header file itself.  Not the .cpp src file, because otherwise any
// conditional compilation guidelines, which rely on the build configuration, will not be present in the moc_xxx files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include <QDialog>

class MonitoredDataMapper;
class OptionsModel;
class QValidatedLineEdit;

namespace Ui {
class OptionsDialog;
}

/** Preferences dialog. */
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent, bool enableWallet);
    ~OptionsDialog();

    void setModel(OptionsModel *model);
    void setMapper();

protected:
    bool eventFilter(QObject *object, QEvent *event);

private Q_SLOTS:
    /* enable OK button */
    void enableOkButton();
    /* disable OK button */
    void disableOkButton();
    /* set OK button state (enabled / disabled) */
    void setOkButtonState(bool fState);
    void on_resetButton_clicked();
    void on_okButton_clicked();
    void on_cancelButton_clicked();

    void showRestartWarning(bool fPersistent = false);
    void clearStatusLabel();
    void updateDisplayUnit();
    void doProxyIpChecks(QValidatedLineEdit *pUiProxyIp, int nProxyPort);

Q_SIGNALS:
    void proxyIpChecks(QValidatedLineEdit *pUiProxyIp, int nProxyPort);

private:
    Ui::OptionsDialog *ui;
    OptionsModel *model;
    MonitoredDataMapper *mapper;
    bool fProxyIpValid;
};

#endif // OPTIONSDIALOG_H
