// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef OPTIONSMODEL_H
#define OPTIONSMODEL_H

// Many builder specific things set in the config file, for any source files where we rely on moc_xxx files being generated
// it is best to include the anoncoin-config.h in the header file itself.  Not the .cpp src file, because otherwise any
// conditional compilation guidelines, which rely on the build configuration, will not be present in the moc_xxx files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include <QAbstractListModel>

QT_BEGIN_NAMESPACE
class QNetworkProxy;
QT_END_NAMESPACE

/** Interface from Qt to configuration data structure for Anoncoin client.
   To Qt, the options are presented as a list with the different options
   laid out vertically.
   This can be changed to a tree once the settings become sufficiently
   complex.
 */
class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject *parent = 0);

    enum OptionID {
        StartAtStartup,         // bool
        MinimizeToTray,         // bool
        MapPortUPnP,            // bool
        MinimizeOnClose,        // bool
        ProxyUse,               // bool
        ProxyIP,                // QString
        ProxyPort,              // int
        ProxySocksVersion,      // int
        Fee,                    // qint64
        DisplayUnit,            // AnoncoinUnits::Unit
        ThirdPartyTxUrls,       // QString
        Language,               // QString
        CoinControlFeatures,    // bool
        ThreadsScriptVerif,     // int
        DatabaseCache,          // int
        SpendZeroConfChange,    // bool
#ifdef ENABLE_I2PSAM
        eI2PUseI2POnly,             // bool
        eI2PSAMHost,                // QString
        eI2PSAMPort,                // int
        eI2PSessionName,            // QString

        I2PInboundQuantity,         // int
        I2PInboundLength,           // int
        I2PInboundLengthVariance,   // int
        I2PInboundBackupQuantity,   // int
        I2PInboundAllowZeroHop,     // bool
        I2PInboundIPRestriction,    // int

        I2POutboundQuantity,        // int
        I2POutboundLength,          // int
        I2POutboundLengthVariance,  // int
        I2POutboundBackupQuantity,  // int
        I2POutboundAllowZeroHop,    // bool
        I2POutboundIPRestriction,   // int
        I2POutboundPriority,        // int
#endif // ENABLE_I2PSAM
        OptionIDRowCount,
    };

    void Init();
    void Reset();

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);
    /** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
    void setDisplayUnit(const QVariant &value);

    /* Explicit getters */
    bool getMinimizeToTray() { return fMinimizeToTray; }
    bool getMinimizeOnClose() { return fMinimizeOnClose; }
    int getDisplayUnit() { return nDisplayUnit; }
    QString getThirdPartyTxUrls() { return strThirdPartyTxUrls; }
    bool getProxySettings(QNetworkProxy& proxy) const;
    bool getCoinControlFeatures() { return fCoinControlFeatures; }
    const QString& getOverriddenByCommandLine() { return strOverriddenByCommandLine; }

    /* Restart flag helper */
    void setRestartRequired(bool fRequired);
    bool isRestartRequired();

private:
    /* Qt-only settings */
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    QString language;
    int nDisplayUnit;
    QString strThirdPartyTxUrls;
    bool fCoinControlFeatures;
    /* settings that were overriden by command-line */
    QString strOverriddenByCommandLine;

    /// Add option to list of GUI options overridden through command line/config file
    void addOverriddenOption(const std::string &option);

#ifdef ENABLE_I2PSAM
    bool I2PUseI2POnly;
    QString I2PSAMHost;
    int I2PSAMPort;
    QString I2PSessionName;
    int i2pInboundQuantity;
    int i2pInboundLength;
    int i2pInboundLengthVariance;
    int i2pInboundBackupQuantity;
    bool i2pInboundAllowZeroHop;
    int i2pInboundIPRestriction;
    int i2pOutboundQuantity;
    int i2pOutboundLength;
    int i2pOutboundLengthVariance;
    int i2pOutboundBackupQuantity;
    bool i2pOutboundAllowZeroHop;
    int i2pOutboundIPRestriction;
    int i2pOutboundPriority;
#endif // ENABLE_I2PSAM

signals:
    void displayUnitChanged(int unit);
    void transactionFeeChanged(qint64);
    void coinControlFeaturesChanged(bool);
};

#endif // OPTIONSMODEL_H
