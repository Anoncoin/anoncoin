# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

BITCOIN_INCLUDES=-I$(builddir) $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

BITCOIN_INCLUDES += -I$(srcdir)/secp256k1/include
BITCOIN_INCLUDES += $(UNIVALUE_CFLAGS)

LIBBITCOIN_SERVER=libanoncoin_server.a
LIBBITCOIN_COMMON=libanoncoin_common.a
LIBBITCOIN_CONSENSUS=libanoncoin_consensus.a
LIBBITCOIN_CLI=libanoncoin_cli.a
LIBBITCOIN_I2PSAM=libanoncoin_i2psam.a
LIBBITCOIN_I2PD=libanoncoin_i2pnet.a
LIBBITCOIN_UTIL=libanoncoin_util.a
LIBBITCOIN_CRYPTO=crypto/libanoncoin_crypto.a
LIBBITCOINQT=qt/libanoncoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBBITCOIN_ZMQ=libanoncoin_zmq.a
endif
if BUILD_BITCOIN_LIBS
LIBBITCOINCONSENSUS=libanoncoinconsensus.la
endif
if ENABLE_WALLET
LIBBITCOIN_WALLET=libanoncoin_wallet.a
endif
if ENABLE_I2PSAM
LIBBITCOIN_I2PSAM=libanoncoin_i2psam.a
endif
if ENABLE_I2PD
LIBBITCOIN_I2PD=libanoncoin_i2pnet.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_ZMQ)

lib_LTLIBRARIES = $(LIBBITCOINCONSENSUS)

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_BITCOIND
  bin_PROGRAMS += anoncoind
endif

if BUILD_BITCOIN_UTILS
  bin_PROGRAMS += anoncoin-cli anoncoin-tx
endif

.PHONY: FORCE check-symbols check-security
# bitcoin core #
BITCOIN_CORE_H = \
  addrdb.h \
  addrman.h \
  base58.h \
  bech32.h \
  bloom.h \
  blockencodings.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  core_io.h \
  core_memusage.h \
  cuckoocache.h \
  fs.h \
  httprpc.h \
  httpserver.h \
  indirectmap.h \
  init.h \
  i2p.h \
  key.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  policy/feerate.h \
  policy/fees.h \
  policy/policy.h \
  policy/rbf.h \
  pow.h \
  protocol.h \
  random.h \
  reverse_iterator.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/client.h \
  rpc/mining.h \
  rpc/protocol.h \
  rpc/safemode.h \
  rpc/server.h \
  rpc/register.h \
  rpc/util.h \
  scheduler.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/ismine.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utiltime.h \
  validation.h \
  validationinterface.h \
  versionbits.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/fees.h \
  wallet/init.h \
  wallet/rpcwallet.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  wallet/walletutil.h \
  warnings.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libanoncoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between bitcoind and bitcoin-qt
libanoncoin_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libanoncoin_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_server_a_SOURCES = \
  addrdb.cpp \
  addrman.cpp \
  bloom.cpp \
  blockencodings.cpp \
  chain.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  httprpc.cpp \
  httpserver.cpp \
  init.cpp \
  dbwrapper.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  pow.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/safemode.cpp \
  rpc/server.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  timedata.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  ui_interface.cpp \
  validation.cpp \
  validationinterface.cpp \
  versionbits.cpp \
  $(BITCOIN_CORE_H)

if ENABLE_ZMQ
libanoncoin_zmq_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(ZMQ_CFLAGS)
libanoncoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif


# wallet: shared between bitcoind and bitcoin-qt, but only linked
# when wallet enabled
libanoncoin_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_wallet_a_SOURCES = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/walletdb.cpp \
  wallet/walletutil.cpp \
  $(BITCOIN_CORE_H)

# crypto primitives library
crypto_libanoncoin_crypto_a_CPPFLAGS = $(AM_CPPFLAGS) $(SSL_CFLAGS)
crypto_libanoncoin_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libanoncoin_crypto_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/scrypt.cpp \
  crypto/scrypt-sse2.cpp \
  crypto/scrypt.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h \
  Crypto.h \
  Gost3410.cpp \
  Gost3410.h \
  Gost3411.cpp \
  Gost3411.h

if USE_ASM
crypto_libanoncoin_crypto_a_SOURCES += crypto/sha256_sse4.cpp
endif

if USE_I2PSAM
libanoncoin_i2psam_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_i2psam_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_i2psam_a_SOURCES += i2p.cpp i2p.h i2psam/i2psam.h i2psam/i2psam.cpp
endif

if USE_I2PD
libanoncoin_i2pd_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_i2pd_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_i2pd_a_SOURCES += i2p.cpp i2p.h
endif

# consensus: shared between all executables that validate any consensus rules.
libanoncoin_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_consensus_a_SOURCES = \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  pubkey.cpp \
  pubkey.h \
  script/bitcoinconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  serialize.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h

# common: shared between bitcoind, and bitcoin-qt and non-server tools
libanoncoin_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_common_a_SOURCES = \
  base58.cpp \
  bech32.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  key.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  warnings.cpp \
  $(BITCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libanoncoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  fs.cpp \
  random.cpp \
  rpc/protocol.cpp \
  rpc/util.cpp \
  support/cleanse.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libanoncoin_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: shared between bitcoin-cli and bitcoin-qt
libanoncoin_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libanoncoin_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libanoncoin_cli_a_SOURCES = \
  rpc/client.cpp \
  $(BITCOIN_CORE_H)

nodist_libanoncoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# bitcoind binary #
anoncoind_SOURCES = anoncoind.cpp
anoncoind_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
anoncoind_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
anoncoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
anoncoind_SOURCES += anoncoind-res.rc
endif

anoncoind_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_ZMQ) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

anoncoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS)

# bitcoin-cli binary #
litecoin_cli_SOURCES = anoncoin-cli.cpp
litecoin_cli_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(EVENT_CFLAGS)
litecoin_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
litecoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
litecoin_cli_SOURCES += anoncoin-cli-res.rc
endif

anoncoin_cli_LDADD = \
  $(LIBBITCOIN_CLI) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO)

anoncoin_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# bitcoin-tx binary #
litecoin_tx_SOURCES = anoncoin-tx.cpp
litecoin_tx_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
litecoin_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
litecoin_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
litecoin_tx_SOURCES += anoncoin-tx-res.rc
endif

anoncoin_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1)

anoncoin_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

# bitcoinconsensus library #
if BUILD_BITCOIN_LIBS
include_HEADERS = script/bitcoinconsensus.h
libanoncoinconsensus_la_SOURCES = $(crypto_libanoncoin_crypto_a_SOURCES) $(libanoncoin_consensus_a_SOURCES)

if GLIBC_BACK_COMPAT
  libanoncoinconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libanoncoinconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libanoncoinconsensus_la_LIBADD = $(LIBSECP256K1) $(CRYPTO_LIBS)
libanoncoinconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_BITCOIN_INTERNAL $(SSL_CFLAGS)
libanoncoinconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno
CLEANFILES += obj/build.h

EXTRA_DIST = $(CTAES_DIST)


config/anoncoin-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/anoncoin-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/anoncoin-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/anoncoin-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
