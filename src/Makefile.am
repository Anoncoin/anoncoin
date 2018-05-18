# Anoncoin edits as required to build 9.5 code,
# Still some things we're not building, but getting closer, others no one else has...
#
# DIST_SUBDIRS = secp256k1
DIST_SUBDIRS=
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS)


if EMBEDDED_LEVELDB
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/include
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/helpers/memenv
LIBLEVELDB += $(builddir)/leveldb/libleveldb.a
LIBMEMENV  += $(builddir)/leveldb/libmemenv.a

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
$(LIBLEVELDB): $(LIBMEMENV)

$(LIBLEVELDB) $(LIBMEMENV):
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
          OPT="$(CXXFLAGS) $(CPPFLAGS)"
endif

BOOST_CPPFLAGS += -DBOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT

ANONCOIN_CONFIG_INCLUDES=-I$(builddir)/config
ANONCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

#ANONCOIN_INCLUDES += -I$(srcdir)/secp256k1/include

LIBANONCOIN_SERVER=libanoncoin_server.a
LIBANONCOIN_WALLET=libanoncoin_wallet.a
LIBANONCOIN_COMMON=libanoncoin_common.a
LIBANONCOIN_CLI=libanoncoin_cli.a
LIBANONCOIN_UTIL=libanoncoin_util.a
LIBANONCOIN_CRYPTO=crypto/libanoncoin_crypto.a
LIBANONCOIN_UNIVALUE=univalue/libanoncoin_univalue.a
LIBANONCOIN_SCRYPT=libanoncoin_scrypt.a
LIBANONCOIN_I2PNET=libanoncoin_i2pnet.a
LIBANONCOINQTCLASS=qt/libanoncoinqtc.a
LIBANONCOINQTHEMES=qthemes/libanoncoinqtt.a
#LIBSECP256K1=secp256k1/libsecp256k1.la
LIBSECP256K1=

#$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
#	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libanoncoin_crypto.a \
  libanoncoin_util.a \
  libanoncoin_common.a \
  univalue/libanoncoin_univalue.a \
  libanoncoin_server.a \
  libanoncoin_cli.a \
  libanoncoin_scrypt.a
if ENABLE_WALLET
ANONCOIN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libanoncoin_wallet.a
endif
if ENABLE_I2PSAM
EXTRA_LIBRARIES += libanoncoin_i2pnet.a
endif

#if BUILD_ANONCOIN_LIBS
#lib_LTLIBRARIES = libanoncoinconsensus.la
#LIBANONCOIN_CONSENSUS=libanoncoinconsensus.la
#else
LIBANONCOIN_CONSENSUS=
#endif

bin_PROGRAMS =
TESTS =

if BUILD_ANONCOIND
  bin_PROGRAMS += anoncoind
endif

if BUILD_ANONCOIN_UTILS
  bin_PROGRAMS += anoncoin-cli anoncoin-tx
endif

.PHONY: FORCE
# anoncoin core #
ANONCOIN_CORE_H = \
  addrman.h \
  alert.h \
  allocators.h \
  amount.h \
  base58.h \
  block.h \
  bloom.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compressor.h \
  core_io.h \
  crypter.h \
  db.h \
  hash.h \
  i2psam.h \
  i2pwrapper.h \
  init.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  netbase.h \
  net.h \
  noui.h \
  pow.h \
  protocol.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  script.h \
  script_error.h \
  scrypt.h \
  serialize.h \
  sigcache.h \
  sign.h \
  streams.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  transaction.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  version.h \
  wallet.h \
  walletdb.h \
  compat/sanity.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libanoncoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between anoncoind and anoncoin-qt
