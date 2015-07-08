// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "anoncoingui.h"

#include "anoncoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "rpcconsole.h"
#include "utilitydialog.h"
#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#endif

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "ui_interface.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QObject>
#include <QPoint>
#include <QProgressBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef ENABLE_I2PSAM
#include "i2pshowaddresses.h"
#endif // ENABLE_I2PSAM

#if QT_VERSION < 0x050000
#include <QUrl>
#include <QTextDocument>
#else
#include <QUrlQuery>
#endif

const QString AnoncoinGUI::DEFAULT_WALLET = "~Default";

AnoncoinGUI::AnoncoinGUI(bool fIsTestnet, QWidget *parent) :
    QMainWindow(parent),
    clientModel(0),
    walletFrame(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    prevBlocks(0),
    spinnerFrame(0)
{
    GUIUtil::restoreWindowGeometry("nWindow", QSize(850, 550), this);

    QString windowTitle = tr("Anoncoin Core") + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    bool enableWallet = !GetBoolArg("-disablewallet", false);
#else
    bool enableWallet = false;
#endif
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }

    if (!fIsTestnet)
    {
#ifndef Q_OS_MAC
        QApplication::setWindowIcon(QIcon(":/icons/anoncoin"));
        setWindowIcon(QIcon(":/icons/anoncoin"));
#else
        MacDockIconHandler::instance()->setIcon(QIcon(":/icons/anoncoin"));
#endif
    }
    else
    {
        windowTitle += " " + tr("[testnet]");
#ifndef Q_OS_MAC
        QApplication::setWindowIcon(QIcon(":/icons/anoncoin_testnet"));
        setWindowIcon(QIcon(":/icons/anoncoin_testnet"));
#else
        MacDockIconHandler::instance()->setIcon(QIcon(":/icons/anoncoin_testnet"));
#endif
    }
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(enableWallet ? this : 0);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(this);
        setCentralWidget(walletFrame);
    } else
#endif
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Default search path for OS icons
    // ... if we ever use Qt themes
    QStringList my_icon_paths = QIcon::themeSearchPaths();
    QString ddDir = QDir::fromNativeSeparators( QString::fromStdString ( GetDataDir().string() ) );
    QString themeDir = ddDir + "/themes";
    my_icon_paths.prepend( themeDir );
    QIcon::setThemeSearchPaths(my_icon_paths);

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions(fIsTestnet);

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(fIsTestnet);

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    unitDisplayControl = new UnitDisplayStatusBarControl();
    labelEncryptionIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
#ifdef ENABLE_I2PSAM
    labelI2PConnections = new QLabel();
    labelI2POnly = new QLabel();
    labelI2PGenerated = new QLabel();

    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelI2PGenerated);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelI2POnly);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelI2PConnections);
#endif
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(unitDisplayControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setProperty("windowsstyle", true);
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // prevents an oben debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();
}

AnoncoinGUI::~AnoncoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::instance()->setMainWindow(NULL);
#endif
}

