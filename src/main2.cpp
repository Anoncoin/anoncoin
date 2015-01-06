// Protocol 1 & 2

static const int64_t nTargetTimespan = 86184;                       //420 * 205.2; = 86184 // Anoncoin: 420 blocks
static const int64_t nTargetSpacing = 205;                          //3.42 * 60; // Anoncoin: 3.42 minutes
static const int64_t nInterval = nTargetTimespan / nTargetSpacing;

static const int nDifficultySwitchHeight = 15420;
static const int nDifficultySwitchHeight2 = 77777;

// Protocol 3
static const int nDifficultyProtocol3 = 87777;


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


//
// minimum amount of work that could possibly be required nTime after
// minimum work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    // ToDo: Which Algo?
    const CBigNum &bnLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    // Testnet has min-difficulty blocks
    // after nTargetSpacing*2 time between blocks:
    if (TestNet() && nTime > nTargetSpacing*2)
        return bnLimit.GetCompact();

    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    while (nTime > 0 && bnResult < bnLimit)
    {
        // Maximum 141% adjustment...
        bnResult = (bnResult * 99) / 70;
        // ... in best-case exactly 4-times-normal target time
        nTime -= nTargetTimespan*4;
    }
    if (bnResult > bnLimit)
        bnResult = bnLimit;
    return bnResult.GetCompact();
}


/*
 *  Current difficulty formula, Anoncoin - kimoto gravity well
 */
unsigned int static KimotoGravityWell(const CBlockIndex* pindexLast,
                                      const CBlockHeader *pblock,
                                      uint64_t TargetBlocksSpacingSeconds,
                                      uint64_t PastBlocksMin,
                                      uint64_t PastBlocksMax) {

    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;

    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    CBigNum PastDifficultyAverage;
    CBigNum PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;
    CBigNum bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin) { return bnProofOfWorkLimit.GetCompact(); }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
        PastBlocksMass++;

        if (i == 1) { PastDifficultyAverage.SetCompact(BlockReading->nBits); }
        else { PastDifficultyAverage = ((CBigNum().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev; }
        PastDifficultyAveragePrev = PastDifficultyAverage;

        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if (PastRateActualSeconds < 0) { PastRateActualSeconds = 0; }
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
        }
        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(144)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) { assert(BlockReading); break; }
        }
        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }

    CBigNum bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }
    if (bnNew > bnProofOfWorkLimit) { bnNew = bnProofOfWorkLimit; }

    /// debug print
    // LogPrintf("Difficulty Retarget - Kimoto Gravity Well\n");
    // LogPrintf("PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
    // LogPrintf("Before: %08x %s\n", BlockLastSolved->nBits, CBigNum().SetCompact(BlockLastSolved->nBits).getuint256().ToString().c_str());
    // LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());

    return bnNew.GetCompact();
}


unsigned int static NeoGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    static const int64_t BlocksTargetSpacing = 3 * 60; // 3 minutes
    unsigned int TimeDaySeconds = 60 * 60 * 24;
    int64_t PastSecondsMin = TimeDaySeconds * 0.25;
    int64_t PastSecondsMax = TimeDaySeconds * 7;
    uint64_t PastBlocksMin = PastSecondsMin / BlocksTargetSpacing;
    uint64_t PastBlocksMax = PastSecondsMax / BlocksTargetSpacing;

    return KimotoGravityWell(pindexLast, pblock, BlocksTargetSpacing, PastBlocksMin, PastBlocksMax);
}


unsigned int static OldGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    // ToDo: Which Algo?
    CBigNum bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );
    unsigned int nProofOfWorkLimit = bnProofOfWorkLimit.GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Anoncoin difficulty adjustment protocol switch (Thanks to FeatherCoin for this idea)

    static const int newTargetTimespan = 2050;
    int nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = (nHeight >= nDifficultySwitchHeight || TestNet());
    bool fNewDifficultyProtocol2 = (nHeight >= nDifficultySwitchHeight2 || TestNet());
    if (fNewDifficultyProtocol2) {
        // Jumping back to sqrt(2) as the factor of adjustment.
        fNewDifficultyProtocol = false;
    }

    int64_t nTargetTimespanCurrent = fNewDifficultyProtocol ? (nTargetTimespan*4) : nTargetTimespan;
    int64_t nInterval = nTargetTimespanCurrent / nTargetSpacing;

    if (fNewDifficultyProtocol2) nTargetTimespanCurrent = newTargetTimespan;
    if (TestNet() && nHeight < (newTargetTimespan/205)+1) return pindexLast->nBits;

    // Only change once per interval, or at protocol switch height
    if ((nHeight % nInterval != 0 && !fNewDifficultyProtocol2) &&
        (nHeight != nDifficultySwitchHeight) && !TestNet())
    {
        return pindexLast->nBits;
    }

    // Anoncoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval-1;
    if ((pindexLast->nHeight+1) != nInterval)
        blockstogoback = nInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    blockstogoback = fNewDifficultyProtocol2 ? (newTargetTimespan/205) : blockstogoback;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    int64_t nActualTimespanMax = fNewDifficultyProtocol ? (nTargetTimespanCurrent*4) : ((nTargetTimespanCurrent*99)/70);
    int64_t nActualTimespanMin = fNewDifficultyProtocol ? (nTargetTimespanCurrent/4) : ((nTargetTimespanCurrent*70)/99);
    LogPrintf("  nActualTimespan = %lld  before bounds\n", nActualTimespan);
    if (pindexLast->nHeight+1 >= nDifficultySwitchHeight2) {
        if (nActualTimespan < nActualTimespanMin)
            nActualTimespan = nActualTimespanMin;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    } else if (pindexLast->nHeight+1 > nDifficultySwitchHeight) {
        if (nActualTimespan < nActualTimespanMin/4)
            nActualTimespan = nActualTimespanMin/4;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    } else {
        if (nActualTimespan < nActualTimespanMin)
            nActualTimespan = nActualTimespanMin;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    }

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    if (fNewDifficultyProtocol2) {
        bnNew /= nTargetTimespanCurrent;
    } else {
        bnNew /= nTargetTimespan;
    }

    if (bnNew > bnProofOfWorkLimit) bnNew = bnProofOfWorkLimit;

    /// debug print
    LogPrintf("GetNextWorkRequired RETARGET\n");
    LogPrintf("nTargetTimespan = %lld    nActualTimespan = %lld\n", nTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString());

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    assert(pindexLast);
    if (pindexLast->nHeight > nDifficultyProtocol3 || TestNet()) {
        return NeoGetNextWorkRequired(pindexLast, pblock);
    } else {
        return OldGetNextWorkRequired(pindexLast, pblock);
    }
}


bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    // ToDo: Which Algo?
    if (bnTarget <= 0 || bnTarget > Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ))
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

