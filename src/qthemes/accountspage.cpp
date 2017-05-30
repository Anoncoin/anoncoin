// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accountspage.h"
#include "ui_accountspage.h"

#include "anoncoingui.h"
#include "csvmodelwriter.h"
#include "util.h"
#include "guiutil.h"

#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>

AccountsPage::AccountsPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AccountsPage),
    model(0)
{
    ui->setupUi(this);
    tableWidget = ui->tableWidget;

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->copyAddress->setIcon(QIcon());
    ui->exportButton->setIcon(QIcon());
#endif

    setWindowTitle(tr("Accounts"));
    connect(ui->tableWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
    ui->tableWidget->setFocus();
    ui->closeButton->setText(tr("C&lose"));
    ui->exportButton->hide();
    ui->labelExplanation->setText(tr("These are your Anoncoin addresses for receiving payments."));

    // Context menu actions
    QAction *copyAddressAction = new QAction(tr("&Copy Address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyBalanceAction = new QAction(tr("Copy &Balance"), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyBalanceAction);
    contextMenu->addSeparator();

    // Connect signals for actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyAddress_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(copyBalanceAction, SIGNAL(triggered()), this, SLOT(onCopyBalanceAction()));
    connect(ui->tableWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(accept()));
    if(parent == 0)
    {
        ui->closeButton->hide();
    }
}

AccountsPage::~AccountsPage()
{
    delete ui;
}

void AccountsPage::setModel(WalletModel *model)
{
    this->model = model;
    QStringList horzHeaders;
    horzHeaders << tr("Name") << tr("Account") << tr("Balance");
    ui->tableWidget->setHorizontalHeaderLabels( horzHeaders );
    ui->tableWidget->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
#if QT_VERSION < 0x050000
    ui->tableWidget->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setResizeMode(3, QHeaderView::ResizeToContents);
#else
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
#endif

    connect(ui->tableWidget->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged()));

    selectionChanged();
}

void AccountsPage::on_copyAddress_clicked()
{
    GUIUtil::copyEntryData(ui->tableWidget, 1);
}

void AccountsPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableWidget, 2);
}

void AccountsPage::onCopyBalanceAction()
{
    GUIUtil::copyEntryData(ui->tableWidget, 3);
}

void AccountsPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableWidget *tableW = ui->tableWidget;
    if(!tableW->selectionModel())
        return;

    if(tableW->selectionModel()->hasSelection())
    {
        ui->copyAddress->setEnabled(true);
    }
    else
    {
        ui->copyAddress->setEnabled(false);
    }
}

void AccountsPage::done(int retval)
{
    QTableWidget *tableW = ui->tableWidget;
    if(!tableW->selectionModel())
        return;

    // Figure out which address was selected, and return it
    QModelIndexList indexes = tableW->selectionModel()->selectedRows(2);

    foreach (QModelIndex index, indexes)
    {
        QVariant address = tableW->model()->data(index);
        returnValue = address.toString();
    }

    if(returnValue.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

void AccountsPage::on_exportButton_clicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Export Address List"), QString(),
        tr("Comma separated file (*.csv)"), NULL);

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.addColumn("Label", 1, Qt::EditRole);
    writer.addColumn("Address", 2, Qt::EditRole);
    writer.addColumn("Balance", 3, Qt::EditRole);

    if(!writer.write()) {
        QMessageBox::critical(this, tr("Exporting Failed"),
            tr("There was an error trying to save the address list to %1.").arg(filename));
    }
}

void AccountsPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableWidget->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}