libanoncoin_server_a_CPPFLAGS = $(ANONCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libanoncoin_server_a_SOURCES = \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  txdb.cpp \
  txmempool.cpp \
  $(JSON_H) \
  $(ANONCOIN_CORE_H)

# wallet: shared between anoncoind and anoncoin-qt, but only linked
# when wallet enabled
libanoncoin_wallet_a_CPPFLAGS = $(ANONCOIN_INCLUDES)
libanoncoin_wallet_a_SOURCES = \
  db.cpp \
  crypter.cpp \
  rpcdump.cpp \
  rpcwallet.cpp \
  wallet.cpp \
  walletdb.cpp \
  $(ANONCOIN_CORE_H)

# crypto primitives library
crypto_libanoncoin_crypto_a_CPPFLAGS = $(ANONCOIN_CONFIG_INCLUDES)
crypto_libanoncoin_crypto_a_SOURCES = \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/hmac_sha256.cpp \
  crypto/rfc6979_hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/rfc6979_hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/sha1.h \
  crypto/ripemd160.h \
  Gost3411.cpp \
  Gost3411.h

# univalue JSON library
univalue_libanoncoin_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp \
  univalue/univalue_escapes.h \
  univalue/univalue.h

# common: shared between anoncoind, and anoncoin-qt and non-server tools
libanoncoin_common_a_CPPFLAGS = $(ANONCOIN_INCLUDES)
libanoncoin_common_a_SOURCES = \
  addrman.cpp \
  allocators.cpp \
  amount.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  block.cpp \
  timedata.cpp \
  transaction.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  protocol.cpp \
  script.cpp \
  script_error.cpp \
  sigcache.cpp \
  sign.cpp \
  $(ANONCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libanoncoin_util_a_CPPFLAGS = $(ANONCOIN_INCLUDES)
libanoncoin_util_a_SOURCES = \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  random.cpp \
  rpcprotocol.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  $(ANONCOIN_CORE_H)

# i2pnet: shared between anoncoind and anoncoin-qt, but only linked
# when the network type is enabled
libanoncoin_i2pnet_a_CPPFLAGS = $(ANONCOIN_INCLUDES)
libanoncoin_i2pnet_a_SOURCES = \
  i2pwrapper.cpp \
  i2psam.cpp \
  $(ANONCOIN_CORE_H)

# scrypt: shared between anoncoind and anoncoin-qt, requires special compiler flags
libanoncoin_scrypt_a_CPPFLAGS = $(ANONCOIN_INCLUDES) -msse2 -mstackrealign
libanoncoin_scrypt_a_SOURCES = \
  scrypt.cpp \
  scrypt-sse2.cpp \
  $(ANONCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libanoncoin_util_a_SOURCES += compat/glibc_compat.cpp
libanoncoin_util_a_SOURCES += compat/glibcxx_compat.cpp
endif

# cli: shared between anoncoin-cli and anoncoin-qt
libanoncoin_cli_a_CPPFLAGS = $(ANONCOIN_INCLUDES)
libanoncoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(ANONCOIN_CORE_H)

nodist_libanoncoin_util_a_SOURCES = $(srcdir)/obj/build.h

# anoncoind binary #
anoncoind_LDADD = \
  $(LIBANONCOIN_SERVER) \
  $(LIBANONCOIN_COMMON) \
  $(LIBANONCOIN_UNIVALUE) \
  $(LIBANONCOIN_UTIL) \
  $(LIBANONCOIN_CRYPTO) \
  $(LIBANONCOIN_SCRYPT) \
  $(LIBLEVELDB) \
  $(LIBMEMENV)

if ENABLE_WALLET
anoncoind_LDADD += libanoncoin_wallet.a
endif
if ENABLE_I2PSAM
anoncoind_LDADD += libanoncoin_i2pnet.a
endif

anoncoind_SOURCES = anoncoind.cpp
if TARGET_WINDOWS
anoncoind_SOURCES += anoncoind-res.rc
endif

anoncoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS)
anoncoind_CPPFLAGS = $(ANONCOIN_INCLUDES)
anoncoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

# anoncoin-cli binary #
anoncoin_cli_LDADD = \
  $(LIBANONCOIN_CLI) \
  $(LIBANONCOIN_UTIL) \
  $(BOOST_LIBS) \
  $(SSL_LIBS) \
  $(CRYPTO_LIBS)

anoncoin_cli_SOURCES = anoncoin-cli.cpp
if TARGET_WINDOWS
anoncoin_cli_SOURCES += anoncoin-cli-res.rc
endif

anoncoin_cli_CPPFLAGS = $(ANONCOIN_INCLUDES)
anoncoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

#anoncoin-tx binary #
anoncoin_tx_LDADD = \
  $(LIBANONCOIN_UNIVALUE) \
  $(LIBANONCOIN_COMMON) \
  $(LIBANONCOIN_UTIL) \
  $(LIBANONCOIN_CRYPTO) \
  $(LIBANONCOIN_SCRYPT) \
  $(BOOST_LIBS) \
  $(CRYPTO_LIBS)

if ENABLE_I2PSAM
anoncoin_tx_LDADD += libanoncoin_i2pnet.a
endif

anoncoin_tx_SOURCES = anoncoin-tx.cpp

anoncoin_tx_CPPFLAGS = $(ANONCOIN_INCLUDES)
anoncoin_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

#if BUILD_ANONCOIN_LIBS
#include_HEADERS = script/anoncoinconsensus.h
#libanoncoinconsensus_la_SOURCES = \
#  transaction.cpp \
#  crypto/sha1.cpp \
#  crypto/sha2.cpp \
#  crypto/ripemd160.cpp \
#  eccryptoverify.cpp \
#  ecwrapper.cpp \
#  hash.cpp \
#  pubkey.cpp \
#  script/script.cpp \
#  script/interpreter.cpp \
#  script/anoncoinconsensus.cpp \
#  uint256.cpp \
#  utilstrencodings.cpp

#if GLIBC_BACK_COMPAT
#  libanoncoinconsensus_la_SOURCES += compat/glibc_compat.cpp
#  libanoncoinconsensus_la_SOURCES += compat/glibcxx_compat.cpp
#endif

#libanoncoinconsensus_la_LDFLAGS = -no-undefined $(RELDFLAGS)
#libanoncoinconsensus_la_LIBADD = $(CRYPTO_LIBS)
#libanoncoinconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) -I$(builddir)/obj -DBUILD_ANONCOIN_INTERNAL
#if USE_LIBSECP256K1
#libanoncoinconsensus_la_LIBADD += secp256k1/libsecp256k1.la
#endif
#endif

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h $(EXTRA_LIBRARIES)

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
#	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
include Makefile.qthemes.include
endif

if ENABLE_QT_TESTS
#include Makefile.qttest.include
endif
