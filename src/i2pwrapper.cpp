// Copyright (c) 2013-2015 The Anoncoin Core developers
// Copyright (c) 2012-2013 giv
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------

// Many builder specific things set in the config file, don't forget to include it this way in your source files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "i2pwrapper.h"
#include "util.h"
#include "hash.h"

#include <boost/thread/shared_mutex.hpp>

namespace SAM
{

    class StreamSessionAdapter::SessionHolder {
        public:
            explicit SessionHolder(std::auto_ptr<SAM::StreamSession> session);

            const SAM::StreamSession& getSession() const;
            SAM::StreamSession& getSession();
        private:
            void heal() const;
            void reborn() const;

            mutable std::auto_ptr<SAM::StreamSession> session_;
            typedef boost::shared_mutex mutex_type;
            mutable mutex_type mtx_;
    };

    StreamSessionAdapter::SessionHolder::SessionHolder(std::auto_ptr<SAM::StreamSession> session) : session_(session)
    {}

    const SAM::StreamSession& StreamSessionAdapter::SessionHolder::getSession() const
    {
        boost::upgrade_lock<mutex_type> lock(mtx_);
        if (session_->isSick())
        {
            boost::upgrade_to_unique_lock<mutex_type> ulock(lock);
            heal();
        }
        return *session_;
    }

    SAM::StreamSession& StreamSessionAdapter::SessionHolder::getSession()
    {
        boost::upgrade_lock<mutex_type> lock(mtx_);
        if (session_->isSick())
        {
            boost::upgrade_to_unique_lock<mutex_type> ulock(lock);
            heal();
        }
        return *session_;
    }

    void StreamSessionAdapter::SessionHolder::heal() const
    {
        reborn(); // if we don't know how to heal it just reborn it
    }

    void StreamSessionAdapter::SessionHolder::reborn() const
    {
        if (!session_->isSick())
            return;
        std::auto_ptr<SAM::StreamSession> newSession(new SAM::StreamSession(*session_));
        if (!newSession->isSick() && session_->isSick())
            session_ = newSession;
    }

    //--------------------------------------------------------------------------------------------------

    StreamSessionAdapter::StreamSessionAdapter(
            const std::string& nickname,
            const std::string& SAMHost       /*= SAM_DEFAULT_ADDRESS*/,
                  uint16_t     SAMPort       /*= SAM_DEFAULT_PORT*/,
            const std::string& myDestination /*= SAM_GENERATE_MY_DESTINATION*/,
            const std::string& i2pOptions    /*= SAM_DEFAULT_I2P_OPTIONS*/,
            const std::string& minVer        /*= SAM_DEFAULT_MIN_VER*/,
            const std::string& maxVer        /*= SAM_DEFAULT_MAX_VER*/)
        : sessionHolder_(
              new SessionHolder(
                  std::auto_ptr<SAM::StreamSession>(
                      new SAM::StreamSession(nickname, SAMHost, SAMPort, myDestination, i2pOptions, minVer, maxVer))))
    {}

    StreamSessionAdapter::~StreamSessionAdapter()
    {}

    SAM::SOCKET StreamSessionAdapter::accept(bool silent)
    {
        SAM::RequestResult<std::auto_ptr<SAM::Socket> > result = sessionHolder_->getSession().accept(silent);
        // call Socket::release
        return result.isOk ? result.value->release() : SAM_INVALID_SOCKET;
    }

    SAM::SOCKET StreamSessionAdapter::connect(const std::string& destination, bool silent)
    {
        SAM::RequestResult<std::auto_ptr<SAM::Socket> > result = sessionHolder_->getSession().connect(destination, silent);
        // call Socket::release
        return result.isOk ? result.value->release() : SAM_INVALID_SOCKET;
    }

    bool StreamSessionAdapter::forward(const std::string& host, uint16_t port, bool silent)
    {
        return sessionHolder_->getSession().forward(host, port, silent).isOk;
    }

    std::string StreamSessionAdapter::namingLookup(const std::string& name) const
    {
        SAM::RequestResult<const std::string> result = sessionHolder_->getSession().namingLookup(name);
        return result.isOk ? result.value : std::string();
    }

    SAM::FullDestination StreamSessionAdapter::destGenerate() const
    {
        SAM::RequestResult<const SAM::FullDestination> result = sessionHolder_->getSession().destGenerate();
        return result.isOk ? result.value : SAM::FullDestination();
    }

