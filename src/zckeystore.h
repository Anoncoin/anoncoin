#ifndef _ZCKEYSTORE_H_
#define _ZCKEYSTORE_H_

#include <map>
#include <string>
#include "zerocoin/Coin.h"   // PrivateCoin

typedef std::map<std::string, libzerocoin::PrivateCoin> zPrivCoinMap;

class ZCKeyStore
{
public:
	ZCKeyStore();
	~ZCKeyStore();

private:
	zPrivCoinMap m_PrivateCoins;
};



#endif

