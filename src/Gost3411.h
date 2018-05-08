/*
* Copyright (c) 2013-2018, The PurpleI2P Project
*
* This file is part of Purple i2pd project and licensed under BSD3
*
*/

#ifndef GOST3411_H__
#define GOST3411_H__

#include <memory>
#include <inttypes.h>
#include <openssl/ec.h>

namespace i2p
{
namespace crypto
{

// Big Endian
	void GOSTR3411_2012_256 (const uint8_t * buf, size_t len, uint8_t * digest);
	void GOSTR3411_2012_512 (const uint8_t * buf, size_t len, uint8_t * digest);
}
}

#endif
