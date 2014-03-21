Zerocoin
=====================

TODO:

* Update zc_ufo* to use the four ~3800 bit UFOs

* init.cpp#AppInit2: start Zerocoin thread pool (and document in coding.md)


None of the following scripts are actually executed; instead, the ZC opcodes
are recognized and so that input or output is ignored by the regular Anoncoin
transaction processing (this is why they are prefix rather than postfix). This
transaction is marked for Zerocoin processing.

New input type:

* ZC spend: can have a maximum of one in a transaction, to reduce
  implementation complexity.
  Non-rate-limited script format: ZCSPEND <version: VarInt> <isRateLimited: 0> <serialNum: VarInt> <spendRootHash: VarInt>
  Rate-limited script format: ZCSPEND <version: VarInt> <isRateLimited: 1> <serialNum: VarInt> <spendRootHashTxHash: VarInt> <firstHalfTxOutIdx: VarInt> <snSoKSigHash: VarInt>

New output types:

* ZC checkpoint: always has an amount of zero.
  Script format: RETURN ZCCHECKPT <version: VarInt> <denom: VarInt> <accum0value: VarInt> .. <accumNvalue: VarInt>.
  These can only occur in the coinbase transaction. A checkpoint output can
  occur at most once for any given denomination, and is required if any ZC mints
  using that denomination occur in the block.

