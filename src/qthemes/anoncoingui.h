// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOINGUI_H
#define ANONCOINGUI_H

// Many builder specific things set in the config file, for any source files where we rely on moc_xxx files being generated
// it is best to include the anoncoin-config.h in the header file itself.  Not the .cpp src file, because otherwise any
// conditional compilation guidelines, which rely on the build configuration, will not be present in the moc_xxx files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "amount.h"
#include "i2pshowaddresses.h"

#include <boost/filesystem/path.hpp>

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QToolButton>

class ClientModel;
class Notificator;
class OptionsModel;
class RPCConsole;
class SendCoinsRecipient;
class UnitDisplayStatusBarControl;
class WalletFrame;
class WalletModel;
class CWallet;
class AnoncoinGUI;

QT_BEGIN_NAMESPACE
class QAction;
class QLabel;
class QMenu;
class QPoint;
class QProgressBar;
class QProgressDialog;
class QMainToolAction;
QT_END_NAMESPACE

//! These subclasses deal with the Main Page Toolbar buttons and actions, they were developed primarily to deal with the
//! Theme changes we want to offer and the fact that the Icons will not stay changed after an update.  As the icon used
//! is associated with the actions, not the toolbar button that was also sub classed and both now handle the processing
//! required to configure and initialize those aspects of our interface and allow quick customization in the future.
//! Keeping a changed style icon and having it stick to the correct image is not an easy programming task, as the action/
//! icon default loaded from our built-in resources and connected during initialization is what it wants to fall back
//! to thus ignoring the newly selected Theme icon whenever clicked upon.
class QMainToolAction : public QAction
{
    Q_OBJECT

public:
    QMainToolAction( const QString& sDefaultIcon, const QString& sDefaultText, const QString& sDefaultTip, const int nShortCutKey, AnoncoinGUI* pGUI );
    QIcon& GetDefaultIcon() { return ourDefaultIcon; }

protected:
    bool event(QEvent *pEvent);

private:
    int nOurKey;
    AnoncoinGUI* pMainWindow;
    QString strOurName;
    QIcon ourDefaultIcon;
    // QIcon ourStyledIcon;

    ~QMainToolAction() { disconnect(); }

private slots:
    void showNormalIfMinimized();
    void gotoPage();
};

class QMainToolButton : public QToolButton
{
    Q_OBJECT

public:
    QMainToolButton( const QString& sNameIn, QMainToolAction* pActionIn, AnoncoinGUI* pGUI );
    // ~QMainToolButton();

    void RestoreDefaultIcon();

protected:
    bool event(QEvent* pEvent);

private:
    QString strOurName;
    QMainToolAction* pAction;
    AnoncoinGUI* pMainWindow;
};


