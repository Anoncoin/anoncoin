#ifndef __RPCSTEALTH_H__
#define __RPCSTEALTH_H__
#include "rpcserver.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "json/json_spirit_value.h"

using namespace json_spirit;

// main.cpp
extern bool IsStandardTx(const CTransaction& tx, std::string& reason);

Value getnewstealthaddress(const Array& params, bool fHelp);
Value liststealthaddresses(const Array& params, bool fHelp);
Value importstealthaddress(const Array& params, bool fHelp);
Value sendtostealthaddress(const Array& params, bool fHelp);
Value clearwallettransactions(const Array& params, bool fHelp);
Value scanforalltxns(const Array& params, bool fHelp);
Value scanforstealthtxns(const Array& params, bool fHelp);

#endif