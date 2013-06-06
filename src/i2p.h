// Copyright (c) 2012-2013 giv
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//--------------------------------------------------------------------------------------------------
#ifndef I2P_H
#define I2P_H

#include "util.h"
#include "i2psam.h"

#define I2P_SESSION_NAME_PARAM          "-i2psessionname"
#define I2P_SESSION_NAME_DEFAULT        "Bitcoin-client"

#define I2P_SAM_HOST_PARAM              "-samhost"
#define I2P_SAM_HOST_DEFAULT            SAM_DEFAULT_ADDRESS

#define I2P_SAM_PORT_PARAM              "-samport"
#define I2P_SAM_PORT_DEFAULT            SAM_DEFAULT_PORT

#define I2P_SAM_MY_DESTINATION_PARAM    "-mydestination"
#define I2P_SAM_MY_DESTINATION_DEFAULT  SAM_GENERATE_MY_DESTINATION

#define I2P_SAM_GENERATE_DESTINATION_PARAM "-generatei2pdestination"

class I2PSession : private SAM::StreamSession
{
private:
    I2PSession()
        : SAM::StreamSession(
              GetArg(I2P_SESSION_NAME_PARAM, I2P_SESSION_NAME_DEFAULT),
              GetArg(I2P_SAM_HOST_PARAM, I2P_SAM_HOST_DEFAULT),
              (uint16_t)GetArg(I2P_SAM_PORT_PARAM, I2P_SAM_PORT_DEFAULT),
              GetArg(I2P_SAM_MY_DESTINATION_PARAM, I2P_SAM_MY_DESTINATION_DEFAULT))
    {}
    ~I2PSession() {}

    I2PSession(const I2PSession&);
    I2PSession& operator=(const I2PSession&);
public:
    // In C++11 this code is thread safe, in C++03 it isn't
    static SAM::StreamSession& Instance()
    {
        static I2PSession i2pSession;
        return i2pSession;
    }

    static std::string GenerateB32AddressFromDestination(const std::string& destination)
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
};

#endif // I2P_H
