// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACCOUNTSPAGE_H
#define ACCOUNTSPAGE_H

#include <QDialog>

class OptionsModel;
class WalletModel;
class QTableWidget;

QT_BEGIN_NAMESPACE
class QItemSelection;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class AccountsPage;
}

/** Widget that shows a list of sending or receiving addresses.
  */
class AccountsPage : public QDialog
{
    Q_OBJECT

public:
    explicit AccountsPage(QWidget *parent = 0);
    ~AccountsPage();
    const QString &getReturnValue() const { return returnValue; }
    void setModel(WalletModel *model);
    QTableWidget *tableWidget;

public slots:
    void done(int retval);

private:
    Ui::AccountsPage *ui;
    WalletModel *model;
    QString returnValue;
    QMenu *contextMenu;

private slots:
    /** Copy address of currently selected address entry to clipboard */
    void on_copyAddress_clicked();
    /** Copy label of currently selected address entry to clipboard (no button) */
    void onCopyLabelAction();
    /** Copy balance of currently selected address entry to clipboard (no button) */
    void onCopyBalanceAction();
    /** Export button clicked */
    void on_exportButton_clicked();

    /** Set button states based on selected tab and selection */
    void selectionChanged();
    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);

signals:
    void sendCoins(QString addr);
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);
};

#endif // ACCOUNTSPAGE_H
