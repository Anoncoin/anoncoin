// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "optionsmodel.h"
// Anoncoin-config.h has been loaded...

#include "anoncoinunits.h"
#include "guiutil.h"

#include "init.h"
#include "main.h"
#include "net.h"
#include "txdb.h" // for -dbcache defaults
#ifdef ENABLE_WALLET
#include "wallet.h"
#include "walletdb.h"
#endif
#ifdef ENABLE_I2PSAM
#include "i2pwrapper.h"
#endif

#include <QApplication>
#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>
#include <QFile>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif
#include <QStyle>
#include <QTextStream>

bool applyTheme();

// Only needed because we have a temporary readonly on the i2p settings
#ifdef NOTYET_ENABLE_I2PSAM
#include <QMessageBox>
            QMessageBox::warning( NULL, "Notice - Upgrade in progress",
                                  "Values are now for read-only purposes only",
                                  QMessageBox::Ok, QMessageBox::Ok );
#endif

OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init()
{
    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool();

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", AnoncoinUnits::ANC);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    if (!settings.contains("nDatabaseCache"))
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif"))
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    // Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    nTransactionFee = settings.value("nTransactionFee").toLongLong(); // if -paytxfee is set, this will be overridden later in init.cpp
    if (mapArgs.count("-paytxfee"))
        addOverriddenOption("-paytxfee");

    if (!settings.contains("bSpendZeroConfChange"))
        settings.setValue("bSpendZeroConfChange", true);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
#endif

    // Network
    if (!settings.contains("fUseUPnP"))
#ifdef USE_UPNP
        settings.setValue("fUseUPnP", true);
#else
        settings.setValue("fUseUPnP", false);
#endif
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");

    // Display
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();
    selectedTheme = settings.value("selectedTheme", "(default)").toString();
    //printf("DEBUG: %s\n", selectedTheme.toAscii().data());
    applyTheme();

    if (!selectedTheme.isEmpty())
        SoftSetArg("-theme", selectedTheme.toStdString());
#ifdef ENABLE_I2PSAM
    // Initialize our options model parameters for I2P session variables, from what has already been set, primarily from the variables in anoncoin.conf
    // ToDo: These have become read-only, yet the interface still needs to be changed.
    I2PUseI2POnly = IsI2POnly();
    I2PSAMHost = QString::fromStdString( GetArg( "-i2p.options.samhost", SAM_DEFAULT_ADDRESS ) );
    I2PSAMPort = (int)GetArg( "-i2p.options.samport", SAM_DEFAULT_PORT );
    I2PSessionName = QString::fromStdString( GetArg( "-i2p.options.sessionname", I2P_SESSION_NAME_DEFAULT ) );

    i2pInboundQuantity        = (int)GetArg( "-i2p.options.inbound.quantity"        , SAM_DEFAULT_INBOUND_QUANTITY );
    i2pInboundLength          = (int)GetArg( "-i2p.options.inbound.length"          , SAM_DEFAULT_INBOUND_LENGTH );
    i2pInboundLengthVariance  = (int)GetArg( "-i2p.options.inbound.lengthvariance"  , SAM_DEFAULT_INBOUND_LENGTHVARIANCE );
    i2pInboundBackupQuantity  = (int)GetArg( "-i2p.options.inbound.backupquantity"  , SAM_DEFAULT_INBOUND_BACKUPQUANTITY );
    i2pInboundAllowZeroHop    = GetBoolArg( "-i2p.options.inbound.allowzerohop"     , SAM_DEFAULT_INBOUND_ALLOWZEROHOP );
    i2pInboundIPRestriction   = (int)GetArg( "-i2p.options.inbound.iprestriction"   , SAM_DEFAULT_INBOUND_IPRESTRICTION );
    i2pOutboundQuantity       = (int)GetArg( "-i2p.options.outbound.quantity"       , SAM_DEFAULT_OUTBOUND_QUANTITY );
    i2pOutboundLength         = (int)GetArg( "-i2p.options.outbound.length"         , SAM_DEFAULT_OUTBOUND_LENGTH );
    i2pOutboundLengthVariance = (int)GetArg( "-i2p.options.outbound.lengthvariance" , SAM_DEFAULT_OUTBOUND_LENGTHVARIANCE );
    i2pOutboundBackupQuantity = (int)GetArg( "-i2p.options.outbound.backupquantity" , SAM_DEFAULT_OUTBOUND_BACKUPQUANTITY );
    i2pOutboundAllowZeroHop   = GetBoolArg( "-i2p.options.outbound.allowzerohop"    , SAM_DEFAULT_OUTBOUND_ALLOWZEROHOP );
    i2pOutboundIPRestriction  = (int)GetArg( "-i2p.options.outbound.iprestriction"  , SAM_DEFAULT_OUTBOUND_IPRESTRICTION );
    i2pOutboundPriority       = (int)GetArg( "-i2p.options.outbound.priority"       , SAM_DEFAULT_OUTBOUND_PRIORITY );
#endif // ENABLE_I2PSAM
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Remove all entries from our QSettings object
    settings.clear();

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case Fee:
            // Attention: Init() is called before nTransactionFee is set in AppInit2()!
            // To ensure we can change the fee on-the-fly update our QSetting when
            // opening OptionsDialog, which queries Fee via the mapper.
            if (nTransactionFee != settings.value("nTransactionFee").toLongLong())
                settings.setValue("nTransactionFee", (qint64)nTransactionFee);
            // Todo: Consider to revert back to use just nTransactionFee here, if we don't want
            // -paytxfee to update our QSettings!
            return settings.value("nTransactionFee");
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
#endif
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Language:
            return settings.value("language");
        case OurTheme:
            return settings.value("selectedTheme");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
#ifdef ENABLE_I2PSAM
        case eI2PUseI2POnly:
            return QVariant( I2PUseI2POnly );
        case eI2PSAMHost:
            return QVariant( I2PSAMHost );
        case eI2PSAMPort:
            return QVariant( I2PSAMPort );
        case eI2PSessionName:
            return QVariant( I2PSessionName );
        case I2PInboundQuantity:
            return QVariant(i2pInboundQuantity);
        case I2PInboundLength:
            return QVariant(i2pInboundLength);
        case I2PInboundLengthVariance:
            return QVariant(i2pInboundLengthVariance);
        case I2PInboundBackupQuantity:
            return QVariant(i2pInboundBackupQuantity);
        case I2PInboundAllowZeroHop:
            return QVariant(i2pInboundAllowZeroHop);
        case I2PInboundIPRestriction:
            return QVariant(i2pInboundIPRestriction);
        case I2POutboundQuantity:
            return QVariant(i2pOutboundQuantity);
        case I2POutboundLength:
            return QVariant(i2pOutboundLength);
        case I2POutboundLengthVariance:
            return QVariant(i2pOutboundLengthVariance);
        case I2POutboundBackupQuantity:
            return QVariant(i2pOutboundBackupQuantity);
        case I2POutboundAllowZeroHop:
            return QVariant(i2pOutboundAllowZeroHop);
        case I2POutboundIPRestriction:
            return QVariant(i2pOutboundIPRestriction);
        case I2POutboundPriority:
            return QVariant(i2pOutboundPriority);
#endif // ENABLE_I2PSAM
        default:
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        }
        break;
