#ifndef I2P_H
#define I2P_H
#ifdef ENABLE_I2PD
#include <string>
#include <memory>
#include <thread>
#include <boost/asio.hpp>
#include "api.h" //i2pd

#define I2P_NET_NAME_PARAM              "-i2p"

#define I2P_SESSION_NAME_PARAM          "-i2psessionname"
#define I2P_SESSION_NAME_DEFAULT        "Anoncoin-client"

#define I2P_SAM_HOST_PARAM              "-samhost"
#define I2P_SAM_HOST_DEFAULT            "127.0.0.1"

#define I2P_SAM_PORT_PARAM              "-samport"
#define I2P_SAM_PORT_DEFAULT            7656

#define I2P_SAM_MY_DESTINATION_PARAM    "-mydestination"
#define I2P_SAM_MY_DESTINATION_DEFAULT  "TRANSIENT"

#define I2P_SAM_I2P_OPTIONS_PARAM       "-i2poptions"
#define I2P_SAM_I2P_OPTIONS_DEFAULT     ""

#define I2P_SAM_GENERATE_DESTINATION_PARAM "-generatei2pdestination"

const int I2P_STREAM_CONNECT_TIMEOUT = 30;

class I2PSession 
{
public:
    // In C++11 this code is thread safe, in C++03 it isn't
    static I2PSession& Instance()
    {
        static I2PSession i2pSession;
        return i2pSession;
    }	

    static std::string GenerateB32AddressFromDestination(const std::string& destination);

	const i2p::data::PrivateKeys& GenerateKeys ();
	const i2p::data::PrivateKeys& GetKeys () const { return m_Keys; };	
	bool IsSet () const { return m_IsSet; };
	bool IsReady () const { return m_LocalDestination ? m_LocalDestination->IsReady () : false; }	
	std::shared_ptr<i2p::client::ClientDestination> GetLocalDestination () const { return m_LocalDestination; };
	std::string GetB32Address () const;	

	void Start ();
	void Stop ();

	std::shared_ptr<i2p::stream::Stream> Connect (const i2p::data::IdentHash& ident);
	std::string NamingLookup (const std::string& b32);

private:

    I2PSession();
    ~I2PSession();

	std::shared_ptr<const i2p::data::LeaseSet> RequestLeaseSet (const i2p::data::IdentHash& ident);
	void HandleAccept (std::shared_ptr<i2p::stream::Stream> stream);

private:

	bool m_IsSet;
	i2p::data::PrivateKeys m_Keys;
	std::shared_ptr<i2p::client::ClientDestination> m_LocalDestination;
};
#endif
#endif // I2P_H
