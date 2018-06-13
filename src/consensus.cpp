#include "consensus.h"

#include "block.h"
#include "chain.h"
#include "chainparams.h"
#include "uint256.h"
#include "amount.h"
#include "pow.h"

#include <stdint.h>

namespace CashIsKing
{

static CCriticalSection cs_consensus;

bool ANCConsensus::CheckProofOfWork(const uint256& hash, unsigned int nBits)
{}

unsigned int ANCConsensus::GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader)
{
    //! Any call to this routine needs to have at least 1 block and the header of a new block.
    //! ...NULL pointers are not allowed...GR
    assert(pindexLast);
    assert( pBlockHeader );

    //! All networks now always calculate the RetargetPID output, it needs to have been setup
    //! during initialization, if unit tests are being run, perhaps where the genesis block
    //! has been added to the index, but no pRetargetPID setup yet then we detect it here and
    //! return minimum difficulty for the next block, in any other case the programmer should
    //! have already at least created a RetargetPID class object and set the master pointer up.
    uint256 uintResult;
    if( pRetargetPid ) {
        //! Under normal conditions, update the PID output and return the next new difficulty required.
        //! We do this while locked, once the Output Result is captured, it is immediately unlocked.
        //! Based on height, perhaps during a blockchain initial load, other older algos will need to
        //! be run, and their result returned.  That is detected below and processed accordingly.
        {
            LOCK( cs_consensus );
            if( !pRetargetPid->UpdateOutput( pindexLast, pBlockHeader) )
                LogPrint( "retarget", "Insufficient BlockIndex, unable to set RetargetPID output values.\n");
            uintResult = pRetargetPid->GetRetargetOutput(); //! Always returns a limit checked valid result
            //LogPrintf("pRetargetPid->GetRetargetOutput() selected\n");
        }
        //! Testnets always use the P-I-D Retarget Controller, only the MAIN network might not...
        if( isMainNetwork() ) {
            if( pindexLast->nHeight > nDifficultySwitchHeight3 ) {      //! Start of KGW era
                //! The new P-I-D retarget algo will start at this hardfork block + 1
                if( pindexLast->nHeight <= nDifficultySwitchHeight4 )   //! End of KGW era
                {
                    //LogPrintf("NextWorkRequiredKgwV2 selected\n");
                    uintResult = NextWorkRequiredKgwV2(pindexLast);     //! Use fast v2 KGW calculator
                }
            } else {
                //LogPrintf("OriginalGetNextWorkRequired selected\n");
                uintResult = OriginalGetNextWorkRequired(pindexLast);   //! Algos Prior to the KGW era
            }
        }
    } else {
        uintResult = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );
        //LogPrintf("Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ) selected\n");
    }

    //! Finish by converting the resulting uint256 value into a compact 32 bit 'nBits' value
    return uintResult.GetCompact();
}

/**
 *  Difficulty formula, Anoncoin - From the early months, when blocks were very new...
 */
uint256 ANCConsensus::OriginalGetNextWorkRequired(const CBlockIndex* pindexLast)
{
    //! These legacy values define the Anoncoin block rate production and are used in this difficulty calculation only...
    static const int64_t nLegacyTargetSpacing = 205;    //! Originally 3.42 minutes * 60 secs was Anoncoin spacing target in seconds
    static const int64_t nLegacyTargetTimespan = 86184; //! in Seconds - Anoncoin legacy value is ~23.94hrs, it came from a
                                                        //! 420 blocks * 205.2 seconds/block calculation, now used only in original NextWorkRequired function.

    //! Anoncoin difficulty adjustment protocol switch (Thanks to FeatherCoin for this idea)
    static const int newTargetTimespan = 2050;              //! For when another adjustment in the timespan was made
    int32_t nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = nHeight >= nDifficultySwitchHeight1;
    bool fNewDifficultyProtocol2 = false;
    int64_t nTargetTimespanCurrent = nLegacyTargetTimespan;
    int64_t nLegacyInterval;

    if( nHeight >= nDifficultySwitchHeight2 ) {         //! Jump back to sqrt(2) as the factor of adjustment.
        fNewDifficultyProtocol2 = true;
        fNewDifficultyProtocol = false;
    }

    if( fNewDifficultyProtocol ) nTargetTimespanCurrent *= 4;
    if (fNewDifficultyProtocol2) nTargetTimespanCurrent = newTargetTimespan;
    nLegacyInterval = nTargetTimespanCurrent / nLegacyTargetSpacing;

    //! Only change once per interval, or at protocol switch height
    if( nHeight % nLegacyInterval != 0 && !fNewDifficultyProtocol2 && nHeight != nDifficultySwitchHeight1 )
        return uint256().SetCompact( pindexLast->nBits );

    //! Anoncoin: This fixes an issue where a 51% attack can change difficulty at will.
    //! Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nLegacyInterval-1;
    if ((pindexLast->nHeight+1) != nLegacyInterval)
        blockstogoback = nLegacyInterval;

    //! Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    blockstogoback = fNewDifficultyProtocol2 ? (newTargetTimespan/205) : blockstogoback;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    //! Limit adjustment step
    int64_t nNewSetpoint = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    int64_t nAveragedTimespanMax = fNewDifficultyProtocol ? (nTargetTimespanCurrent*4) : ((nTargetTimespanCurrent*99)/70);
    int64_t nAveragedTimespanMin = fNewDifficultyProtocol ? (nTargetTimespanCurrent/4) : ((nTargetTimespanCurrent*70)/99);
    if (pindexLast->nHeight+1 >= nDifficultySwitchHeight2) {
        if (nNewSetpoint < nAveragedTimespanMin)
            nNewSetpoint = nAveragedTimespanMin;
        if (nNewSetpoint > nAveragedTimespanMax)
            nNewSetpoint = nAveragedTimespanMax;
    } else if (pindexLast->nHeight+1 > nDifficultySwitchHeight1) {
        if (nNewSetpoint < nAveragedTimespanMin/4)
            nNewSetpoint = nAveragedTimespanMin/4;
        if (nNewSetpoint > nAveragedTimespanMax)
            nNewSetpoint = nAveragedTimespanMax;
    } else {
        if (nNewSetpoint < nAveragedTimespanMin)
            nNewSetpoint = nAveragedTimespanMin;
        if (nNewSetpoint > nAveragedTimespanMax)
            nNewSetpoint = nAveragedTimespanMax;
    }

    //! Retarget
    uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nNewSetpoint;
    bnNew /= fNewDifficultyProtocol2 ? nTargetTimespanCurrent : nLegacyTargetTimespan;
    // debug print
#if defined( LOG_DEBUG_OUTPUT )
    LogPrintf("Difficulty Retarget - pre-Kimoto Gravity Well era\n");
    LogPrintf("  TargetTimespan = %lld    ActualTimespan = %lld\n", nLegacyTargetTimespan, nNewSetpoint);
    LogPrintf("  Before: %08x  %s\n", pindexLast->nBits, uint256().SetCompact(pindexLast->nBits).ToString());
    LogPrintf("  After : %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());
#endif
    const uint256 &uintPOWlimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );
    if( bnNew > uintPOWlimit ) {
        LogPrint( "retarget", "Block at Height %d, Computed Next Work Required %0x limited and set to Minimum %0x\n", nHeight, bnNew.GetCompact(), uintPOWlimit.GetCompact());
        bnNew = uintPOWlimit;
    }
    return bnNew;
}

