// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H

// Many builder specific things set in the config file, for any source files where we rely on moc_xxx files being generated
// it is best to include the anoncoin-config.h in the header file itself.  Not the .cpp src file, because otherwise any
// conditional compilation guidelines, which rely on the build configuration, will not be present in the moc_xxx files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include <QObject>

class AddressTableModel;
class OptionsModel;
class PeerTableModel;
class TransactionTableModel;

class CWallet;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

enum NumConnections {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN   = (1U << 0),
    CONNECTIONS_OUT  = (1U << 1),
    CONNECTIONS_I2P_IN  = (1U << 2),
    CONNECTIONS_I2P_OUT = (1U << 3),
    CONNECTIONS_I2P_ALL = (CONNECTIONS_I2P_OUT | CONNECTIONS_I2P_IN),
    CONNECTIONS_ALL  = (CONNECTIONS_IN | CONNECTIONS_OUT),
};

/** Model for Anoncoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel *optionsModel, QObject *parent = 0);
    ~ClientModel();

    OptionsModel *getOptionsModel();
    PeerTableModel *getPeerTableModel();

    //! Return number of connections, default is in- and outbound (total)
    int getNumConnections(unsigned int flags = CONNECTIONS_ALL) const;
    int getNumBlocks() const;
    int getNumBlocksAtStartup();

    quint64 getTotalBytesRecv() const;
    quint64 getTotalBytesSent() const;

    double getVerificationProgress() const;
    QDateTime getLastBlockDate() const;

    //! Return network (main, testnetX, regtest)
    QString getNetworkName() const;
    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return true if core is importing blocks
    enum BlockSource getBlockSource() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatBuildDate() const;
    bool isReleaseVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

#ifdef ENABLE_I2PSAM
    /*
     * Public functions needed for handling the I2P Config and operational settings
     */
    QString formatI2PNativeFullVersion() const;
    QString getPublicI2PKey() const;
    QString getPrivateI2PKey() const;
    bool isI2PAddressGenerated() const;
    bool isI2POnly() const;
    bool isTorOnly() const;
    bool isDarknetOnly() const;
    bool isBehindDarknet() const;
    QString getB32Address(const QString& destination) const;
    void generateI2PDestination(QString& pub, QString& priv) const;
#endif // ENABLE_I2PSAM

private:
    OptionsModel *optionsModel;
    PeerTableModel *peerTableModel;

    int cachedNumBlocks;
    bool cachedReindexing;
    bool cachedImporting;

    int numBlocksAtStartup;

    QTimer *pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

/** Keep in mind...
 * Note signal functions like this are created here in the header file, yet no source implementation
 * will be found in the developer code.  It's up-to the build process to create moc_xx files, from which
 * is generated sufficient information that QT runs & the linker is able to create an executable.
 */
Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count);
    void alertsChanged(const QString &warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);

    //! Fired when a message should be reported to the user
    void message(const QString &title, const QString &message, unsigned int style);

    //! Show progress dialog e.g. for verifychain
    void showProgress(const QString &title, int nProgress);

/** Keep in mind...
 * From: https://qt-project.org/doc/qt-5-snapshot/signalsandslots.html
 * A slot is a function that is called in response to a particular signal. Qt's widgets have many pre-defined slots,
 * but it is common practice to subclass widgets and add slots so that you can handle the signals that you are interested in...
 */
public Q_SLOTS:
    void updateTimer();
    void updateNumConnections(int numConnections);
    void updateAlert(const QString &hash, int status);
};

#endif // CLIENTMODEL_H
