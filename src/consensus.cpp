#include "consensus.h"

#include "block.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "main.h"
#include "uint256.h"
#include "amount.h"
#include "pow.h"
#include "util.h"

#include <stdint.h>

namespace CashIsKing
{


//! Difficulty Protocols have changed over the years, at specific points in Anoncoin's history the following SwitchHeight values are those blocks
//! where an event occurred which required changing the way things are calculated. aka HARDFORK
const int32_t ANCConsensus::nDifficultySwitchHeight1 = 15420;   // Protocol 1 happened here
const int32_t ANCConsensus::nDifficultySwitchHeight2 = 77777;  // Protocol 2 starts at this block
const int32_t ANCConsensus::nDifficultySwitchHeight3 = 87777;  // Protocol 3 began the KGW era
const int32_t ANCConsensus::nDifficultySwitchHeight4 = 555555;
const int32_t ANCConsensus::nDifficultySwitchHeight5 = 585555;
int32_t ANCConsensus::nDifficultySwitchHeight6 = 871111;
const int32_t ANCConsensus::nDifficultySwitchHeight7 = -1; // The next era


/**
 * The primary routine which verifies a blocks claim of Proof Of Work
 */
bool ANCConsensus::CheckProofOfWork(const CBlockHeader& pBlockHeader, unsigned int nBits)
{
  bool fNegative;
  bool fOverflow;
  uint256 bnTarget;

  bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

  const uint256 proofOfWorkLimit = 
      (pBlockHeader.nHeight >= unsigned(nDifficultySwitchHeight6) ) ?
        Params().ProofOfWorkLimit( CChainParams::ALGO_GOST3411 ) 
        : Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

  uint256 hash = pBlockHeader.GetHash();

  //! Check range of the Target Difficulty value stored in a block
  if( fNegative || bnTarget == 0 || fOverflow || bnTarget > proofOfWorkLimit )
    return error("CheckProofOfWork() : nBits below minimum work (0x%s / %s)", proofOfWorkLimit.ToString(), strprintf( "0x%08x",proofOfWorkLimit.GetCompact()));

  //! Check the proof of work matches claimed amount
  if (hash > bnTarget) {
    //! There is one possibility where this is allowed, if this is TestNet and this hash is better than the minimum
    //! and the Target hash (as found in the block) is equal to the starting difficulty.  Then we assume this check
    //! is being called for one of the first mocktime blocks used to initialize the chain.
    bool fTestNet = TestNet();
    uint256 uintStartingDiff;
    if( fTestNet ) uintStartingDiff = pRetargetPid->GetTestNetStartingDifficulty();
    if( !fTestNet || hash > proofOfWorkLimit || uintStartingDiff.GetCompact() != nBits ) {
      LogPrintf( "%s : Failed. ", __func__ );
      if( fTestNet ) LogPrintf( "StartingDiff=0x%s ", uintStartingDiff.ToString() );
      LogPrintf( "Target=0x%s hash=0x%s\n", bnTarget.ToString(), hash.ToString() );
      return error("CheckProofOfWork() : hash doesn't match nBits (0x%s)", uint256().SetCompact(nBits).ToString() );
    }
  }

  return true;
}

bool ANCConsensus::SkipPoWCheck()
{
  CBlockIndex *tip = chainActive.Tip();
  // AIP09 : Fast accept early blockchain
  if (tip)
    if (tip->nHeight < nDifficultySwitchHeight5) return true;
  return false;
}

bool ANCConsensus::CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW)
{
  uint256 hash = block.GetHash();
  if (Checkpoints::IsExceptionBlock(hash))
    fCheckPOW = false;

  if (fCheckPOW && !TestNet())
    fCheckPOW = !SkipPoWCheck();
  
  if (!fCheckPOW)
    return true;

  // Check proof of work matches claimed amount
  
  if (fCheckPOW && !CheckProofOfWork(block, block.nBits))
    return state.DoS(50, error("CheckBlockHeader() : proof of work failed (%s)", hash.ToString()), REJECT_INVALID, "high-hash");

  // Check timestamp, for Anoncoin we make that less than 2hrs after the hardfork,
  // no mined block time need have a future time so large.  In fact the header can
  // not be used unless this value is reduced to mere seconds.
  int64_t nTimeLimit = GetAdjustedTime();
  CBlockIndex *tip = chainActive.Tip();

  if(tip) nTimeLimit += ( tip->nHeight < nDifficultySwitchHeight4 ) ? 2 * 60 * 60 : 15 * 60;

  if( block.GetBlockTime() > nTimeLimit )
    return state.Invalid(error("CheckBlockHeader(): block timestamp too far in the future"), REJECT_INVALID, "time-too-new");
  return true;
}

