// ToDo: Remove this file as an include in main.cpp.
//       Rename it to something better & write a header file for it.
//       Then develop the associated build script updates so it compiles & links in with everything else...
//

// #define LOG_DEBUG_OUTPUT

// This value defines the Anoncoin block rate production, difficulty calculations and Proof Of Work functions should use this as the goal...
// nTargetSpacing = 180;      and the definition has been moved to main.h for global visibility

// Difficulty Protocols have changed over time, at specific points in Anoncoin's history
// the following SwitchHeight values are those blocks where an event occurred which
// required changing in the way things were calculated. aka HARDFORK
static const int nDifficultySwitchHeight = 15420;   // Protocol 1 happened here
static const int nDifficultySwitchHeight2 = 77777;  // Protocol 2 starts at this block
static const int nDifficultySwitchHeight3 = 87777;  // Protocol 3 began the KGW era
#if defined( HARDFORK_BLOCK )
static const int nDifficultySwitchHeight4 = HARDFORK_BLOCK;
#endif

#define SECONDSPERDAY (60 * 60 * 24)  // This value is used in various difficulty calculation routines, was stored, should have just been DEFINED for the compiler.

/*
 *  Difficulty formula, Anoncoin - From the early months, when blocks were very new...
 */
unsigned int static OldGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit)
{
    // These legacy values define the Anoncoin block rate production and are used in this difficulty calculation only...
    static const int64_t nLegacyTargetSpacing = 205;    // Originally 3.42 minutes * 60 secs was Anoncoin spacing target in seconds
    static const int64_t nLegacyTargetTimespan = 86184; // in Seconds - Anoncoin legacy value is ~23.94hrs, it came from a
                                                        // 420 blocks * 205.2 seconds/block calculation, now used only in original NextWorkRequired function.

    if (pindexLast == NULL)                                 // If the last block was the Genesis block, we're done
        return bnProofOfWorkLimit.GetCompact();

    // Anoncoin difficulty adjustment protocol switch (Thanks to FeatherCoin for this idea)
    static const int newTargetTimespan = 2050;              // For when another adjustment in the timespan was made
    int nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = nHeight >= nDifficultySwitchHeight;
    bool fNewDifficultyProtocol2 = false;
    int64_t nTargetTimespanCurrent = nLegacyTargetTimespan;
    int64_t nLegacyInterval;

    if( nHeight >= nDifficultySwitchHeight2 ) {         // Jump back to sqrt(2) as the factor of adjustment.
        fNewDifficultyProtocol2 = true;
        fNewDifficultyProtocol = false;
    }

    if( fNewDifficultyProtocol ) nTargetTimespanCurrent *= 4;
    if (fNewDifficultyProtocol2) nTargetTimespanCurrent = newTargetTimespan;
    nLegacyInterval = nTargetTimespanCurrent / nLegacyTargetSpacing;

    // Only change once per interval, or at protocol switch height
    if( nHeight % nLegacyInterval != 0 && !fNewDifficultyProtocol2 && nHeight != nDifficultySwitchHeight )
        return pindexLast->nBits;

    // Anoncoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nLegacyInterval-1;
    if ((pindexLast->nHeight+1) != nLegacyInterval)
        blockstogoback = nLegacyInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    blockstogoback = fNewDifficultyProtocol2 ? (newTargetTimespan/205) : blockstogoback;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nNewSetpoint = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    int64_t nAveragedTimespanMax = fNewDifficultyProtocol ? (nTargetTimespanCurrent*4) : ((nTargetTimespanCurrent*99)/70);
    int64_t nAveragedTimespanMin = fNewDifficultyProtocol ? (nTargetTimespanCurrent/4) : ((nTargetTimespanCurrent*70)/99);
    if (pindexLast->nHeight+1 >= nDifficultySwitchHeight2) {
        if (nNewSetpoint < nAveragedTimespanMin)
            nNewSetpoint = nAveragedTimespanMin;
        if (nNewSetpoint > nAveragedTimespanMax)
            nNewSetpoint = nAveragedTimespanMax;
    } else if (pindexLast->nHeight+1 > nDifficultySwitchHeight) {
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

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nNewSetpoint;
    bnNew /= fNewDifficultyProtocol2 ? nTargetTimespanCurrent : nLegacyTargetTimespan;
    if (bnNew > bnProofOfWorkLimit) bnNew = bnProofOfWorkLimit;

    // debug print
#if defined( LOG_DEBUG_OUTPUT )
    LogPrintf("Difficulty Retarget - pre-Kimoto Gravity Well era\n");
    LogPrintf("  TargetTimespan = %lld    ActualTimespan = %lld\n", nLegacyTargetTimespan, nNewSetpoint);
    LogPrintf("  Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString());
    LogPrintf("  After : %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString());
#endif
    return bnNew.GetCompact();
}

/*
 *  Difficulty formula, Anoncoin - kimoto gravity well, use of this ended early in 2015
 */
unsigned int static GetNextWorkRequiredForKgw(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit)
{
    uint64_t PastBlocksMin = (SECONDSPERDAY * 0.25) / nTargetSpacing;
    uint64_t PastBlocksMax = (SECONDSPERDAY * 7) / nTargetSpacing;

    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;

    uint64_t    PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double  PastRateAdjustmentRatio = double(1);
    CBigNum PastDifficultyAverage;
    CBigNum PastDifficultyAveragePrev;
    double  EventHorizonDeviation;
    double  EventHorizonDeviationFast;
    double  EventHorizonDeviationSlow;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin)
        return bnProofOfWorkLimit.GetCompact();

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if( PastBlocksMax > 0 && i > PastBlocksMax )
            break;
        PastBlocksMass++;

        if (i == 1)
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
        else
            PastDifficultyAverage = ((CBigNum().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;
        PastDifficultyAveragePrev = PastDifficultyAverage;
        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = nTargetSpacing * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if (PastRateActualSeconds < 0)
            PastRateActualSeconds = 0;
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        EventHorizonDeviation = 1.0 + (0.7084 * pow((double(PastBlocksMass)/double(144.0)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1.0 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) {
                assert(BlockReading);
                break;
            }
        }
        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    CBigNum bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }
    if( bnNew > bnProofOfWorkLimit ) bnNew = bnProofOfWorkLimit;

    // debug print
#if defined( LOG_DEBUG_OUTPUT )
    LogPrintf("Difficulty Retarget - Kimoto Gravity Well era\n");
    LogPrintf("  PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
    LogPrintf("  Before: %08x %s\n", BlockLastSolved->nBits, CBigNum().SetCompact(BlockLastSolved->nBits).getuint256().ToString().c_str());
    LogPrintf("  After : %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());
#endif
    return bnNew.GetCompact();
}

// K1773R and Cryptoslave eyes only...
// It maybe that all these comments in the code below get deleted before posting it to github, leaving the readers to figure it out on their own,
// have not made a decision on that yet.  Nor has this code yet been testnet'd by me here locally, we are fiddling with the values used and even considering the possibility
// of switching to the mining of two algos, SHA256D in addition to Scrypt, have not looked into the hours required yet to complete that task, and implement a new version #3 block
// type as proposed by the code below, the work done for that so far, is only the tip of the iceberg, as far as getting the full implementation of a new block header type to work
// across the board...
//
// K1773R wanted you to see the link, and shortened text description below, when we last talked, it had not been discovered yet.  Cryptoslave found an important
// explanation of why DigiByte has a larger value (16%) for decreasing the difficulty adjustment, than the limit imposed for increasing (8%) a difficulty
// adjustment.  Until now, we could not figure out if it was a coding mistake, or why it seemed backwards.  The full link is definitely worth reading, if your
// interested in the topic.
//
#if defined( HARDFORK_BLOCK )
/***
 * http://www.reddit.com/r/Digibyte/comments/213t7b/what_is_digishield_how_it_works_to_retarget/
 *
 * The secret to DigiShield is an asymmetrical approach to difficulty re-targeting. With DigiShield, the difficulty is allowed to decrease in larger movements
 * than it is allowed to increase from block to block. This keeps a blockchain from getting "stuck" i.e., not finding the next block for several hours following
 * a major drop in the net hash of coin. It is all a balancing act. You need to allow the difficulty to increase enough between blocks to catch up to a sudden
 * spike in net hash, but not enough to accidentally send the difficulty sky high when two miners get lucky and find blocks back to back. The same thing occurs
 * with difficulty decreases. Since it takes much longer to find the next block, you need to allow it to drop quicker than it increases.
 *
 * We found that the difficulty needed to be able to decrease by a larger magnitude than it was allowed to increase. When the difficulty was allowed to increase
 * or decrease at the same rate with larger orders of magnitude, some very bad oscillations occurred along with some crazy high difficulties when two lucky blocks
 * were found quickly back to back. The asymmetrical adjustments keep the difficulty from going to high to fast, but allow it to drop much quicker after a large
 * hash down swing as it takes a much longer time to discover the next two blocks for the difficulty adjustment to occur.
 * You don't want to let the difficulty go to high to fast, but you need to give it enough room to catch up quickly. The same thing goes with down swings, since
 * it takes longer to discover new blocks you need to give it more room to go down, but not enough to send it to the floor.
 *
 **/

// MultiAlgo Target contants.
// #define ALGO_COUNT_BEING_USED CChainParams::MAX_ALGO_TYPES
// static const int64_t nTargetTimespan = SECONDSPERDAY / 10;              // in Seconds, 8640  ToDo: Setting this to 1/10 of a day is arbitary, consider alternatives..
// static const int64_t nInterval = nTargetTimespan / nTargetSpacing;      // in Blocks, 48
// static const int64_t nLocalDifficultyAdjustment = 4;                 // Used in per algo diff adjs, as a percentage: 4%
#define ALGO_COUNT_BEING_USED 1
#define MULTIALGO_AVERAGING_INTERVAL (CBlockIndex::nMedianTimeSpan - 1)         // in blocks (10) This value is one less nMedianTimeSpan...
#define MULTIALGO_TARGET_SPACING  ALGO_COUNT_BEING_USED * (nTargetSpacing/2)    // in seconds, Anoncoin with 1 algo = target spacing is 90 seconds
                                                                                // Note: nTargetSpacing is defined elsewhere in the code as 180 seconds

static const int64_t nIntegratedTargetTimespan = MULTIALGO_AVERAGING_INTERVAL * MULTIALGO_TARGET_SPACING;   // in Seconds 10x2x90 = 1800 or 900 for 1 algo
static const int64_t nIncreasingDifficultyLimit = ( nIntegratedTargetTimespan * (100 - 12) ) / 100;         // Adjustment limit per retarget can be 12%
static const int64_t nDecreasingDifficultyLimit = ( nIntegratedTargetTimespan * (100 + 16) ) / 100;         // Adjustment limit per retarget can be 16%

// New Block version type #3 definitions we'll be using
enum BlockMinedWith {
    BLOCK_VERSION_DEFAULT        = 3,           // Merged mined blocks, up the default block version by one, to version #3, and default to algo SCRYPT
    BLOCK_VERSION_SHA256D        = (1 << 8) | 3,// The next byte up, now represents the algo this block was mined with, if zero then it was SCRYPT and simply is a version 3 block
    BLOCK_VERSION_ALGOS          = (7 << 8) | 3 // This mask can be used to expose the 3 bits in the byte above version #, allowing for up to 7 algos
};

// GetAlgo takes the simple nVersion field (int), found in every block, and decides which Algo was used for it.
// Returns the enumeration type, as defined in the chainparam source files
CChainParams::MinedWithAlgo GetAlgo(int nVersion)
{
    CChainParams::MinedWithAlgo mwa;
    switch( (BlockMinedWith)nVersion )
    {
        case BLOCK_VERSION_SHA256D:
            mwa = CChainParams::ALGO_SHA256D;
        default: mwa = CChainParams::ALGO_SCRYPT;     // Version 1, 2 or anything else, we set the Algo returned to SCRYPT
    }
    return mwa;
}

std::string GetAlgoName(CChainParams::MinedWithAlgo Algo)
{
    switch (Algo)
    {
        case CChainParams::ALGO_SCRYPT:
            return std::string("scrypt");
        case CChainParams::ALGO_SHA256D:
            return std::string("sha256d");
    }
    return std::string("unknown");
}

CChainParams::MinedWithAlgo GetBlockAlgo( const CBlockIndex* pIndex ) { return GetAlgo( pIndex->nVersion ); }
/**
 * Here we hunt back in time from the given block index, to the newest block mined of the given algo,
 * if the block index search takes us back to the genesis or before the HARDFORK_BLOCK, then we know the algo was SCRYPT,
 * and can stop looking any further...
 */
const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, CChainParams::MinedWithAlgo mwa);

const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, CChainParams::MinedWithAlgo mwa)
{
    // Here we walk back the linked block list, trying to find the newest one mined with the Algo parameter provided.
    // As there were no other mined block types before the hardfork, its pointless to keep looking if that algo is
    // not Scrypt...
    while( pindex && (mwa == CChainParams::ALGO_SCRYPT || pindex->nHeight > HARDFORK_BLOCK) )
    {
        if( GetBlockAlgo( pindex ) == mwa )
            return pindex;
        pindex = pindex->pprev;
    }
    return NULL;
}

/*
 *  Difficulty formula, Anoncoin - Early in 2015, due to continous attack & weakness exploits known about the KGW difficulty algo, this was implemented as a solution
 */
unsigned int static GetNextWorkRequiredAfterKgw(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit )
{
    CChainParams::MinedWithAlgo algo = CChainParams::ALGO_SCRYPT;      // Temporary variable for now with just one algo

    // find first block in averaging interval
    // Setup pindexFirst by going back until we point at the block which is MULTIALGO_AVERAGING_INTERVAL seconds ago, more algos multiplies this value by that interval
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < ALGO_COUNT_BEING_USED * MULTIALGO_AVERAGING_INTERVAL; i++)
        pindexFirst = pindexFirst->pprev;

    // Setup pPrevBlockSameAlgo to point at the last block mined with this Algo, for single algo mining this will be the previous block
    // So this simply sets up pPrevBlockSameAlgo to point at the same pindexLast block.
    const CBlockIndex* pPrevBlockSameAlgo = GetLastBlockIndexForAlgo(pindexLast, algo);
    if( pindexFirst == NULL || pPrevBlockSameAlgo == NULL )         // Not must more we can do then...
        return bnProofOfWorkLimit.GetCompact();                 // not enough blocks available

    LogPrintf("AncSheild-P1v0.1 RETARGET: Integrating constants in seconds: TimeSpan=%lld, Difficulty UpLimit=%lld, DownLimit=%lld\n", nIntegratedTargetTimespan, nIncreasingDifficultyLimit, nDecreasingDifficultyLimit );

    // Use medians to prevent time-warp attacks
    // Basically we're setting up a controlled feedback loop here, based on integrating past block times & calculating a derivative on the rate of change,
    // for a faster response time, while keeping each adjustment step within an upper and lower bounds limit.
    //
    // Here we calculate the Median time, using the most recent 11 blocks, and again for what it was before that. (aka ~ the last hour for Anoncoin and one algo)
    // We take these two values & subtract them, to get the relative recent performance into an absolute term called nNewSetpoint, which we then proceed to finish calculating...
    int64_t nLatestAvgMedian = pindexLast->GetMedianTimePast();
    int64_t nOlderAvgMedian = pindexFirst->GetMedianTimePast();
    int64_t nNewSetpoint = nLatestAvgMedian - nOlderAvgMedian;       // In a perfect world, nNewSetpoint should approach our target timespan
    LogPrintf("  Integrated Setpoint = %lld using TimeMedian=%lld from block %d & TimeMedian=%lld from block %d\n", nNewSetpoint, nLatestAvgMedian, pindexLast->nHeight, nOlderAvgMedian, pindexFirst->nHeight);

    // This next line of code calculates the difference between the real timespan and the target timespan, as a signed +/- value, using fast integer math.
    // Picking the right constant to scale this differential value (we are dividing the amount here by 2 with one algo) is a challenge.  We want the
    // difficulty shield to have a fast response time as mining conditions change, yet not have it endlessly oscillate when mining difficulty would otherwise
    // be running smooth.  To small of a value and the retarget will not response as fast as it could to a sudden change in mining difficulty, if the value
    // is too large, it can easily cause it to always hit the upper bound, then snap to the lower bound limit and vise-versa.
    int64_t nTimespanDiff = (nNewSetpoint - nIntegratedTargetTimespan) / (ALGO_COUNT_BEING_USED + 1);

    // Here now we finally have a new target time that will bring our integrated average block time back to what it should be with the differential added.
    nNewSetpoint = nIntegratedTargetTimespan + nTimespanDiff;
    LogPrintf("  Differential = %lld secs & New Setpoint = %lld before bound limits applied\n", nTimespanDiff, nNewSetpoint );

    // Limit adjustment step...
    // Now we limit that new difficulty adjustment by a predetermined upper and lower maximum amount of possible adjustment for the next block's difficulty
    // Under stressful mining difficulty changes, the values programmed will increase the difficulty twice as fast as it will decay back to a lower difficulty level.
    if (nNewSetpoint < nIncreasingDifficultyLimit)
        nNewSetpoint = nIncreasingDifficultyLimit;
    if (nNewSetpoint > nDecreasingDifficultyLimit)
        nNewSetpoint = nDecreasingDifficultyLimit;

    // Global retarget
    CBigNum bnNew;
    // Start with what the previous block (of same algo) had for difficulty, multiply it by our new calculated setpoint timespan desired & divide by the target goal.
    // that is our new difficulty, unless its less that what Anoncoin can allow as the absolute minimum difficulty on the network aka ProofOfWorkLimit
    // This is done with bignums, upgrade to the new v10 arith_uint256 class is the plan...
    bnNew.SetCompact(pPrevBlockSameAlgo->nBits);
    bnNew *= nNewSetpoint;
    bnNew /= nIntegratedTargetTimespan;

#if ALGO_COUNT_BEING_USED > 1
    // Per-algo retarget
    int nAdjustments = pPrevBlockSameAlgo->nHeight - pindexLast->nHeight + CChainParams::MAX_ALGO_TYPES - 1;
    if (nAdjustments > 0)
    {
        for (int i = 0; i < nAdjustments; i++)
        {
            bnNew /= 100 + nLocalDifficultyAdjustment;
            bnNew *= 100;
        }
    }
    if (nAdjustments < 0)
    {
        for (int i = 0; i < -nAdjustments; i++)
        {
            bnNew *= 100 + nLocalDifficultyAdjustment;
            bnNew /= 100;
        }
    }
#endif

    if( bnNew > bnProofOfWorkLimit ) bnNew = bnProofOfWorkLimit;

    // debug print
// #if defined( LOG_DEBUG_OUTPUT )
    // LogPrintf("  nTargetTimespan = %lld    nNewSetpoint after bounds = %lld\n", nTargetTimespan, nNewSetpoint);
    LogPrintf("  nNewSetpoint used after bounds = %lld\n", nNewSetpoint);
    LogPrintf("  Before: %08x %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString().c_str());
    LogPrintf("  After : %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());
// #endif

	return bnNew.GetCompact();
}

#endif // defined( HARD_FORK )

// The following function are all that is seen from the outside world and used throughout
// the rest of the source code for Anoncoin, everything above should be static and only
// referenced from within this source code file.

/*
 * The workhorse routine, which oversees the blockchain Proof Of Work algorithms
 */
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    // Before we implement merged mining, there was no other ProofOfWorkLimit than the base Scrypt algo
    // We now setup that variable, and pass it as a parameter to the various implementations of difficulty
    // calculation, that have happened over the years...
    const CBigNum &bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    assert(pindexLast);
    if( TestNet() || pindexLast->nHeight > nDifficultySwitchHeight3 ) {
#if defined( HARDFORK_BLOCK )
        // For Testnet first we mine some default blocks.  These are forced to be spaced near or after our target timespan in miner.cpp
        // regardless of the ProofOfWorkLimit initially set. This nHeight value has
        // been choosen, to have enough blocks to start with, so that the new algo can look back in time, and at
        // least have something to work with.
        // Also doing it this way can be helpful for tuning the chainparam value picked as the limit,
        // and/or the miners you will be using to testnet with.  We want things to start out in the right ball park with the first blocks generated....
        if( TestNet() && pindexLast->nHeight <= 22 )
            return bnProofOfWorkLimit.GetCompact();
        if( pindexLast->nHeight > nDifficultySwitchHeight4 || TestNet() ) {             // Otherwise this new algo will happen at the hardfork block value you have set for mainnet.
            return GetNextWorkRequiredAfterKgw(pindexLast, pblock, bnProofOfWorkLimit );// Or if we are running Testnet...
        } else
#endif
            return GetNextWorkRequiredForKgw(pindexLast, pblock, bnProofOfWorkLimit );
    } else
        return OldGetNextWorkRequired(pindexLast, pblock, bnProofOfWorkLimit );
}


/*
 * The primary routine, which determines Anoncoins mined value over time...
 */
int64_t GetBlockValue(int nHeight, int64_t nFees)
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


/*
 * The primary routine, which verifies a claim of Proof Of Work
 */
bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // printf("CheckProofOfWork Hash: %s, nBits: %08x, Target: %08x, bnLimt: %08x \n", hash.ToString().c_str(), nBits, bnTarget.GetCompact(), Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ).GetCompact());
    // ToDo: Which Algo?  After merge mining begins, which coins ProofOfWorkLimit
    // needs to be considered when evaluating someone else's claim about it.
    if (bnTarget <= 0 || bnTarget > Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ))
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches or exceeds the claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

//
// minimum amount of work that could possibly be required nTime after
// minimum work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    // ToDo: Need to print this out, and reload the bootstrap blockchain looking for references that call this in the debug.log file.
    // at least through the last checkpoint, block 200K something..
    return Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ).GetCompact();
}

void UpdateTime(CBlockHeader& block, const CBlockIndex* pindexPrev)
{
    static int64_t nLastReportTime = 0;
    int64_t nMedianTimePast;

    if( pindexPrev->nHeight > CBlockIndex::nMedianTimeSpan ) {                    // This is needed for testnet, were the previous block starts out as the genesis
        nMedianTimePast = pindexPrev->GetMedianTimePast();
        block.nTime = max( nMedianTimePast+1, GetAdjustedTime() );
    } else {
        block.nTime = GetAdjustedTime();
        nMedianTimePast = pindexPrev->GetBlockTime();               // This is only used for the reporting below, if needed
    }

    // Needless reporting of the the blocktime update is reduced to 1/10 of the target timespan (18secs) by this if...
    if( block.nTime - nLastReportTime > 18 ) {
        LogPrintf( (pindexPrev->nHeight <= CBlockIndex::nMedianTimeSpan) ? "New blocktime since last block %lld (blocks 1-11)\n" : "New blocktime since last median %lld\n", block.nTime - nMedianTimePast );
        nLastReportTime = block.nTime;
    }

    // Updating time can change work required on testnet:
    // That maybe true, have found that comment and this code in many other crypto coin sources,
    // and if using an rpc call for testnet mining, to getwork or getblocktemplate, this may yet be required.
    // For Anoncoin on testnet, initially here just using the built-in miner, which
    // calls this code right after UpdateTime() is set, so using that to mine with this no longer makes sense to me.
    // We can better control TestNet() operation there, than in this function....
    // ToDo: yet to be determined if this is needed...
    // if (TestNet())
        // block.nBits = GetNextWorkRequired(pindexPrev, &block);
}

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}


