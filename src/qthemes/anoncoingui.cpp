// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
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
#include "walletaddviewdialog.h"
#include "walletclearviewsdialog.h"
#include "walletrestartcoredialog.h"
#include "walletupdateviewdialog.h"

#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "ui_interface.h"

#include <iostream>

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QIcon>
#include <QKeySequence>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QObject>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>


#ifdef ENABLE_I2PSAM
#include "i2pshowaddresses.h"
#endif

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
    unitDisplayControl(0),
    labelEncryptionIcon(0),
    labelConnectionsIcon(0),
    labelBlocksIcon(0),
    progressBarLabel(0),
    progressBar(0),
    progressDialog(0),
    appMenuBar(0),
    appToolBar(0),
    overviewAction(0),
    historyAction(0),
    quitAction(0),
    sendCoinsAction(0),
    usedSendingAddressesAction(0),
    usedReceivingAddressesAction(0),
    signMessageAction(0),
    verifyMessageAction(0),
    aboutAction(0),
    receiveCoinsAction(0),
    optionsAction(0),
    toggleHideAction(0),
    encryptWalletAction(0),
    backupWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    openRPCConsoleAction(0),
    openAction(0),
    showHelpMessageAction(0),
    trayIcon(0),
//    trayIconMenu(0),
    notificator(0),
    rpcConsole(0),
    prevBlocks(0),
    spinnerFrame(0)
#ifdef ENABLE_I2PSAM
    ,i2pAddress(0)
#endif
{
    GUIUtil::restoreWindowGeometry("nWindow", QSize(850, 550), this);
    fShutdownInProgress = false;

    QString windowTitle = tr("Anoncoin Core") + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET
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
    i2pAddress = new ShowI2PAddresses( this );

#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

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
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelEncryptionIcon);
    }
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

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

#ifdef ENABLE_I2PSAM
    // Do the same for the I2P Destination details window
    connect(openI2pAddressAction, SIGNAL(triggered()), i2pAddress, SLOT(show()));
    connect(quitAction, SIGNAL(triggered()), i2pAddress, SLOT(hide()));
#endif

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

    //! Todo: Improve this, hiding it is not a good solution, it should be removed before our mainwindow is destroyed
    if( notificator ) delete notificator;
    // Original Code suggested: Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
    // That is now being done long before MainWindow destruction and addition steps here to destroy it properly
    if(trayIcon) {
//        trayIcon->hide();
        trayIcon->disconnect();
        // trayIcon->cleanup();      Not available in QT 4.6.4
        delete trayIcon;
    }
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::instance()->setMainWindow(NULL);
#endif
}