/**
  Anoncoin GUI main class. This class represents the main window of the Anoncoin UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class AnoncoinGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const QString DEFAULT_WALLET;
    bool fShutdownInProgress;

    explicit AnoncoinGUI(bool fIsTestnet = false, QWidget *parent = 0);
    ~AnoncoinGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel *clientModel);

    //! Handles details for shutting down and hiding all the application windows, system tray icon etc.  This is called after quit() has for sure been called,
    //! but there is yet time to handle various event processing
    void ShutdownMainWindow();

#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a anoncoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addWallet(const QString& name, WalletModel *walletModel);
    bool setCurrentWallet(const QString& name);
    void removeAllWallets();
#endif // ENABLE_WALLET
    bool enableWallet;
#ifdef ENABLE_I2PSAM
    void UpdateI2PAddressDetails( void ) { i2pAddress->UpdateParameters(); }
    void ShowI2pDestination( void ) { openI2pAddressAction->activate( QAction::Trigger ); }
#endif
    //! When themes change, we must first restore the main toolbar buttons Icons to their default values, Then if the theme designer decides they
    //! will be changed to some new style, if not the defaults will be restored with this call.
    void RestoreDefaultIcons();
    //! The theme data path varies greatly with OS and is searched for in various places.  If needed, one is created and once that has been done
    //! and logged to the debug.log file, this method always returns the same path regardless of theme selection and network (main,testnet,regtest).
    bool GetThemeDataPath( boost::filesystem::path& themeDirPathOut );
    //! Allows any other part of the software to discover what the current stylesheet is defined as
    const QString& getCurrentStyleSheet() { return strCurrentStyleSheet; }
    //! This method actually does all the heavy processing to import, parse and update the system with a new theme.
    bool applyTheme();


protected:
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

private:
    ClientModel *clientModel;
    WalletFrame *walletFrame;

    UnitDisplayStatusBarControl *unitDisplayControl;
    QLabel *labelEncryptionIcon;
    QLabel *labelConnectionsIcon;
    QLabel *labelBlocksIcon;
    QLabel *progressBarLabel;
    QProgressBar *progressBar;
    QProgressDialog *progressDialog;

    QMenuBar *appMenuBar;
    QToolBar *appToolBar;

    QAction *quitAction;
    QAction *usedSendingAddressesAction;
    QAction *usedReceivingAddressesAction;

    QAction *signMessageAction;
    QAction *verifyMessageAction;
    QAction *aboutAction;
    QAction *optionsAction;
    QAction *toggleHideAction;
    QAction *encryptWalletAction;
    QAction *backupWalletAction;
    QAction *changePassphraseAction;
    QAction *aboutQtAction;
    QAction *openRPCConsoleAction;
    QAction *openAction;
    QAction *showHelpMessageAction;

    QAction *pWalletAddViewAction;
    QAction *pWalletClearViewsAction;
    QAction *pWalletRestartCoreAction;
    QAction *pWalletUpdateViewAction;

    QMainToolAction *accountsAction;
    QMainToolAction *addressesAction;
    QMainToolAction *historyAction;
    QMainToolAction *overviewAction;
    QMainToolAction *receiveCoinsAction;
    QMainToolAction *sendCoinsAction;

    QMainToolButton *buttonAccounts;
    QMainToolButton *buttonAddresses;
    QMainToolButton *buttonHistory;
    QMainToolButton *buttonOverview;
    QMainToolButton *buttonReceiveCoins;
    QMainToolButton *buttonSendCoins;

    QSystemTrayIcon *trayIcon;
    Notificator *notificator;
    RPCConsole *rpcConsole;
#ifdef ENABLE_I2PSAM
    QLabel* labelI2PConnections;
    QLabel* labelI2POnly;
    QLabel* labelI2PGenerated;
    QAction *openI2pAddressAction;
    ShowI2PAddresses *i2pAddress;
#endif
    /** Keep track of previous number of blocks, to detect progress */
    int prevBlocks;
    int spinnerFrame;

    //! The current themes resulting parsed and filled out StyleSheet, if needed this can be used to update the display or debugging.
    QString strCurrentStyleSheet;
    //! The Initial default list of paths used for finding Icons, Theme changes start with this and add what is required for the current selection
    QStringList strlistDefaultIconPaths;

    /** Create the main UI actions. */
    void createActions(bool fIsTestnet);
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray icon and notification */
    void createTrayIcon(bool fIsTestnet);
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    /** Enable or disable all wallet-related actions */
    void setWalletActionsEnabled(bool enabled);

    /** Connect core signals to GUI client */
    void subscribeToCoreSignals();
    /** Disconnect core signals from GUI client */
    void unsubscribeFromCoreSignals();

signals:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString &uri);

public slots:
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
#ifdef ENABLE_I2PSAM
    void setNumI2PConnections(int count);
#endif
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count);

    /** Notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see CClientUIInterface::MessageBoxFlags
       @param[in] ret       pointer to a bool that will be modified to whether Ok was clicked (modal only)
    */
    void message(const QString &title, const QString &message, unsigned int style, bool *ret = NULL);
    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);

#ifdef ENABLE_WALLET
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    /** Show incoming transaction notification for new transactions. */
    void incomingTransaction(const QString& date, int unit, qint64 amount, const QString& type, const QString& address);

    /** Switch to one of the main toolbar pages, this can be connected to whatever signal you might want to watch and simulates the MainWindow
        effect of pressing the tab button for that view.  Pass it Qt::Key_1 -> Qt::Key_6 values depending on which one you want */
    void gotoPage( const int nKey );
    void gotoHistoryPage( void );

#endif // ENABLE_WALLET

private slots:
#ifdef ENABLE_WALLET
    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");

    /** Show open dialog */
    void openClicked();

    //! New Multiwallet menu options
    void walletAddViewClicked();
    void walletClearViewsClicked();
    void walletRestartCoreClicked();
    void walletUpdateViewClicked();

#endif // ENABLE_WALLET
    /** Show configuration dialog */
    void optionsClicked();
    /** Show about dialog */
    void aboutClicked();
    /** Show help message dialog */
    void showHelpMessageClicked();
#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif

    /** Simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    /** called by a timer to check if fRequestShutdown has been set within the Core */
    void detectShutdown();

    /** Show progress dialog e.g. for verifychain */
    void showProgress(const QString &title, int nProgress);
};

class UnitDisplayStatusBarControl : public QLabel
{
    Q_OBJECT

public:
    explicit UnitDisplayStatusBarControl();
    /** Lets the control know about the Options Model (and its signals) */
    void setOptionsModel(OptionsModel *optionsModel);

protected:
    /** So that it responds to left-button clicks */
    void mousePressEvent(QMouseEvent *event);

private:
    OptionsModel *optionsModel;
    QMenu* menu;

    /** Shows context menu with Display Unit options by the mouse coordinates */
    void onDisplayUnitsClicked(const QPoint& point);
    /** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
    void createContextMenu();

private slots:
    /** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
    void updateDisplayUnit(int newUnits);
    /** Tells underlying optionsModel to update its current display unit. */
    void onMenuSelection(QAction* action);
};

#endif // ANONCOINGUI_H