bool ANCConsensus::ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const CBlockIndex* pindexPrev)
{
    uint256 hash = block.GetHash();
    if (hash == Params().HashGenesisBlock())
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->nHeight+1;

    /**
     * 
     * Anoncoin new fast fetch
     * 
     * */

    uint256 blockHash = block.CalcSha256dHash();
    if (signed(nHeight) >= CashIsKing::ANCConsensus::nDifficultySwitchHeight6)
      blockHash = hash;

    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint();
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s : forked chain older than last checkpoint (height %d)", __func__, nHeight));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckBlock(nHeight, blockHash))
        return state.DoS(100, error("%s : rejected by checkpoint lock-in at %d", __func__, nHeight), REJECT_CHECKPOINT, "checkpoint mismatch");

    // Don't fast skip in testnet
    if (nHeight < ancConsensus.nDifficultySwitchHeight4 && !TestNet())
        return true;


    // Checking PoW for blocks not in checkpoints
    uint32_t checkPowVal = GetNextWorkRequired(pindexPrev, &block);
    uint32_t maxDiff = 32768;
    uint32_t difference = (checkPowVal > block.nBits) ? checkPowVal - block.nBits : block.nBits - checkPowVal; 
    if (block.nBits != checkPowVal && !Checkpoints::IsBlockInCheckpoints(nHeight) && !TestNet()) {
        if (nHeight < ancConsensus.nDifficultySwitchHeight6) {
            #ifdef __APPLE__
            if (!((nHeight > ancConsensus.nDifficultySwitchHeight4 && nHeight < ancConsensus.nDifficultySwitchHeight5) || difference < maxDiff)) {
                return state.Invalid(error("%s : incorrect Scrypt proof of work block %d", __func__,nHeight), REJECT_INVALID, "bad-diffbits");
            }
            #else
            return state.Invalid(error("%s : incorrect Scrypt proof of work block %d", __func__,nHeight), REJECT_INVALID, "bad-diffbits"); 
            #endif  
        }
        else {
            return state.Invalid(error("%s : incorrect Gost proof of work block %d", __func__,nHeight), REJECT_INVALID, "bad-diffbits"); 
        }
    }       





    // Check timestamp against prev
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.Invalid(error("%s : block's timestamp is too early", __func__),
                             REJECT_INVALID, "time-too-old");

    // Reject block.nVersion=1 blocks
    // This code has been taken from the v0.8.5.5 source, and does not work, because the IsSuperMajority test was
    // set to always return false.  As of 3/15/2015 in the last 10000 blocks over 700 where version 1 blocks
    if( block.nVersion < 2 && nHeight > ancConsensus.nDifficultySwitchHeight4 )                // We'll start enforcing the new rule
        return state.Invalid(error("%s : rejected nVersion=1 block", __func__), REJECT_OBSOLETE, "bad-version");

    if( block.nVersion < 3 && nHeight > ancConsensus.nDifficultySwitchHeight6 )                // We'll start enforcing the new rule
        return state.Invalid(error("%s : rejected nVersion=2 block", __func__), REJECT_OBSOLETE, "bad-version");

    return true;
}


/***
 * 
 * Mainnet hardforks are tracked here
 * 
 * 
 ***/
void ANCConsensus::getMainnetStrategy(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, uint256& uintResult)
{
  if ( pindexLast->nHeight > nDifficultySwitchHeight3 )
  { //! Start of KGW era
    //! The new P-I-D retarget algo will start at this hardfork block + 1
    if ( pindexLast->nHeight <= nDifficultySwitchHeight4 ) //! End of KGW era
    {
      uintResult = NextWorkRequiredKgwV2(pindexLast); //! Use fast v2 KGW calculator
    } 
    else if ( pindexLast->nHeight + 1 == nDifficultySwitchHeight6 ) {
      uint256 hashTarget;
      hashTarget.SetCompact(0x1e0ffff0);
      uintResult = hashTarget;
    } 
    else if (pindexLast->nHeight + 1 > nDifficultySwitchHeight6) {
      uintResult = NextWorkRequiredKgwV2(pindexLast);
    }
  } else {
    uintResult = OriginalGetNextWorkRequired(pindexLast);   //! Algos Prior to the KGW era
  }
}

/***
 * 
 * Testnet hardforks are tracked here
 * 
 * 
 ***/
void ANCConsensus::getTestnetStrategy(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, uint256& uintResult)
{
  if ( pindexLast->nHeight + 1 == nDifficultySwitchHeight6 ) {
    uint256 hashTarget;
    hashTarget.SetCompact(0x1e0ffff0);
    uintResult = hashTarget;
  } 
  else if (pindexLast->nHeight + 1 > nDifficultySwitchHeight6) {
    uintResult = NextWorkRequiredKgwV2(pindexLast);
  }
}

