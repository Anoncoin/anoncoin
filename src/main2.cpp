// ToDo: Remove this file as an include in main.cpp.
//       Rename it to something better & write a header file for it.
//       Then develop the associated build script updates so it compiles & links in with everything else...
//

// #define LOG_DEBUG_OUTPUT

// This value defines the Anoncoin block rate production, difficulty calculations and Proof Of Work functions should use this as the goal...
// nTargetSpacing = 180;      and the definition has been moved to main.h for global visibility Anoncoin is 3 minute spacing

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


#if defined( HARDFORK_BLOCK )

#define ALGO_COUNT_BEING_USED 1
#define MULTIALGO_AVERAGING_INTERVAL (CBlockIndex::nMedianTimeSpan - 1)         // in blocks (10) This value is one less nMedianTimeSpan...
#define MULTIALGO_TARGET_SPACING  ALGO_COUNT_BEING_USED * (nTargetSpacing/2)    // in seconds, Anoncoin with 1 algo = target spacing is 90 seconds
                                                                                // nTargetSpacing is now defined in main.h, and has been 180 seconds (3mins) since KGW era

static const int64_t nIntegratedTargetTimespan = MULTIALGO_AVERAGING_INTERVAL * MULTIALGO_TARGET_SPACING;   // in Seconds 10x2x90 = 1800 or 900 for 1 algo
static const int64_t nIncreasingDifficultyLimit = ( nIntegratedTargetTimespan * (1000 - 120) ) / 1000;      // in Seconds the adjustment limit per retarget, set to 12%
static const int64_t nDecreasingDifficultyLimit = ( nIntegratedTargetTimespan * (1000 + 160) ) / 1000;      // in Seconds the adjustment limit per retarget, set to 16%

// New Block version type #3 definitions we'll be using
enum BlockMinedWith {
    BLOCK_VERSION_DEFAULT        = 3,           // Up the default block version by one, to version #3, and default to algo SCRYPT
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
    while( pindex && (mwa == CChainParams::ALGO_SCRYPT || pindex->nHeight > HARDFORK_BLOCK) )
    {
        if( GetBlockAlgo( pindex ) == mwa )
            return pindex;
        pindex = pindex->pprev;
    }
    return NULL;
}

/*
 *  Difficulty formula, Anoncoin - Early in 2015, due to continuous attack & weakness exploits known about the KGW difficulty algo, this was implemented as a solution
 */
unsigned int static GetNextWorkRequiredAfterKgw(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit )
{
    CChainParams::MinedWithAlgo algo = CChainParams::ALGO_SCRYPT;       // Temporary variable for now with just one algo

    const CBlockIndex* pindexFirst = pindexLast;                        // find first block in averaging interval
    for (int i = 0; pindexFirst && i < ALGO_COUNT_BEING_USED * MULTIALGO_AVERAGING_INTERVAL; i++)
        pindexFirst = pindexFirst->pprev;

    const CBlockIndex* pPrevBlockSameAlgo = GetLastBlockIndexForAlgo(pindexLast, algo);
    if( pindexFirst == NULL || pPrevBlockSameAlgo == NULL )             // Not much more we can do then...
        return bnProofOfWorkLimit.GetCompact();                         // not enough blocks available

    LogPrintf("AncSheild-P1v0.1 RETARGET: Integrating constants in seconds: TimeSpan=%lld, Difficulty UpLimit=%lld, DownLimit=%lld\n", nIntegratedTargetTimespan, nIncreasingDifficultyLimit, nDecreasingDifficultyLimit );
    // Use medians to prevent time-warp attacks
    int64_t nLatestAvgMedian = pindexLast->GetMedianTimePast();
    int64_t nOlderAvgMedian = pindexFirst->GetMedianTimePast();
    int64_t nNewSetpoint = nLatestAvgMedian - nOlderAvgMedian;
    LogPrintf("  Integrated Setpoint = %lld using TimeMedian=%lld from block %d & TimeMedian=%lld from block %d\n", nNewSetpoint, nLatestAvgMedian, pindexLast->nHeight, nOlderAvgMedian, pindexFirst->nHeight);
    int64_t nTimespanDiff = (nNewSetpoint - nIntegratedTargetTimespan) / (ALGO_COUNT_BEING_USED + 1);
    // Here now we finally have a new target time that will bring our integrated average block time back to what it should be with the differential added.
    nNewSetpoint = nIntegratedTargetTimespan + nTimespanDiff;
    LogPrintf("  Differential = %lld secs & New Setpoint = %lld before bound limits applied\n", nTimespanDiff, nNewSetpoint );

    // Limit adjustment step...
    if (nNewSetpoint < nIncreasingDifficultyLimit)
        nNewSetpoint = nIncreasingDifficultyLimit;
    if (nNewSetpoint > nDecreasingDifficultyLimit)
        nNewSetpoint = nDecreasingDifficultyLimit;

    // Global retarget
    CBigNum bnNew;
    bnNew.SetCompact(pPrevBlockSameAlgo->nBits);
    bnNew *= nNewSetpoint;
    bnNew /= nIntegratedTargetTimespan;

    if( bnNew > bnProofOfWorkLimit ) bnNew = bnProofOfWorkLimit;
    LogPrintf("  nNewSetpoint used after bounds = %lld\n", nNewSetpoint);
    LogPrintf("  Before: %08x %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString().c_str());
    LogPrintf("  After : %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());

	return bnNew.GetCompact();
}

#endif // defined( HARD_FORK )

// The following function are all that is seen from the outside world and used throughout
// the rest of the source code for Anoncoin, everything above should be static and only
// referenced from within this file.

/*
 * The workhorse routine, which oversees the blockchain Proof Of Work algorithms
 */
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    const CBigNum &bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    assert(pindexLast);
    if( TestNet() || pindexLast->nHeight > nDifficultySwitchHeight3 ) {
#if defined( HARDFORK_BLOCK )
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