    void StreamSessionAdapter::stopForwarding(const std::string& host, uint16_t port)
    {
        sessionHolder_->getSession().stopForwarding(host, port);
    }

    void StreamSessionAdapter::stopForwardingAll()
    {
        sessionHolder_->getSession().stopForwardingAll();
    }

    const SAM::FullDestination& StreamSessionAdapter::getMyDestination() const
    {
        return sessionHolder_->getSession().getMyDestination();
    }

    const sockaddr_in& StreamSessionAdapter::getSAMAddress() const
    {
        return sessionHolder_->getSession().getSAMAddress();
    }

    const std::string& StreamSessionAdapter::getSAMHost() const
    {
        return sessionHolder_->getSession().getSAMHost();
    }

    uint16_t StreamSessionAdapter::getSAMPort() const
    {
        return sessionHolder_->getSession().getSAMPort();
    }

    const std::string& StreamSessionAdapter::getNickname() const
    {
        return sessionHolder_->getSession().getNickname();
    }

    const std::string& StreamSessionAdapter::getSAMMinVer() const
    {
        return sessionHolder_->getSession().getSAMMinVer();
    }

    const std::string& StreamSessionAdapter::getSAMMaxVer() const
    {
        return sessionHolder_->getSession().getSAMMaxVer();
    }

    const std::string& StreamSessionAdapter::getSAMVersion() const
    {
        return sessionHolder_->getSession().getSAMVersion();
    }

    const std::string& StreamSessionAdapter::getOptions() const
    {
        return sessionHolder_->getSession().getOptions();
    }

    const std::string& StreamSessionAdapter::getSessionID() const
    {
        return sessionHolder_->getSession().getSessionID();
    }

} // namespace SAM

//--------------------------------------------------------------------------------------------------
static std::string sSession = I2P_SESSION_NAME_DEFAULT;
static std::string sHost = SAM_DEFAULT_ADDRESS;
static uint16_t uPort = SAM_DEFAULT_PORT;
static std::string sDestination = SAM_GENERATE_MY_DESTINATION;
static std::string sOptions;

I2PSession::I2PSession() : SAM::StreamSessionAdapter( sSession, sHost, uPort, sDestination, sOptions )
{
    ::sDestination = this->getMyDestination().priv;
}

I2PSession::~I2PSession()
{}

/*static*/
std::string I2PSession::GenerateB32AddressFromDestination(const std::string& destination)
{
    return ::B32AddressFromDestination(destination);
}

void static FormatBoolI2POptionsString( std::string& I2pOptions, const std::string& I2pSamName, const std::string& confParamName ) {
    bool fConfigValue = GetArg( confParamName, true );

    if (!I2pOptions.empty()) I2pOptions += " ";                             // seperate the parameters with <whitespace>
    I2pOptions += I2pSamName + "=" + (fConfigValue ? "true" : "false");     // I2P router wants the words...
}

void static FormatIntI2POptionsString( std::string& I2pOptions, const std::string& I2pSamName, const std::string& confParamName ) {
    int64_t i64ConfigValue = GetArg( confParamName, 0 );

    if (!I2pOptions.empty()) I2pOptions += " ";                     // seperate the parameters with <whitespace>
    std::ostringstream oss;
    oss << i64ConfigValue;                                          // One way of converting a value to a string
    I2pOptions += I2pSamName + "=" + oss.str();                     // I2P router wants the chars....
}

void static BuildI2pOptionsString( void ) {
    std::string OptStr;

    FormatIntI2POptionsString(OptStr, SAM_NAME_INBOUND_QUANTITY       , "-i2p.options.inbound.quantity" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_INBOUND_LENGTH         , "-i2p.options.inbound.length" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_INBOUND_LENGTHVARIANCE , "-i2p.options.inbound.lengthvariance" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_INBOUND_BACKUPQUANTITY , "-i2p.options.inbound.backupquantity" );
    FormatBoolI2POptionsString(OptStr, SAM_NAME_INBOUND_ALLOWZEROHOP  , "-i2p.options.inbound.allowzerohop" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_INBOUND_IPRESTRICTION  , "-i2p.options.inbound.iprestriction" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_QUANTITY      , "-i2p.options.outbound.quantity" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_LENGTH        , "-i2p.options.outbound.length" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_LENGTHVARIANCE, "-i2p.options.outbound.lengthvariance" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_BACKUPQUANTITY, "-i2p.options.outbound.backupquantity" );
    FormatBoolI2POptionsString(OptStr, SAM_NAME_OUTBOUND_ALLOWZEROHOP , "-i2p.options.outbound.allowzerohop" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_IPRESTRICTION , "-i2p.options.outbound.iprestriction" );
    FormatIntI2POptionsString(OptStr, SAM_NAME_OUTBOUND_PRIORITY      , "-i2p.options.outbound.priority" );

    std::string ExtrasStr = GetArg( "-i2p.options.extra", "");
    if( ExtrasStr.size() ) {
        if (!OptStr.empty()) OptStr += " ";                             // seperate the parameters with <whitespace>
        OptStr += ExtrasStr;
    }
    sOptions = OptStr;                                                  // Keep this globally for use later in opening the session
}