uint256 ANCConsensus::GetPoWRequiredForNextBlock()
{
  CBlockIndex *tip = chainActive.Tip();
  if (tip)
  {
    int32_t nHeight = tip->nHeight;
  } else {
    //TODO: FIX
    return uint256(0);
  }
}

int64_t ANCConsensus::GetBlockValue(int nHeight, int64_t nFees)
{
  int64_t nSubsidy = 5 * COIN;
  // Some adjustments to the start of the lifetime to Anoncoin
  if (nHeight < 42000) {
    nSubsidy = 4.2 * COIN;
  } else if (nHeight < 77777) { // All luck is seven ;)
    nSubsidy = 7 * COIN;
  } else if (nHeight == 77778) {
    nSubsidy = 10 * COIN;
  } else {
    nSubsidy >>= (nHeight / 306600); // Anoncoin: 306600 blocks in ~2 years
  }
  return nSubsidy + nFees;
}

/**
 * 
 * This function always return scrypt pow, until nDifficultySwitchHeight6
 * where GOST3411 take over.
 * 
 * */
uint256 ANCConsensus::GetPoWHashForThisBlock(const CBlockHeader& block)
{
  CBlockIndex *tip = chainActive.Tip();
  if (tip)
  {
    int32_t nHeight = tip->nHeight;
    if (nHeight >= nDifficultySwitchHeight6)
    {
      return block.GetGost3411Hash();
    } else {
      return block.GetHash();
    }
  } else {
    return block.GetHash();
  }
}

uint256 ANCConsensus::GetWorkProof(const uint256& uintTarget)
{
  // We need to compute 2**256 / (Target+1), but we can't represent 2**256
  // as it's too large for a uint256. However, as 2**256 is at least as large
  // as Target+1, it is equal to ((2**256 - Target - 1) / (Target+1)) + 1,
  // or ~Target / (Target+1) + 1.
  return !uintTarget.IsNull() ? (~uintTarget / (uintTarget + 1)) + 1 : ::uint256(0);
}

uint256 ANCConsensus::GetBlockProof(const ::CBlockIndex& block)
{
  ::uint256 bnTarget;
  bool fNegative;
  bool fOverflow;

  bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
  if (block.nHeight >= HARDFORK_BLOCK3) {
      
  }
  return (!fNegative && !fOverflow) ? ANCConsensus::GetWorkProof( bnTarget ) : ::uint256(0);
}

//! Difficulty Protocols have changed over the years, at specific points in Anoncoin's history the following SwitchHeight values are those blocks
//! where an event occurred which required changing the way things are calculated. aka HARDFORK
const int32_t ANCConsensus::nDifficultySwitchHeight1 = 15420;   // Protocol 1 happened here
const int32_t ANCConsensus::nDifficultySwitchHeight2 = 77777;  // Protocol 2 starts at this block
const int32_t ANCConsensus::nDifficultySwitchHeight3 = 87777;  // Protocol 3 began the KGW era
const int32_t ANCConsensus::nDifficultySwitchHeight4 = 555555;
const int32_t ANCConsensus::nDifficultySwitchHeight5 = 585555;
const int32_t ANCConsensus::nDifficultySwitchHeight6 = 900000;


}
