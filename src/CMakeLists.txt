#cmake_minimum_required(VERSION 3.0)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/local/share/cmake/Modules/")
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(LIB_HEADERS
    addrman.h
    alert.h
    allocators.h
    base58.h
    coins.h
    block.h
    bloom.h
    chain.h
    chainparams.h
    chainparamsbase.h
    checkpoints.h
    checkqueue.h
    clientversion.h
    coincontrol.h
    compat.h
    compat/sanity.h
    compressor.h
    core_io.h
    transaction.h
    crypter.h
    crypto/common.h
    crypto/hmac_sha256.h
    crypto/hmac_sha512.h
    crypto/rfc6979_hmac_sha256.h
    crypto/ripemd160.h
    crypto/sha1.h
    crypto/sha256.h
    crypto/sha512.h
    db.h
    hash.h
    init.h
    key.h
    keystore.h
    leveldbwrapper.h
    limitedmap.h
    main.h
    merkleblock.h
    miner.h
    mruset.h
    netbase.h
    net.h
    pow.h
    protocol.h
    rpcclient.h
    rpcprotocol.h
    rpcserver.h
    scrypt.h
    script.h
    script_error.h
    sigcache.h
    sign.h
    serialize.h
    streams.h
    sync.h
    threadsafety.h
    timedata.h
    tinyformat.h
    txdb.h
    txmempool.h
    ui_interface.h
    uint256.h
    univalue/univalue.h
    univalue/univalue_escapes.h
    util.h
    version.h
    walletdb.h
    wallet.h
    i2psam.h
    i2pwrapper.h
    )

set(LIB_SOURCES
    addrman.cpp
    amount.cpp
    alert.cpp
    allocators.cpp
    base58.cpp
    bloom.cpp
    chain.cpp
    chainparamsbase.cpp
    chainparams.cpp
    checkpoints.cpp
    clientversion.cpp
    coins.cpp
    compat/glibc_sanity.cpp
    compat/glibcxx_sanity.cpp
    compat/strnlen.cpp
    compressor.cpp
    core_read.cpp
    core_write.cpp
    block.cpp
    transaction.cpp
    crypter.cpp
    crypto/hmac_sha256.cpp
    crypto/hmac_sha512.cpp
    crypto/rfc6979_hmac_sha256.cpp
    crypto/ripemd160.cpp
    crypto/sha1.cpp
    crypto/sha256.cpp
    crypto/sha512.cpp
    db.cpp
    hash.cpp
    init.cpp
    key.cpp
    keystore.cpp
    leveldbwrapper.cpp
    main.cpp
    merkleblock.cpp
    miner.cpp
    netbase.cpp
    net.cpp
    noui.cpp
    pow.cpp
    protocol.cpp
    random.cpp
    rest.cpp
    rpcblockchain.cpp
    rpcclient.cpp
    rpcdump.cpp
    rpcmining.cpp
    rpcmisc.cpp
    rpcnet.cpp
    rpcprotocol.cpp
    rpcrawtransaction.cpp
    rpcserver.cpp
    rpcwallet.cpp
    scrypt.cpp
    scrypt-sse2.cpp
    script.cpp
    script_error.cpp
    sigcache.cpp
    sign.cpp
    sync.cpp
    timedata.cpp
    txdb.cpp
    txmempool.cpp
    uint256.cpp
    univalue/gen.cpp
    univalue/univalue.cpp
    univalue/univalue_read.cpp
    univalue/univalue_write.cpp
    util.cpp
    wallet.cpp
    walletdb.cpp
    i2psam.cpp
    i2pwrapper.cpp
    )


if(GLIBC_BACK_COMPAT)
    #list(APPEND ${LIB_SOURCES} compat/glibc_compat.cpp compat/glibcxx_compat.cpp)
    set(LIB_SOURCES ${LIB_SOURCES} compat/glibc_compat.cpp compat/glibcxx_compat.cpp)
endif()

if(USE_ASM)
    # This should never be reached. It's disabled in ../CMakeLists.txt
    set(can_use_assembler FALSE)
    set(LIB_SOURCES ${LIB_SOURCES} secp256k1/src/field_5x52_asm.asm)
endif()

set(DIR_SOURCES)
foreach(file ${LIB_SOURCES})
    set(DIR_SOURCES ${DIR_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()

set(DIR_HEADERS)
foreach(file ${LIB_HEADERS})
    set(DIR_HEADERS ${DIR_HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/leveldb)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/i2psam)

add_definitions( "-DHAS_BOOST -DHAVE_WORKING_BOOST_SLEEP" )

include_directories(${CMAKE_CURRENT_BINARY_DIR}/config
                    ${BDB_INCLUDE}
                    ${Boost_INCLUDE_DIRS}
                    ${OPENSSL_INCLUDE_DIRS}
                    ${OPENSSL_INCLUDE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/leveldb/helpers
                    ${CMAKE_CURRENT_SOURCE_DIR}/leveldb/helpers/memenv
                    ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1
                    ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/include
                    )

# TODO: autotools splits this into:
#      libbitcoin
#      libbitcoinconsensus
#      libsecp256k1
add_library(anoncoin ${DIR_HEADERS} ${DIR_SOURCES})
target_link_libraries(anoncoin leveldb i2psam
                      ${CMAKE_THREAD_LIBS_INIT}
                      ${BDB_LIBRARY}
                      ${Boost_LIBRARIES}
                      ${OPENSSL_LIBRARIES}
                     )

if(USE_GMP)
    target_link_libraries(anoncoin gmp)
endif()

if(USE_UPNP)
    include_directories(${MINIUPNP_INCLUDE_DIR})
    target_link_libraries(anoncoin ${MINIUPNP_LIBRARY})
endif()

add_executable(anoncoind ${CMAKE_CURRENT_SOURCE_DIR}/anoncoind.cpp)
target_link_libraries(anoncoind anoncoin)

add_executable(anoncoin-cli ${CMAKE_CURRENT_SOURCE_DIR}/anoncoin-cli.cpp)
target_link_libraries(anoncoin-cli anoncoin)

add_executable(anoncoin-tx ${CMAKE_CURRENT_SOURCE_DIR}/anoncoin-tx.cpp)
target_link_libraries(anoncoin-tx anoncoin)

if(BUILD_QT)
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/qt)
endif()

if(ENABLE_TESTS)
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/test)
endif()

install(TARGETS anoncoind DESTINATION bin)