unsigned int ANCConsensus::GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader)
{
  //! Any call to this routine needs to have at least 1 block and the header of a new block.
  //! ...NULL pointers are not allowed...GR
  assert( pindexLast );
  assert( pBlockHeader );

  //! All networks now always calculate the RetargetPID output, it needs to have been setup
  //! during initialization, if unit tests are being run, perhaps where the genesis block
  //! has been added to the index, but no pRetargetPID setup yet then we detect it here and
  //! return minimum difficulty for the next block, in any other case the programmer should
  //! have already at least created a RetargetPID class object and set the master pointer up.
  uint256 uintResult;
  if ( pRetargetPid ) {
    //! Under normal conditions, update the PID output and return the next new difficulty required.
    //! We do this while locked, once the Output Result is captured, it is immediately unlocked.
    //! Based on height, perhaps during a blockchain initial load, other older algos will need to
    //! be run, and their result returned.  That is detected below and processed accordingly.
    {
      if (pindexLast->nHeight < nDifficultySwitchHeight6)
      {
        LOCK( cs_retargetpid );
        if( !pRetargetPid->UpdateOutput( pindexLast, pBlockHeader) )
          LogPrint( "retarget", "Insufficient BlockIndex, unable to set RetargetPID output values.\n");
        uintResult = pRetargetPid->GetRetargetOutput(); //! Always returns a limit checked valid result
      }
      //LogPrintf("pRetargetPid->GetRetargetOutput() selected\n");
    }
  } else {
    uintResult = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );
    //LogPrintf("Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ) selected\n");
  }
  
  // Desperate attempt to clean up some code.
  if ( isMainNetwork() ) {
    getMainnetStrategy(pindexLast, pBlockHeader, uintResult);
  } else {
    getTestnetStrategy(pindexLast, pBlockHeader, uintResult);
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
    if (bShouldDebugLogPoW)
    {
      LogPrintf("Difficulty Retarget - pre-Kimoto Gravity Well era\n");
      LogPrintf("  TargetTimespan = %lld    ActualTimespan = %lld\n", nLegacyTargetTimespan, nNewSetpoint);
      LogPrintf("  Before: %08x  %s\n", pindexLast->nBits, uint256().SetCompact(pindexLast->nBits).ToString());
      LogPrintf("  After : %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());
    }

    const uint256 &uintPOWlimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );
    if( bnNew > uintPOWlimit ) {
      LogPrint( "retarget", "Block at Height %d, Computed Next Work Required %0x limited and set to Minimum %0x\n", nHeight, bnNew.GetCompact(), uintPOWlimit.GetCompact());
      bnNew = uintPOWlimit;
    }
    return bnNew;
}

uint256 ANCConsensus::GetPoWRequiredForNextBlock()
{
  if (IsUsingGost3411Hash())
  {
    //TODO: FIX
    return uint256(0);
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

bool ANCConsensus::IsUsingGost3411Hash()
{
  CBlockIndex *tip = chainActive.Tip();
  if (tip)
  {
    int32_t nHeight = tip->nHeight;
    if (nHeight >= nDifficultySwitchHeight6)
    {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

/**
 * 
 * This function always return scrypt pow, until nDifficultySwitchHeight6
 * where GOST3411 take over.
 * 
 * */
uint256 ANCConsensus::GetPoWHashForThisBlock(const CBlockHeader& block)
{
  if (signed(block.nHeight) >= nDifficultySwitchHeight6)
  {
    return block.GetGost3411Hash();
  } else {
    return block.GetHash();
  }
}

//! Primary routine and methodology used to convert an unsigned 256bit value to something
//! small enough which can be converted to a double floating point number and represent
//! Difficulty in a meaningful way.  The only value not allowed is 0.  This process inverts
//! Difficulty value, which is normally thought of as getting harder as the value gets
//! smaller.  Larger proof values, indicate harder difficulty, and visa-versa...
uint256 ANCConsensus::GetWorkProof(const uint256& uintTarget)
{
  // We need to compute 2**256 / (Target+1), but we can't represent 2**256
  // as it's too large for a uint256. However, as 2**256 is at least as large
  // as Target+1, it is equal to ((2**256 - Target - 1) / (Target+1)) + 1,
  // or ~Target / (Target+1) + 1.
  return !uintTarget.IsNull() ? (~uintTarget / (uintTarget + 1)) + 1 : ::uint256(0);
}

//! The primary method of providing difficulty to the user requires the use of a logarithm scale, that can not be done
//! for 256bit numbers unless they are first converted to a work proof, then its value can be returned as a double
//! floating point value which is meaningful.
double ANCConsensus::GetLog2Work( const uint256& uintDifficulty )
{
  double dResult = 0.0;   //! Return zero, if there are any errors

  uint256 uintWorkProof = GetWorkProof( uintDifficulty );
  if( !uintWorkProof.IsNull() ) {
    double dWorkProof = uintWorkProof.getdouble();
    dResult = (dWorkProof != 0.0) ? log(dWorkProof) / log(2.0) : 0.0;
  }
  return dResult;
}

uint256 ANCConsensus::GetBlockProof(const ::CBlockIndex& block)
{
  ::uint256 bnTarget;
  bool fNegative;
  bool fOverflow;

  bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
  return (!fNegative && !fOverflow) ? GetWorkProof( bnTarget ) : ::uint256(0);
}

} // End namespace CashIsKing