void AnoncoinGUI::createActions(bool fIsTestnet)
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    accountsAction = new QAction(QIcon(":/icons/accounts"), tr("&Accounts"), this);
    accountsAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));
    accountsAction->setToolTip(accountsAction->statusTip());
    accountsAction->setCheckable(true);
    accountsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(accountsAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Anoncoin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and anoncoin: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(historyAction);

    addressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    accountsAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    addressesAction->setToolTip(addressesAction->statusTip());
    addressesAction->setCheckable(true);
    addressesAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(addressesAction);

    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressesAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressesAction, SIGNAL(triggered()), walletFrame, SLOT(gotoAddressBookPage()));
    connect(accountsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(accountsAction, SIGNAL(triggered()), walletFrame, SLOT(gotoAccountsPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    if (!fIsTestnet)
        aboutAction = new QAction(QIcon(":/icons/anoncoin"), tr("&About Anoncoin Core"), this);
    else
        aboutAction = new QAction(QIcon(":/icons/anoncoin_testnet"), tr("&About Anoncoin Core"), this);
    aboutAction->setStatusTip(tr("Show information about Anoncoin"));
    aboutAction->setMenuRole(QAction::AboutRole);
#if QT_VERSION < 0x050000
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
    aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for Anoncoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    if (!fIsTestnet)
        toggleHideAction = new QAction(QIcon(":/icons/anoncoin"), tr("&Show / Hide"), this);
    else
        toggleHideAction = new QAction(QIcon(":/icons/anoncoin_testnet"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);

    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Anoncoin addresses to prove you own them"));
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Anoncoin addresses"));

    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));

    usedSendingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));
    openAction = new QAction(QIcon(":/icons/fileopen"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a anoncoin: URI or payment request"));

    showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
    showHelpMessageAction->setStatusTip(tr("Show the Anoncoin Core help message to get a list with possible Anoncoin command-line options"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
    }
#endif
}

void AnoncoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    if(walletFrame)
    {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void AnoncoinGUI::createToolBars()
{
    if(walletFrame)
    {
        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        toolbar->setObjectName("maintoolbar");
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        QToolButton * tbButton;

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonOverview");
        tbButton->setDefaultAction(overviewAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonAccounts");
        tbButton->setDefaultAction(accountsAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonSendCoins");
        tbButton->setDefaultAction(sendCoinsAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonReceiveCoins");
        tbButton->setDefaultAction(receiveCoinsAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonHistory");
        tbButton->setDefaultAction(historyAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        tbButton = new QToolButton();
        tbButton->setObjectName("toolbuttonAddresses");
        tbButton->setDefaultAction(addressesAction);
        tbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addWidget(tbButton);

        overviewAction->setChecked(true);
    }
}

void AnoncoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

#ifdef ENABLE_I2PSAM
        setNumI2PConnections(clientModel->getNumI2PConnections());
        connect(clientModel, SIGNAL(numI2PConnectionsChanged(int)), this, SLOT(setNumI2PConnections(int)));

        if (clientModel->isI2POnly()) {
            labelI2POnly->setText("I2P");
            labelI2POnly->setToolTip(tr("Wallet is using I2P-network only!"));
        }
        else if (clientModel->isTorOnly()) {
            labelI2POnly->setText("TOR");
            labelI2POnly->setToolTip(tr("Wallet is using Tor-network only"));
        }
        else if (clientModel->isDarknetOnly()) {
            labelI2POnly->setText("I&T");
            labelI2POnly->setToolTip(tr("Wallet is using I2P and Tor networks (Darknet mode)"));
        }
        else if (clientModel->isBehindDarknet()) {
            labelI2POnly->setText("ICT");
            labelI2POnly->setToolTip(tr("Wallet is using I2P and Tor networks, also Tor as a proxy"));
        }
        else {
            labelI2POnly->setText("CLR");
            labelI2POnly->setToolTip(tr("Wallet is using mixed or non-I2P (clear) network"));
        }

        if (clientModel->isI2PAddressGenerated()) {
            labelI2PGenerated->setText("DYN");
            labelI2PGenerated->setToolTip(tr("Wallet is running with a random generated I2P-address"));
        }
        else {
            labelI2PGenerated->setText("STA");
            labelI2PGenerated->setToolTip(tr("Wallet is running with a static I2P-address"));
        }
#endif // ENABLE_I2PSAM

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        rpcConsole->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(clientModel);
        }
#endif

        this->unitDisplayControl->setOptionsModel(clientModel->getOptionsModel());
    }
}

#ifdef ENABLE_WALLET
bool AnoncoinGUI::addWallet(const QString& name, WalletModel *walletModel)
{
    if(!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool AnoncoinGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void AnoncoinGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif

void AnoncoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    accountsAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    addressesAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void AnoncoinGUI::createTrayIcon(bool fIsTestnet)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);

    if (!fIsTestnet)
    {
        trayIcon->setToolTip(tr("Anoncoin client"));
        trayIcon->setIcon(QIcon(":/icons/toolbar"));
    }
    else
    {
        trayIcon->setToolTip(tr("Anoncoin client") + " " + tr("[testnet]"));
        trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
    }

    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void AnoncoinGUI::createTrayIconMenu()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void AnoncoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void AnoncoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this);
    dlg.setModel(clientModel->getOptionsModel());
#ifdef ENABLE_I2PSAM
    dlg.setClientModel(clientModel);
#endif
    dlg.exec();
}

void AnoncoinGUI::aboutClicked()
{
    if(!clientModel)
        return;

    AboutDialog dlg(this);
    dlg.setModel(clientModel);
    dlg.exec();
}

void AnoncoinGUI::showHelpMessageClicked()
{
    HelpMessageDialog *help = new HelpMessageDialog(this);
    help->setAttribute(Qt::WA_DeleteOnClose);
    help->show();
}

#ifdef ENABLE_WALLET
void AnoncoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        emit receivedURI(dlg.getURI());
    }
}

void AnoncoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void AnoncoinGUI::gotoAccountsPage()
{
    accountsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoAccountsPage();
}

void AnoncoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void AnoncoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void AnoncoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void AnoncoinGUI::gotoAddressBookPage()
{
    addressesAction->setEnabled(true);
    if (walletFrame) walletFrame->gotoAddressBookPage();
}

void AnoncoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void AnoncoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
#endif

void AnoncoinGUI::setNumConnections(int count)
{
    QString icon;
#ifdef ENABLE_I2PSAM
    // Real count is minus the i2p connections we're showing that in another icon
    int realcount = count - i2pConnectCount;
    // ToDo: Having a bug here while testing I2P onlynet and binding my port in the anoncoin.conf file to 192.168.xx.xx.
    // The realcount is getting a negative value for awhile, if nodes are found pretty fast on startup, not sure why.
    // Added conditional below, so the user doesn't see that at least
    if( realcount < 0 ) realcount = 0;
#else
    int realcount = count;
#endif
    switch(realcount)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Anoncoin clearnet peers", "", realcount));
}

#ifdef ENABLE_I2PSAM
void AnoncoinGUI::setNumI2PConnections(int count)
{
    QString i2pIcon;
    // See the anoncoin.qrc file for icon files below & their associated alias name
    switch(count) {
    case 0: i2pIcon = ":/icons/i2pconnect_0"; break;
    case 1: case 2: case 3: i2pIcon = ":/icons/i2pconnect_1"; break;
    case 4: case 5: case 6: i2pIcon = ":/icons/i2pconnect_2"; break;
    case 7: case 8: case 9: i2pIcon = ":/icons/i2pconnect_3"; break;
    default: i2pIcon = ":/icons/i2pconnect_4"; break;
    }
    labelI2PConnections->setPixmap(QIcon(i2pIcon).pixmap(4*STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelI2PConnections->setToolTip(tr("%n active connection(s) to I2P-Anoncoin network", "", count));
    i2pConnectCount = count;
}
#endif // ENABLE_I2PSAM

void AnoncoinGUI::setNumBlocks(int count)
{
    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            progressBarLabel->setText(tr("Synchronizing with network..."));
            break;
        case BLOCK_SOURCE_DISK:
            progressBarLabel->setText(tr("Importing blocks from disk..."));
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            // Case: not Importing, not Reindexing and no network connection
            progressBarLabel->setText(tr("No block source available..."));
            break;
    }

    QString tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = lastBlockDate.secsTo(currentDate);

    tooltip = tr("Processed %1 blocks of transaction history.").arg(count);

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
            walletFrame->showOutOfSyncWarning(false);
#endif

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        // Represent time from last generated block in human readable text
        QString timeBehindText;
        const int HOUR_IN_SECONDS = 60*60;
        const int DAY_IN_SECONDS = 24*60*60;
        const int WEEK_IN_SECONDS = 7*24*60*60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if(secs < 2*DAY_IN_SECONDS)
        {
            timeBehindText = tr("%n hour(s)","",secs/HOUR_IN_SECONDS);
        }
        else if(secs < 2*WEEK_IN_SECONDS)
        {
            timeBehindText = tr("%n day(s)","",secs/DAY_IN_SECONDS);
        }
        else if(secs < YEAR_IN_SECONDS)
        {
            timeBehindText = tr("%n week(s)","",secs/WEEK_IN_SECONDS);
        }
        else
        {
            int years = secs / YEAR_IN_SECONDS;
            int remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)","", remainder/WEEK_IN_SECONDS));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress() * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(QIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
            walletFrame->showOutOfSyncWarning(true);
#endif

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void AnoncoinGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("Anoncoin"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "Anoncoin - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        // Ensure we get users attention, but only if main window is visible
        // as we don't want to pop up the main window for messages that happen before
        // initialization is finished.
        if(!(style & CClientUIInterface::NOSHOWGUI))
            showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void AnoncoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void AnoncoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            QApplication::quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

#ifdef ENABLE_WALLET
void AnoncoinGUI::incomingTransaction(const QString& date, int unit, qint64 amount, const QString& type, const QString& address)
{
    // On new transaction, make an info balloon
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             tr("Date: %1\n"
                "Amount: %2\n"
                "Type: %3\n"
                "Address: %4\n")
                  .arg(date)
                  .arg(AnoncoinUnits::formatWithUnit(unit, amount, true))
                  .arg(type)
                  .arg(address), CClientUIInterface::MSG_INFORMATION);
}
#endif

void AnoncoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void AnoncoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        foreach(const QUrl &uri, event->mimeData()->urls())
        {
            emit receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool AnoncoinGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    if(event->type() == QEvent::Paint)
    {
        // Needed to keep the stylesheet tab icons from being overwritten
        if(clientModel)
            clientModel->getOptionsModel()->applyTheme();
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool AnoncoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    else
        return false;
}

void AnoncoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif

void AnoncoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void AnoncoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void AnoncoinGUI::detectShutdown()
{
    if (ShutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

static bool ThreadSafeMessageBox(AnoncoinGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

#ifdef ENABLE_I2PSAM
// ToDo: Check me
// This code ported from 0.8.5.6 didn't work as is, had to rework (see the anoncoin.cpp file in old codebase). Removed all the printf's and msg strings,
// they should go somewhere else (see noui.cpp for anoncoind operation), from what I (GR) could tell they would never have executed.
// The 'guiref' value (old code) here is called 'gui', changed that.  Need to confirm this is a blockingGUI thread.....Think I've fixed this.
// This function now returns a true/false value, that was changed here, but never crossed checked that the caller would process such a thing...
// Ok think I got all the issues addressed and ready for testing...
static bool ThreadSafeShowGeneratedI2PAddress(AnoncoinGUI *gui,
                                              const std::string& caption,
                                              const std::string& pub,
                                              const std::string& priv,
                                              const std::string& b32,
                                              const std::string& configFileName)
{
    bool ret = false;

//    ShowI2PAddresses i2pAddrDialog( QString::fromStdString(caption),
//                                    QString::fromStdString(pub),
//                                    QString::fromStdString(priv),
//                                    QString::fromStdString(b32),
//                                    QString::fromStdString(configFileName),
//                                    gui );
// Latest thoughts,  we need to get the clientmodel for a gui parent, and use new to create the dialog here now...

//    i2pAddrDialog.show();
    ret = true;
//    unsigned int style = CClientUIInterface::BTN_ABORT | CClientUIInterface::MSG_INFORMATION;
//    bool modal = (style & CClientUIInterface::MODAL);

//    QString pubkey = QString::fromStdString(pub);
//    QString privkey = QString::fromStdString(priv);

    // http://qt-project.org/doc/qt-4.8/qmetaobject.html
    // Invokes the member (a signal or a slot name) on the object obj. Returns true if the member could be invoked.
    // Returns false if there is no such member or the parameters did not match.
    // If type is Qt::BlockingQueuedConnection, the method will be invoked in the same way as for Qt::QueuedConnection,
    // except that the current thread will block until the event is delivered. Using this connection type to communicate
    // between objects in the same thread will lead to deadlocks....
//    QMetaObject::invokeMethod(gui, "message",
//                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
//                               Q_ARG(QString, QString::fromStdString(caption)),
//                               pubkey,
//                               privkey,
//                               Q_ARG(QString, QString::fromStdString(b32)),
//                               Q_ARG(QString, QString::fromStdString(configFileName)),
//                               Q_ARG(unsigned int, style),
//                               Q_ARG(bool*, &ret));

    return ret;
}
#endif // ENABLE_I2PSAM

void AnoncoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
#ifdef ENABLE_I2PSAM
    uiInterface.ThreadSafeShowGeneratedI2PAddress.connect(boost::bind(ThreadSafeShowGeneratedI2PAddress, this, _1, _2, _3, _4, _5));
#endif
}

void AnoncoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
#ifdef ENABLE_I2PSAM
    uiInterface.ThreadSafeShowGeneratedI2PAddress.disconnect(boost::bind(ThreadSafeShowGeneratedI2PAddress, this, _1, _2, _3, _4, _5));
#endif
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl():QLabel()
{
    optionsModel = 0;
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
}

/** So that it responds to left-button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu();
    foreach(AnoncoinUnits::Unit u, AnoncoinUnits::availableUnits())
    {
        QAction *menuAction = new QAction(QString(AnoncoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu,SIGNAL(triggered(QAction*)),this,SLOT(onMenuSelection(QAction*)));

    // what happens on right click.
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this,SIGNAL(customContextMenuRequested(const QPoint&)),this,SLOT(onDisplayUnitsClicked(const QPoint&)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *optionsModel)
{
    if (optionsModel)
    {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel,SIGNAL(displayUnitChanged(int)),this,SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(AnoncoinUnits::name(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}

#ifdef ENABLE_I2PSAM
void AnoncoinGUI::showGeneratedI2PAddr(const QString& caption, const QString& pub, const QString& priv, const QString& b32, const QString& configFileName)
{
    ShowI2PAddresses i2pDialog(caption, pub, priv, b32, configFileName, this);
    i2pDialog.exec();
}
#endif // ENABLE_I2PSAM