// Initialize all the parameters with default values, if necessary.
// These should not override any loaded from the anoncoin.conf file,
// but if they are not set, then this insures that they are created with good values
// SoftSetArg will return a bool, if you care to know if the parameter was changed or not.
void InitializeI2pSettings( void ) {
    SoftSetBoolArg( "-i2p.mydestination.static", false );
    SoftSetArg( "-i2p.options.samhost", SAM_DEFAULT_ADDRESS );
    SoftSetArg( "-i2p.options.samport", "7656" );                   /* SAM_DEFAULT_PORT */
    SoftSetArg( "-i2p.options.sessionname", I2P_SESSION_NAME_DEFAULT );
    SoftSetArg( "-i2p.options.inbound.quantity", "3" );             /* SAM_DEFAULT_INBOUND_QUANTITY */
    SoftSetArg( "-i2p.options.inbound.length", "3" );               /* SAM_DEFAULT_INBOUND_LENGTH */
    SoftSetArg( "-i2p.options.inbound.lengthvariance", "0" );       /* SAM_DEFAULT_INBOUND_LENGTHVARIANCE */
    SoftSetArg( "-i2p.options.inbound.backupquantity", "1" );       /* SAM_DEFAULT_INBOUND_BACKUPQUANTITY */
    SoftSetBoolArg( "-i2p.options.inbound.allowzerohop", SAM_DEFAULT_INBOUND_ALLOWZEROHOP );
    SoftSetArg( "-i2p.options.inbound.iprestriction", "2" );        /* SAM_DEFAULT_INBOUND_IPRESTRICTION */
    SoftSetArg( "-i2p.options.outbound.quantity", "3" );            /* SAM_DEFAULT_OUTBOUND_QUANTITY */
    SoftSetArg( "-i2p.options.outbound.length", "3" );              /* SAM_DEFAULT_OUTBOUND_LENGTH */
    SoftSetArg( "-i2p.options.outbound.lengthvariance", "0" );      /* SAM_DEFAULT_OUTBOUND_LENGTHVARIANCE */
    SoftSetArg( "-i2p.options.outbound.backupquantity", "1" );      /* SAM_DEFAULT_OUTBOUND_BACKUPQUANTITY */
    SoftSetBoolArg( "-i2p.options.outbound.allowzerohop", SAM_DEFAULT_OUTBOUND_ALLOWZEROHOP );
    SoftSetArg( "-i2p.options.outbound.iprestriction", "2" );       /* SAM_DEFAULT_OUTBOUND_IPRESTRICTION */
    SoftSetArg( "-i2p.options.outbound.priority", "0" );            /* SAM_DEFAULT_OUTBOUND_PRIORITY */

    // Setup parameters required to open a new SAM session
    uPort = (uint16_t)GetArg( "-i2p.options.samport", SAM_DEFAULT_PORT );
    sSession = GetArg( "-i2p.options.sessionname", I2P_SESSION_NAME_DEFAULT );
    sHost = GetArg( "-i2p.options.samhost", SAM_DEFAULT_ADDRESS );
    // Critical to check here, if we are in dynamic destination mode, the intial session destination MUSTBE default too.
    //  Which may not be what the user has set in the anoncoin.conf file.
    if( GetBoolArg( "-i2p.mydestination.static", false ) )            // GetBoolArg returns true if the .static variable is true
        sDestination = GetArg( "-i2p.mydestination.privatekey", SAM_GENERATE_MY_DESTINATION );
    // It's important here that sDestination be setup correctly, for soon when an initial Session object is about to be
    // created.  When that happens, this variable is used to create the SAM session, upon which, after it's opened,
    // the variable is updated to reflect that ACTUAL destination being used.  Whatever that value maybe,
    // dynamically generated or statically set.  ToDo: Move sDestination into the Session class

    BuildI2pOptionsString();   // Now build the I2P options string that's need to open a session
}

