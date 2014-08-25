#ifndef _ZCKEYSTORE_H_
#define _ZCKEYSTORE_H_

#ifdef ENABLE_ZEROCOIN

#include <map>
#include <string>
// PrivateCoin
#include "zerocoin/Coin.h"

typedef std::map<std::string, PrivateCoin> zPrivCoinMap;

class ZCKeyStore
{
public:
	ZCKeyStore();
	~ZCKeyStore();

private:
	zPrivCoinMap m_PrivateCoins;
};



#endif // Enable ZC

#endif

