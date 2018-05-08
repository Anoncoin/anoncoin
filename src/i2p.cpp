#include <map>
#include <mutex>
#include <condition_variable>
#include "util.h"
#include "Log.h"
#include "net.h"
#include "i2p.h"

I2PSession::I2PSession()
{
	auto dest = GetArg(I2P_SAM_MY_DESTINATION_PARAM, "");
	if (dest.length () > 0)
	{
		m_Keys.FromBase64 (dest);
		m_IsSet = true;	
	}	
	else
		GenerateKeys ();
}

I2PSession::~I2PSession()
{
	Stop ();
}

/*static*/
std::string I2PSession::GenerateB32AddressFromDestination(const std::string& destination)
{
	i2p::data::IdentityEx identity;
	identity.FromBase64 (destination);
	return identity.GetIdentHash ().ToBase32 () + ".b32.i2p";
}

const i2p::data::PrivateKeys& I2PSession::GenerateKeys ()
{
	m_IsSet = true;	
	m_Keys = i2p::data::CreateRandomKeys ();
	return m_Keys;
}

std::string I2PSession::GetB32Address () const
{
	return m_Keys.GetPublic ()->GetIdentHash ().ToBase32 () + ".b32.i2p"; 
}

void I2PSession::Start ()
{
	std::map<std::string, std::string> params;	
	params[i2p::client::I2CP_PARAM_INBOUND_TUNNELS_QUANTITY] = std::to_string (GetArg(i2p::client::I2CP_PARAM_INBOUND_TUNNELS_QUANTITY, i2p::client::DEFAULT_INBOUND_TUNNELS_QUANTITY));	
	params[i2p::client::I2CP_PARAM_INBOUND_TUNNEL_LENGTH] = std::to_string (GetArg(i2p::client::I2CP_PARAM_INBOUND_TUNNEL_LENGTH, i2p::client::DEFAULT_INBOUND_TUNNEL_LENGTH));
	params[i2p::client::I2CP_PARAM_OUTBOUND_TUNNELS_QUANTITY] = std::to_string (GetArg(i2p::client::I2CP_PARAM_OUTBOUND_TUNNELS_QUANTITY, i2p::client::DEFAULT_OUTBOUND_TUNNELS_QUANTITY));	
	params[i2p::client::I2CP_PARAM_OUTBOUND_TUNNEL_LENGTH] = std::to_string (GetArg(i2p::client::I2CP_PARAM_OUTBOUND_TUNNEL_LENGTH, i2p::client::DEFAULT_OUTBOUND_TUNNEL_LENGTH));		
	m_LocalDestination = i2p::api::CreateLocalDestination (m_Keys, true, &params);

	m_LocalDestination->AcceptStreams (std::bind (&I2PSession::HandleAccept, this, std::placeholders::_1));
}

void I2PSession::Stop ()
{
	if (m_LocalDestination)
	{
		i2p::api::DestroyLocalDestination (m_LocalDestination);
		m_LocalDestination = nullptr;
	}
}

void I2PSession::HandleAccept (std::shared_ptr<i2p::stream::Stream> stream)
{
	if (stream)
		AddIncomingI2PStream (stream);
}

std::shared_ptr<const i2p::data::LeaseSet> I2PSession::RequestLeaseSet (const i2p::data::IdentHash& ident)
{
	if (!m_LocalDestination) return nullptr;
	std::condition_variable responded;
	std::mutex respondedMutex;
	bool notified = false;
	auto leaseSet = m_LocalDestination->FindLeaseSet (ident);
	if (!leaseSet)
	{
		std::unique_lock<std::mutex> l(respondedMutex);
		m_LocalDestination->RequestDestination (ident,
		[&responded, &leaseSet, &notified](std::shared_ptr<i2p::data::LeaseSet> ls)
	    {
			leaseSet = ls;
			notified = true;
			responded.notify_all ();
		});
		auto ret = responded.wait_for (l, std::chrono::seconds (I2P_STREAM_CONNECT_TIMEOUT));
		if (!notified ) // unsolicited wakeup
			 m_LocalDestination->CancelDestinationRequest (ident);
		if (ret == std::cv_status::timeout)
		{
			// most likely it shouldn't happen
			LogPrint (eLogError, "I2PSession LeaseSet request timeout expired");
			return nullptr;
		}
	}
	return leaseSet;
}

std::shared_ptr<i2p::stream::Stream> I2PSession::Connect (const i2p::data::IdentHash& ident)
{
	LogPrint (eLogInfo, "Connecting to peer address ", ident.ToBase32 ());	
	auto leaseSet = RequestLeaseSet (ident);
	if (leaseSet)
	{
		LogPrint (eLogInfo, "Connected to peer address ", ident.ToBase32 ());	
		return m_LocalDestination->CreateStream (leaseSet);
	}
	else
		LogPrint (eLogInfo, "Peer address ", ident.ToBase32 (), " not found");	
	return nullptr;
}

std::string I2PSession::NamingLookup (const std::string& b32)
{
	LogPrint (eLogInfo, "Naming lookup of ", b32);
	i2p::data::IdentHash ident;
	ident.FromBase32 (b32);
	auto leaseSet = RequestLeaseSet (ident);	
	return leaseSet ? leaseSet->GetIdentity ()->ToBase64 () : ""; 
}
