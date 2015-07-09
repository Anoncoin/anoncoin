// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_POW_H
#define ANONCOIN_POW_H

#include "chainparams.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

extern const int64_t nTargetSpacing;

/** \brief Takes a block version int type, and decides which Algo was used for it.
 *
 * \param nVersion int
 * \return the enumeration type CChainParams::MinedWithAlgo
 *
 */
CChainParams::MinedWithAlgo GetAlgo(int nVersion);

/** \brief Returns a string, that represents a give algo name
 *
 * \param Algo CChainParams::MinedWithAlgo
 * \return std::string
 *
 */
std::string GetAlgoName(CChainParams::MinedWithAlgo Algo);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits);

uint256 GetBlockProof(const CBlockIndex& block);

//! Hunt back in time from the given block index, to find the last block mined of the given algo,
const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, CChainParams::MinedWithAlgo mwa);

#endif // ANONCOIN_POW_H
