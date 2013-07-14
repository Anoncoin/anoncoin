//
// I2P-patch
// Copyright (c) 2012-2013 giv
#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H

#include <QObject>

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

/** Model for Bitcoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT
public:
    explicit ClientModel(OptionsModel *optionsModel, QObject *parent = 0);
    ~ClientModel();

    enum MiningType
    {
        SoloMining,
        PoolMining
    };

    OptionsModel *getOptionsModel();

    int getNumConnections() const;
    int getNumBlocks() const;
    int getNumBlocksAtStartup();
    MiningType getMiningType() const;
    int getMiningThreads() const;
    bool getMiningStarted() const;

    bool getMiningDebug() const;
    void setMiningDebug(bool debug);
    int getMiningScanTime() const;
    void setMiningScanTime(int scantime);
    QString getMiningServer() const;
    void setMiningServer(QString server);
    QString getMiningPort() const;
    void setMiningPort(QString port);
    QString getMiningUsername() const;
    void setMiningUsername(QString username);
    QString getMiningPassword() const;
    void setMiningPassword(QString password);

    int getHashrate() const;
    double GetDifficulty() const;

    QDateTime getLastBlockDate() const;

    //! Return true if client connected to testnet
    bool isTestNet() const;
    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return conservative estimate of total number of blocks, or 0 if unknown
    int getNumBlocksOfPeers() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    void setMining(MiningType type, bool mining, int threads, int hashrate);

    QString formatFullVersion() const;
    QString formatBuildDate() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

#ifdef USE_NATIVE_I2P
    QString formatI2PNativeFullVersion() const;
    int getNumI2PConnections() const;

    QString getPublicI2PKey() const;
    QString getPrivateI2PKey() const;
    bool isI2PAddressGenerated() const;
    bool isI2POnly() const;
    bool isTOROnly() const;
    QString getB32Address(const QString& destination) const;
    void generateI2PDestination(QString& pub, QString& priv) const;
//    I2PSession& getI2PSession() const;
//    bool isPermanent
#endif

private:
    OptionsModel *optionsModel;

    int cachedNumBlocks;
    int cachedNumBlocksOfPeers;
    int cachedHashrate;

    MiningType miningType;
    int miningThreads;
    bool miningStarted;
    bool miningDebug;
    int miningScanTime;
    QString miningServer;
    QString miningPort;
    QString miningUsername;
    QString miningPassword;

    int numBlocksAtStartup;

    QTimer *pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
signals:
    void numConnectionsChanged(int count);
#ifdef USE_NATIVE_I2P
    void numI2PConnectionsChanged(int count);
#endif
    void numBlocksChanged(int count, int countOfPeers);
    void miningChanged(bool mining, int count);

    //! Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);

public slots:
    void updateTimer();
    void updateNumConnections(int numConnections);
#ifdef USE_NATIVE_I2P
    void updateNumI2PConnections(int numI2PConnections);
#endif
    void updateAlert(const QString &hash, int status);
};

#endif // CLIENTMODEL_H