#ifdef ENABLE_WALLET
        case Fee: // core option - can be changed on-the-fly
            // Todo: Add is valid check  and warn via message, if not
            nTransactionFee = value.toLongLong();
            settings.setValue("nTransactionFee", (qint64)nTransactionFee);
            emit transactionFeeChanged(nTransactionFee);
            break;
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
#endif
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case OurTheme:
            settings.setValue("selectedTheme", value);
            applyTheme();
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            emit coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
#ifdef ENABLE_I2PSAM
        case eI2PUseI2POnly:
        case eI2PSAMHost:
        case eI2PSAMPort:
        case eI2PSessionName:
        case I2PInboundQuantity:
        case I2PInboundLength:
        case I2PInboundLengthVariance:
        case I2PInboundBackupQuantity:
        case I2PInboundAllowZeroHop:
        case I2PInboundIPRestriction:
        case I2POutboundQuantity:
        case I2POutboundLength:
        case I2POutboundLengthVariance:
        case I2POutboundBackupQuantity:
        case I2POutboundAllowZeroHop:
        case I2POutboundIPRestriction:
        case I2POutboundPriority:
            break;
#endif // ENABLE_I2PSAM
        default:
            break;
        }
    }
// ToDo: Finish fixing this so that it is readonly or removed
#ifdef ENABLE_I2PSAM
    if( index.row() < eI2PUseI2POnly )
#endif
        emit dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant &value)
{
    if (!value.isNull())
    {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        emit displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.ToStringIP()));
        proxy.setPort(curProxy.GetPort());

        return true;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

bool applyTheme()
{
    QSettings settings;
    // template variables : key => value
    QMap<QString, QString> variables;
    QString sTheme = settings.value("selectedTheme", "(default)").toString();
    // The datadir is where the wallet and block are kept
    QString ddDir = QString::fromStdString ( GetDataDir().string() );
    // path to selected theme dir
    QString themeDir = ( ddDir + "/themes/" + sTheme );

    // if theme selected
    if ( (sTheme != "") && (sTheme != "(default)") ) {
        QFile qss(ddDir + "/themes/" + sTheme + "/styles.qss");
        // open qss stylesheet
        if (qss.open(QFile::ReadOnly))
        {
            // read stylesheet
            QString styleSheet = QString(qss.readAll());
            QTextStream in(&qss);
            // rewind
            in.seek(0);
            bool readingVariables = false;

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
            styleSheet.replace("_themesdir", themeDir);

            QMapIterator<QString, QString> variable(variables);
            variable.toBack();
            // iterate backwards to prevent overwriting variables
            while (variable.hasPrevious()) {
                variable.previous();
                // replace variables
                styleSheet.replace(variable.key(), variable.value());
            }

            qss.close();

            qApp->setStyleSheet(styleSheet);

            // Promote style change
            QWidgetList widgets = QApplication::allWidgets();
            for (int i = 0; i < widgets.size(); ++i) {
                QWidget *widget = widgets.at(i);
                QEvent event(QEvent::StyleChange);
                QApplication::sendEvent(widget, &event);
            }
        }
    } else {
        // If not theme name given - clear styles
        qApp->setStyleSheet(QString(""));
    }

    return true;
}
