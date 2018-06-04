// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_CHECKPOINT_H
#define ANONCOIN_CHECKPOINT_H

#include <map>

class CBlockIndex;
class uintFakeHash;

/** Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{
    // Returns true if block passes checkpoint checks
    bool CheckBlock(int nHeight, const uintFakeHash& hash);

    // Return conservative estimate of total number of blocks, 0 if unknown
    int GetTotalBlocksEstimate();

    // Anc fix
    bool IsBlockInCheckpoints(int nHeight);

    // Returns last CBlockIndex* in mapBlockIndex that is a checkpoint
    CBlockIndex* GetLastCheckpoint();

    double GuessVerificationProgress(CBlockIndex *pindex, bool fSigchecks = true);

    extern bool fEnabled;
}

#endif // header guard