std::string GetDestinationPublicKey( const std::string& sDestinationPrivateKey )
{
    return( sDestinationPrivateKey.substr(0, NATIVE_I2P_DESTINATION_SIZE) );
}

// This test should pass for both public and private keys, as the first part of the private key is the public key.
bool isValidI2pAddress( const std::string& I2pAddr ) {
    if( I2pAddr.size() < NATIVE_I2P_DESTINATION_SIZE ) return false;
    return (I2pAddr.substr( NATIVE_I2P_DESTINATION_SIZE - 4, 4 ) == "AAAA");
}

bool isValidI2pDestination( const SAM::FullDestination& DestKeys ) {

    // Perhaps we're given a I2P native public address, last 4 symbols of b64-destination must be AAAA
    bool fPublic = ((DestKeys.pub.size() == NATIVE_I2P_DESTINATION_SIZE) && isValidI2pAddress( DestKeys.pub));
    // ToDo: Add more checking on the private key, for now this will do...
    bool fPrivate = ((DestKeys.priv.size() > NATIVE_I2P_DESTINATION_SIZE) && isValidI2pAddress( DestKeys.priv));
    return fPublic && fPrivate;
}

bool isValidI2pB32( const std::string& B32Address ) {
    return (B32Address.size() == NATIVE_I2P_B32ADDR_SIZE) && (B32Address.substr(B32Address.size() - 8, 8) == ".b32.i2p");
}

std::string B32AddressFromDestination(const std::string& destination)
{
    std::string canonicalDest = destination;
    for (size_t pos = canonicalDest.find_first_of('-'); pos != std::string::npos; pos = canonicalDest.find_first_of('-', pos))
        canonicalDest[pos] = '+';
    for (size_t pos = canonicalDest.find_first_of('~'); pos != std::string::npos; pos = canonicalDest.find_first_of('~', pos))
        canonicalDest[pos] = '/';
    std::string rawHash = DecodeBase64(canonicalDest);
    uint256 hash;
    SHA256((const unsigned char*)rawHash.c_str(), rawHash.size(), (unsigned char*)&hash);
    std::string result = EncodeBase32(hash.begin(), hash.end() - hash.begin()) + ".b32.i2p";
    for (size_t pos = result.find_first_of('='); pos != std::string::npos; pos = result.find_first_of('=', pos-1))
        result.erase(pos, 1);
    return result;
}

/**
 * Functions we need for I2P functionality
 */
std::string FormatI2PNativeFullVersion() {
    // We need to NOT talk to the SAM module if I2P is unavailable
    return IsI2PEnabled() ? I2PSession::Instance().getSAMVersion() : "?.??";
}

// GR note...doodled for abit on code cleanup, trying to figure out where these best fit into the scheme of I2P implementation upon the 0.9.3 codebase,
// in the end, put them back here in net.cpp for now...the only reason was because of NATIVE_I2P_NET_STRING being defined in netbase.h, which is included
// within our net.h header, where netbase.h is included as a dependency....clear as mud?
bool IsTorOnly() {
    bool i2pOnly = false;
    const std::vector<std::string>& onlyNets = mapMultiArgs["-onlynet"];
    i2pOnly = (onlyNets.size() == 1 && onlyNets[0] == "tor");
    return i2pOnly;
}

bool IsI2POnly()
{
    bool i2pOnly = false;
    if (mapArgs.count("-onlynet")) {
        const std::vector<std::string>& onlyNets = mapMultiArgs["-onlynet"];
        i2pOnly = (onlyNets.size() == 1 && onlyNets[0] == NATIVE_I2P_NET_STRING);
    }
    return i2pOnly;
}

// If either/or dark net or if we're running a proxy or onion and in either of those cases if i2p is also enabled
bool IsDarknetOnly() {
    return IsI2POnly() || IsTorOnly() ||
            (((mapArgs.count("-proxy") && mapArgs["-proxy"] != "0") || (mapArgs.count("-onion") && mapArgs["-onion"] != "0")) &&
              (mapArgs.count("-i2p.options.enabled") && mapArgs["-i2p.options.enabled"] != "0")
            );
}

// Basically we override the -i2p.options.enabled flag here, if we are running -onlynet=i2p....
bool IsI2PEnabled() {
    return  GetBoolArg("-i2p.options.enabled", false) || IsI2POnly();
}

bool IsBehindDarknet() {
    return IsDarknetOnly() || (mapArgs.count("-onion") && mapArgs["-onion"] != "0");
}