QMainToolAction::QMainToolAction( const QString& sDefaultIcon, const QString& sDefaultText, const QString& sDefaultTip, const int nShortCutKey, AnoncoinGUI* pGUI ) :
     QAction( pGUI ),
     ourDefaultIcon( sDefaultIcon ),
     nOurKey( nShortCutKey ),
     pMainWindow( pGUI )
{
    strOurName = "Action:" + sDefaultText;  // Used for debugging only
    setIcon( ourDefaultIcon );
    setText( sDefaultText );
    setStatusTip( sDefaultTip );
    setToolTip( sDefaultTip );
    setCheckable( true );
    setShortcut( QKeySequence(Qt::ALT, nShortCutKey) );

    connect(this, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(this, SIGNAL(triggered()), this, SLOT(gotoPage()));
}

bool QMainToolAction::event(QEvent *pEvent)
{
    //! We notify our base class of the event, let it do its thing and then update our copy of the icon, which may have been changed due to style settings.
    if( pEvent->type() == QEvent::ActionChanged ) {
        bool fResult = QAction::event(pEvent);
        // ourStyledIcon = icon();
        // The icon is now set from within the customized toolbutton detection of a style change,
        //  we do not use this here in the action atm, but left the code in so you could know how
        // to find it, if needed.  Also the following debug line could be helpful, the action event
        // never returns any value but false with QT v4.6.4 builds.
        // qDebug() << strOurName << " says Action " << (fResult ? "changed" : "failed to change");
        return true;
    }
    //else
        //qDebug() << strOurName << " says event type " << pEvent->type();

    return QAction::event(pEvent);
}

//! Should this happen, we send our parent the message, so that all aspects of the application can be normalized.
void QMainToolAction::showNormalIfMinimized()
{
    pMainWindow->showNormalIfMinimized();
}

void QMainToolAction::gotoPage()
{
    //! We tell our parent about this new request, using the Short cut key as the index to identify whom we are.
    pMainWindow->gotoPage( nOurKey );
}

void AnoncoinGUI::createActions(bool fIsTestnet)
{
    //! Initialize the primary toolbar and button actions used for selecting the different ways we can view wallet details.
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QMainToolAction( ":/icons/overview", tr("&Overview"), tr("Show general overview of wallet"), Qt::Key_1, this );
    tabGroup->addAction( overviewAction );

    accountsAction = new QMainToolAction( ":/icons/accounts", tr("&Accounts"), tr("Show the list of used receiving addresses and labels"), Qt::Key_2, this );
    tabGroup->addAction(accountsAction);

    sendCoinsAction = new QMainToolAction( ":/icons/send", tr("&Send"), tr("Send coins to a Anoncoin address"), Qt::Key_3, this );
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QMainToolAction( ":/icons/receiving_addresses", tr("&Receive"), tr("Request payments (generates QR codes and anoncoin: URIs)"), Qt::Key_4, this );
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QMainToolAction( ":/icons/history", tr("&Transactions"), tr("Browse transaction history"), Qt::Key_5, this );
    tabGroup->addAction(historyAction);

    addressesAction = new QMainToolAction( ":/icons/address-book", tr("&Address Book"), tr("Show the list of used sending addresses and labels"), Qt::Key_6, this );
    tabGroup->addAction(addressesAction);

    //! Finish creating all possible menu and MainWindow Actions
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

    openI2pAddressAction = new QAction(QIcon(":/icons/options"), tr("&I2P Destination details"), this);
    openI2pAddressAction->setStatusTip(tr("Shows your private I2P Destination details"));

    usedSendingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));

    usedReceivingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(QIcon(":/icons/fileopen"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a anoncoin: URI or payment request"));

    showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the Anoncoin Core help message to get a list with possible Anoncoin command-line options"));

    pWalletAddViewAction = new QAction( QIcon(":/icons/eye_plus"), tr("Add a Wallet View..."), this );
    pWalletAddViewAction->setStatusTip( tr("MultiWallet support for Anoncoin is in development") );

    pWalletClearViewsAction = new QAction( QIcon(":/icons/eye_plus"), tr("Clears your Wallet Views..."), this );
    pWalletClearViewsAction->setStatusTip( tr("MultiWallet support for Anoncoin is in development") );

    pWalletUpdateViewAction = new QAction( QIcon(":/icons/eye_plus"), tr("Update a Wallet View..."), this );
    pWalletUpdateViewAction->setStatusTip( tr("MultiWallet support for Anoncoin is in development") );

    pWalletRestartCoreAction = new QAction( QIcon(":/icons/eye_plus"), tr("Restart Core with Wallet..."), this );
    pWalletRestartCoreAction->setStatusTip( tr("MultiWallet support for Anoncoin is in development") );

    //! Connect up all signals and slots that are triggered when actions are selected. NOTE: This has already been done for Toolbar button actions
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

        connect(pWalletAddViewAction, SIGNAL(triggered()), this, SLOT(walletAddViewClicked()));
        connect(pWalletClearViewsAction, SIGNAL(triggered()), this, SLOT(walletClearViewsClicked()));
        connect(pWalletRestartCoreAction, SIGNAL(triggered()), this, SLOT(walletRestartCoreClicked()));
        connect(pWalletUpdateViewAction, SIGNAL(triggered()), this, SLOT(walletUpdateViewClicked()));
    }
#endif // ENABLE_WALLET
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
        file->addSeparator();
        file->addAction(pWalletAddViewAction);
        file->addAction(pWalletClearViewsAction);
        file->addAction(pWalletUpdateViewAction);
        file->addAction(pWalletRestartCoreAction);
        file->addAction(backupWalletAction);
        file->addSeparator();
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
#ifdef ENABLE_I2PSAM
    settings->addAction(openI2pAddressAction);
#endif
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

QMainToolButton::QMainToolButton( const QString& sNameIn, QMainToolAction* pActionIn, AnoncoinGUI* pGUI) :
     QToolButton( pGUI ),
     strOurName( sNameIn ),
     pAction( pActionIn ),
     pMainWindow( pGUI )
{
    setObjectName(strOurName);
    setDefaultAction(pAction);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
}

bool QMainToolButton::event(QEvent *pEvent)
{
    if( pEvent->type() == QEvent::StyleChange ) {
        // qDebug() << strOurName << " wanted to let you know, we received a StyleChange event.";
        //!
        //! This is the magic lines so hard fought for, how and where was the correct icon
        //! being displayed from initially?  After a theme change, the tab buttons always
        //! showed correctly at first, then reverted to the default icon once clicked on.
        //!
        //! Until this next line of code was identified, theme selection changes that tried
        //! to modify the button icons could not be done.  A new permanent action icon
        //! replacement had to be found, set and stored for user selected theme changes.
        //!
        //! This code now builds without error and linked as shared against a QT v4.6.4 library,
        //! runs on Linux using QT v4.8 runtime and on Windows builds using a static QT v5.2.1
        //! Mac build testing ToDo:
        //!
        //! During shutdown we get hit with style change events right until the last second, this causes segment faults because
        //! either the icon or setting the action icon has already been destroyed and our pointer is no longer valid.  Whichever
        //! reason, we must watch for and not update here when style change events are happening during shutdown, other approaches
        //! could be used, such as if the widget is not visible or hidden, but the primary reason is because of shutdown.
        if( !pMainWindow->fShutdownInProgress ) {
            QIcon ourIcon = icon();             //! After a stylesheet change, this button icon() is correct, the action icon is not, and still set to default.
            pAction->setIcon( ourIcon );        //! So here we make sure it is set to the right image, then the designers icon 'sticks' as it was suppose to.
        }
    }
    //else
        //qDebug() << strOurName << " says event type " << pEvent->type();

    return QToolButton::event(pEvent);
}

void QMainToolButton::RestoreDefaultIcon()
{
    QIcon& ourIcon = pAction->GetDefaultIcon();
    pAction->setIcon( ourIcon );
}

void AnoncoinGUI::RestoreDefaultIcons()
{
    buttonAccounts->RestoreDefaultIcon();
    buttonAddresses->RestoreDefaultIcon();
    buttonHistory->RestoreDefaultIcon();
    buttonOverview->RestoreDefaultIcon();
    buttonReceiveCoins->RestoreDefaultIcon();
    buttonSendCoins->RestoreDefaultIcon();
#if defined( DONT_COMPILE )
    // ToDo: This code crashes, why?
    QObjectList ObjList = appToolBar->children();
    foreach( const QObject* anObj, ObjList ) {
        QMainToolButton* pButton = (QMainToolButton*)anObj;
        pButton->RestoreDefaultIcon();
    }
#endif
}

void AnoncoinGUI::createToolBars()
{
    //! If we are simply a node, no walletFrame will have been defined, so no toolbar should or need be created or used.
    if(walletFrame) {
        appToolBar = addToolBar(tr("Tabs toolbar"));
        appToolBar->setObjectName("maintoolbar");
        appToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        buttonOverview = new QMainToolButton("toolbuttonOverview",overviewAction, this);
        appToolBar->addWidget(buttonOverview);

        buttonAccounts = new QMainToolButton("toolbuttonAccounts",accountsAction, this);
        appToolBar->addWidget(buttonAccounts);

        buttonSendCoins = new QMainToolButton("toolbuttonSendCoins",sendCoinsAction, this);
        appToolBar->addWidget(buttonSendCoins);

        buttonReceiveCoins = new QMainToolButton("toolbuttonReceiveCoins",receiveCoinsAction, this);
        appToolBar->addWidget(buttonReceiveCoins);

        buttonHistory = new QMainToolButton("toolbuttonHistory",historyAction, this);
        appToolBar->addWidget(buttonHistory);

        buttonAddresses = new QMainToolButton("toolbuttonAddresses",addressesAction, this);
        appToolBar->addWidget(buttonAddresses);

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
        setNumI2PConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumI2PConnections(int)));

        if (clientModel->isI2POnly()) {
            labelI2POnly->setText("I2P");
            labelI2POnly->setToolTip(tr("Node is using I2P-network only!"));
        }
        else if (clientModel->isTorOnly()) {
            labelI2POnly->setText("TOR");
            labelI2POnly->setToolTip(tr("Node is using Tor-network only"));
        }
        else if (clientModel->isDarknetOnly()) {
            labelI2POnly->setText("I&T");
            labelI2POnly->setToolTip(tr("Node is using I2P and Tor networks (Darknet mode)"));
        }
        else if (clientModel->isBehindDarknet()) {
            labelI2POnly->setText("ICT");
            labelI2POnly->setToolTip(tr("Node is using I2P and Tor networks, also Tor as a proxy"));
        }
        else {
            labelI2POnly->setText("CLR");
            labelI2POnly->setToolTip(tr("Node is using mixed or non-I2P (clear) network"));
        }

        if( IsI2PEnabled() ) {
            if( clientModel->isI2PAddressGenerated() ) {
                labelI2PGenerated->setText("DYN");
                labelI2PGenerated->setToolTip(tr("Node is running with a dynamic (random) I2P destination"));
            } else {
                labelI2PGenerated->setText("STA");
                labelI2PGenerated->setToolTip(tr("Node is running with a static I2P destination"));
            }
        } else {
            labelI2PGenerated->setVisible( false );
            labelI2PConnections->setVisible( false );
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
#endif // ENABLE_WALLET

        this->unitDisplayControl->setOptionsModel(clientModel->getOptionsModel());
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
//      if(trayIconMenu)
//      {
            // Disable context menu on tray icon
//          trayIconMenu->clear();
//      }
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
#endif // ENABLE_WALLET

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
    if( QSystemTrayIcon::isSystemTrayAvailable() ) {
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
        notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
    }
#endif
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

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
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

//! We make this a public slot method so that any other section of the software that wishes to select a view can do so,
//! an example of where this is needed is when the user clicks on a transaction in the overview page, an automatic
//! connection is made to focus on that, while switching to the transactions history view through here.
void AnoncoinGUI::gotoPage( const int nKey )
{
    if( walletFrame ) {
        switch( nKey ) {
            case Qt::Key_2 : buttonAccounts->setChecked(true); walletFrame->gotoAccountsPage(); break;
            case Qt::Key_3 : buttonSendCoins->setChecked(true); walletFrame->gotoSendCoinsPage(); break;
            case Qt::Key_4 : buttonReceiveCoins->setChecked(true); walletFrame->gotoReceiveCoinsPage(); break;
            case Qt::Key_5 : buttonHistory->setChecked(true); walletFrame->gotoHistoryPage(); break;
            case Qt::Key_6 : buttonAddresses->setChecked(true); walletFrame->gotoAddressBookPage(); break;
            default: buttonOverview->setChecked(true); walletFrame->gotoOverviewPage();
        }
    }
}
//! When external events want to trigger a toolbar tab button to switch pages, only the setChecked need be done to change the view, such
//! as when the user clicks on the overview page transactions. Here this slot handler deals with switching the user to that tab, and lets
//! the  normal code (above) handle the final activities and details of passing it on to the walletFrame for further processing.
// Page(Qt::Key_5)
void AnoncoinGUI::gotoHistoryPage( void )
{
    historyAction->trigger();
}

void AnoncoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void AnoncoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}

void AnoncoinGUI::walletAddViewClicked()
{
    WalletAddViewDialog dlg(this);
    dlg.exec();
}

void AnoncoinGUI::walletClearViewsClicked()
{
    WalletClearViewsDialog dlg(this);
    dlg.exec();
}

void AnoncoinGUI::walletRestartCoreClicked()
{
    WalletRestartCoreDialog dlg(this);
    dlg.exec();
}

void AnoncoinGUI::walletUpdateViewClicked()
{
    WalletUpdateViewDialog dlg(this);
    dlg.exec();
}

#endif // ENABLE_WALLET

void AnoncoinGUI::setNumConnections(int count)
{
    QString icon;

#ifdef ENABLE_I2PSAM
    // Until we have the clientModel we can't determine the i2p connection count
    int realcount = clientModel ? count - clientModel->getNumConnections(CONNECTIONS_I2P_ALL) : count;
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
    if( labelI2PConnections->isVisible() ) {
        QString i2pIcon;

        // We never run this until we have the clientModel, so the check shouldn't be necessary.
        // Otherwise we can't determine the i2p connection count
        // so we just show the total count here as well...
        int realcount = clientModel ? clientModel->getNumConnections(CONNECTIONS_I2P_ALL) : count;

        // See the anoncoin.qrc file for icon files below & their associated alias name
        switch(realcount) {
        case 0: i2pIcon = ":/icons/i2pconnect_0"; break;
        case 1: case 2: case 3: i2pIcon = ":/icons/i2pconnect_1"; break;
        case 4: case 5: case 6: i2pIcon = ":/icons/i2pconnect_2"; break;
        case 7: case 8: case 9: i2pIcon = ":/icons/i2pconnect_3"; break;
        default: i2pIcon = ":/icons/i2pconnect_4"; break;
        }
        labelI2PConnections->setPixmap(QIcon(i2pIcon).pixmap(4*STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelI2PConnections->setToolTip(tr("%n active connection(s) to I2P-Anoncoin network", "", realcount));
    }
}
#endif // ENABLE_I2PSAM

void AnoncoinGUI::setNumBlocks(int count)
{
    if(!clientModel)
        return;

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
#endif // ENABLE_WALLET

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
#endif // ENABLE_WALLET

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

        // Here now we introduce some special code for our -generatei2pdestination command
        // We know that is the message, if the button condition is correct (Note: no other
        // place in the code should use that combination for this to work.  The rest is simple...
        bool bGenI2pDest = false;
        QString theMsg;                                     // Setup a place for a local copy of the message
        if( buttons == (CClientUIInterface::BTN_ABORT | CClientUIInterface::BTN_APPLY ) ) {
            theMsg = tr("A new I2P Destination was generated.\nDetails have been written to your debug.log file.\nDo you want to continue or shutdown?");
            LogPrintf( message.toStdString().c_str() );
            bGenI2pDest = true;
        }
        else
            theMsg = message;
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, theMsg, buttons, this);
        int r = mBox.exec();
        if( ret != NULL )
            *ret = r == ( bGenI2pDest ? QMessageBox::Apply : QMessageBox::Ok );
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
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
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
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            QApplication::quit();
        }
    }
#endif
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
#endif // ENABLE_WALLET

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
    // ToDo:
    // When the StyleChange event occurs and the appToolBar gets a copy.  It passes that on to each and every widget it has listed.
    // That would be the toolbuttons that are needing such an event, however this causes it to happen TWICE, so we detect that
    // here and eliminate the double whammy of duplicate style changes for each button, or would if that is all that was needed and
    // done, as we can not know for sure, perhaps the designer needs to adjust the toolbar as well.  This has been commented out so
    // it can be tested with future Themes to see if there is a problem or not with
    if( event->type() == QEvent::StyleChange ) {
        // qDebug() << "The Style Change event has occurred.";
        if( object == appToolBar ) {
            // return true;
            // appToolBar->setStyleSheet(strCurrentStyleSheet);
            // qDebug() << "...and it is for the ToolBar";
        // Needed to keep the stylesheet tab icons from being overwritten
        // if( clientModel && clientModel->getOptionsModel() ) {
            // clientModel->getOptionsModel()->applyTheme();
        // }
        } else {
            // qDebug() << "This was for an Object named: " << object->objectName();
        }
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
        walletFrame->gotoSendCoinsPage(recipient.address);
        return true;
    }
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
#endif // ENABLE_WALLET

void AnoncoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

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

void AnoncoinGUI::ShutdownMainWindow()
{
    if(rpcConsole) rpcConsole->hide();
    if( i2pAddress ) i2pAddress->hide();
    if( trayIcon ) trayIcon->hide();
    hide();
    setClientModel(0);
    fShutdownInProgress = true;
}

//! This will check the Core for a shutdown signal ever 200ms, if found then we process the request as if the user had requested a shutdown.
void AnoncoinGUI::detectShutdown()
{
    if (ShutdownRequested())
    {
        qDebug() << "Anoncoin Core has requested the application be shutdown.";
        qApp->quit();
    }
}

void AnoncoinGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

static bool ThreadSafeMessageBox(AnoncoinGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
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

void AnoncoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void AnoncoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

bool AnoncoinGUI::GetThemeDataPath( boost::filesystem::path& themeDirPathOut )
{
    static bool fPathSet = false;
    static boost::filesystem::path fspCurrentThemes;

    //! The 1st time we run this and setup the path we log the result for debugging purposes
    bool fLogResult = false;
    //! If there was an error, we return the default location in the callers referenced variable and a false result
    boost::filesystem::path fspDefaultLocation;
    //! If an error was caught, the message is stored here
    QString sErr;
    //! Only go through all the path logic once, then use the same path every time afterwards, unless there was an error, in which cause it keeps trying over and over
    if( !fPathSet ) {
        fLogResult = true;
        try {
            //! Only initialize and/or use one theme directory in the primary data directory as our fist choice, regardless of the network type (main,testnet,regtest)
            fspDefaultLocation = GetDataDir(false) / "themes";
            //! If we can't find it there, look elsewhere...
            if( boost::filesystem::exists(fspDefaultLocation) && boost::filesystem::is_directory(fspDefaultLocation) ) {
                fspCurrentThemes = fspDefaultLocation;
                fPathSet = true;
            } else {
                //! Otherwise check in the currently running program executable path, see if it can be found and used from there,
                //! that is where a Windows user will have it automatically installed to (perhaps other OS versions in the future as well).
                //! Be sure to only open the themes files with read-only set, if this is used.  Otherwise security conflicts are likely
                //! to cause a runtime error when referencing a program (or system) folder.
                fspCurrentThemes = boost::filesystem::current_path() / "themes";
                // This maybe needed instead: fspCurrentThemes = boost::filesystem::initial_path() / "themes";
                if( boost::filesystem::exists(fspCurrentThemes) && boost::filesystem::is_directory(fspCurrentThemes) ) {
                    fPathSet = true;
                } else {
                    //! Try to create it in the default location, and add a note to the log that the user should move the themes there...
                    //! If the creation fails, it will not be because the directory already exists, we know that it does not, and so
                    //! an error must have been thrown, in any case we handle log reporting below so hopefully the user can figure out why.
                    if( boost::filesystem::create_directory(fspDefaultLocation) ) {
                        qDebug() << tr("Created default Themes Directory : ") + GUIUtil::boostPathToQString( fspDefaultLocation ) + tr(" You will need to manually populate it with your selected themes.");
                        fspCurrentThemes = fspDefaultLocation;
                        fPathSet = true;
                    }
                }
            }
        } catch( const boost::filesystem::filesystem_error& e ) {
            sErr.fromStdString(e.what());
            fspCurrentThemes = fspDefaultLocation;
        }
    }

    themeDirPathOut = fspCurrentThemes;
    if( fLogResult ) {
        QString sPathUsed = GUIUtil::boostPathToQString( fspCurrentThemes );
        qDebug() << tr("Themes Directory Path set : ") + sPathUsed;
        if( !sErr.isEmpty() )
            // ToDo: This message line can not be added with language translation until this function is in one of the class methods, it is called during optoinsdialog
            // creation, yet can not simply be added to the optionsmodel class for some reason that reference has not been defined yet.  So debug logging was added instead,
            // if this is fine, remove the ToDo
            // QMessageBox::critical(0, tr("Anoncoin"), tr("Error: Specified theme directory \"%1\" cannot be created.").arg(sPathUsed));
            qDebug() << tr("Error: While attempting to discover and define the themes directory, the following error was produced: ") + sErr;
        strlistDefaultIconPaths = QIcon::themeSearchPaths();
    }

    return fPathSet;
}

bool AnoncoinGUI::applyTheme()
{
    bool fSetDefault = false;
    bool fReadDefault = false;
    bool fUpdateStyle = false;
    QString sTheme;
    QString styleSheet;
    QString strThemeFolder;
    QSettings settings;
    static bool fFirstTime = true;

    //! If the setting is incorrectly set to empty() or anything other than default, it should be the name of a subdirectory in the themes folder
    if( settings.contains("selectedTheme") )
        sTheme = settings.value("selectedTheme").toString();

    if( fFirstTime ) {
        qDebug() << tr("Initial Theme set to: ") << sTheme;
        fFirstTime = false;
    }
    //! If anything other than a default theme is selected, proceed carefully into processing the stylesheet file(s) details
    if( sTheme.isEmpty() ) fSetDefault = true;
    if( sTheme == "(default)" ) fReadDefault = true;
    if( !fSetDefault && !fReadDefault ) {
        boost::filesystem::path themeDirPath;
        if( GetThemeDataPath( themeDirPath ) ) {
            boost::filesystem::path themeFolder = themeDirPath / sTheme.toStdString();
            if( boost::filesystem::is_directory( themeFolder ) ) {
                strThemeFolder = GUIUtil::boostPathToQString( themeFolder );
                boost::filesystem::path styleFile = themeFolder / "styles.qss";
                if( boost::filesystem::exists( styleFile ) ) {
                    QFile qss( GUIUtil::boostPathToQString(styleFile) );
                    // open qss stylesheet
                    if (qss.open(QFile::ReadOnly)) {
                        // read stylesheet
                        styleSheet = QString(qss.readAll());
                        QTextStream in(&qss);
                        // rewind
                        in.seek(0);
                        bool readingVariables = false;
                        // template variables : key => value
                        QMap<QString, QString> variables;
                        // seek for variables
                        while(!in.atEnd()) {
                            QString line = in.readLine();
                            // variables starts here
                            if (line == "/** [VARS]") {
                                readingVariables = true;
                            }
                            // variables end here
                            if (line == "[/VARS] */") {
                                break;
                            }
                            // if we're reading variables - store them in a map
                            // Idea came from ZeeWolf's themes in Hyperstake
                            if (readingVariables == true) {
                                // skip empty lines
                                if (line.length()>3 && line.contains('=')) {
                                    QStringList fields = line.split("=");
                                    QString var = fields.at(0).trimmed();
                                    QString value = fields.at(1).trimmed();
                                    variables[var] = value;
                                }
                            }
                        }

                        // For simpler use we replace "_themesdir" in the
                        // stylesheet with the appropriate path.
                        styleSheet.replace("_themesdir", strThemeFolder);

                        QMapIterator<QString, QString> variable(variables);
                        variable.toBack();
                        // iterate backwards to prevent overwriting variables
                        while (variable.hasPrevious()) {
                            variable.previous();
                            // replace variables
                            styleSheet.replace(variable.key(), variable.value());
                        }
                        qss.close();
                        fUpdateStyle = true;
                    } else
                        fSetDefault = true;
                } else
                    fSetDefault = true;
            } else
                fSetDefault = true;
        } else
            fSetDefault = true;
    }

    if( fSetDefault ) {
        // Make sure it is set to "(default)" and saved
        sTheme = "(default)";
        settings.setValue( "selectedTheme", sTheme );
        fReadDefault = true;
    }

    if( fReadDefault ) {
        QFile qss(":/style/default");
        // open qss stylesheet
        if (qss.open(QFile::ReadOnly)) {
            // read stylesheet
            styleSheet = QString(qss.readAll());
            qss.close();
            fUpdateStyle = true;
        }
    }

    if( fUpdateStyle ) {
        //! Before we apply the next stylesheet settings, we restore the icons to default
        RestoreDefaultIcons();
        //! The Default search path for OS and Anoncoin icons was defined when we first located the themes directory,
        //! we always use that here as the starting point encase the user wishes to restore default settings, if not
        //! we place the correct theme subdirectory(s) as the 1st choice in that search path.
        QStringList my_icon_paths = strlistDefaultIconPaths;
        if( !strThemeFolder.isEmpty() ) {
            my_icon_paths.prepend( strThemeFolder );
#if defined( DONT_COMPILE )
            //! Include all subdirectories that are found in the themes folder
            QDir themeDir( strThemeFolder );
            themeDir.setFilter(QDir::Dirs);
            QStringList entries = themeDir.entryList();
            for( QStringList::ConstIterator entry=entries.begin(); entry!=entries.end(); ++entry ) {
                QString themeSubDir=*entry;
                if(themeSubDir != "." && themeSubDir != "..") {
                    QString fullPath = strThemeFolder;
                    fullPath += '/';
                    fullPath += themeSubDir;
                    my_icon_paths.prepend( QDir::fromNativeSeparators( fullPath ) );
                }
            }
            qDebug() << "Theme Icon Paths:";
            for( QStringList::ConstIterator entry=my_icon_paths.begin(); entry!=my_icon_paths.end(); ++entry )
                qDebug() << *entry;
#endif // defined
        }
        //! Set the search path for icon themes
        QIcon::setThemeSearchPaths(my_icon_paths);
        //! Set our theme name
        QIcon::setThemeName( sTheme );
        //! Keep a class wide copy of the parsed styleSheet for later use.  NOTE: This may not be needed, although offers some debugging potential
        strCurrentStyleSheet = styleSheet;

        qApp->setStyleSheet(styleSheet);
        //! Promote style change.  The problem with doing a stylesheet update here is that widgets come and go, hopefully all
        //! issues get resolved, because the application itself has been updated to the current style sheet settings.
        //! If that is not the case, then the new global string (strCurrentStyleSheet ) can be used to update the StyleSheet,
        //! it has already been parsed and made ready with correct paths for all details such as icon files and other.  When
        //! a paint refresh event (or other) occurs, and those happen VERY frequently, it could be used to apply the style again
        //! for some issue. The ApplyTheme call itself should not be called again unless the user has really changed the setting,
        //! it is time complex and OS system dependent on a great many factors, which may requiring allot of processing.
        QWidgetList widgets = QApplication::allWidgets();
        for (int i = 0; i < widgets.size(); ++i) {
            QWidget *widget = widgets.at(i);
            //widget->setStyleSheet(styleSheet);
            QEvent event(QEvent::StyleChange);
            QApplication::sendEvent(widget, &event);
            //widget->style()->polish(widget);
        }
        // qApp->style()->polish(qApp);
    }

    return true;
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
