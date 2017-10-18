
set(CONTRIB_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
string(REGEX REPLACE "/contrib/cmake" "" CMAKE_CURRENT_SOURCE_DIR
       ${CMAKE_CURRENT_SOURCE_DIR})

set(TEST_SOURCES
    accounting_tests.cpp
    #alert_tests.cpp
    allocator_tests.cpp
    base32_tests.cpp
    #base58_tests.cpp
    base64_tests.cpp
    bignum_tests.cpp
    bip32_tests.cpp
    bloom_tests.cpp
    #canonical_tests.cpp
    coins_tests.cpp
    checkblock_tests.cpp
    Checkpoints_tests.cpp
    compress_tests.cpp
    crypto_tests.cpp
    DoS_tests.cpp
    getarg_tests.cpp
    hash_tests.cpp
    hmac_tests.cpp
    key_tests.cpp
    miner_tests.cpp
    mruset_tests.cpp
    multisig_tests.cpp
    netbase_tests.cpp
    pmt_tests.cpp
    rpc_tests.cpp
    rpc_wallet_tests.cpp
    script_P2SH_tests.cpp
    #script_tests.cpp
    serialize_tests.cpp
    sighash_tests.cpp
    skiplist_tests.cpp
    sigopcount_tests.cpp
    #transaction_tests.cpp
    uint256_tests.cpp
    util_tests.cpp
    wallet_tests.cpp
)

set(DIR_SOURCES)
foreach(file ${TEST_SOURCES})
    set(DIR_SOURCES ${DIR_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()

set(JSON_FILES
    data/base58_encode_decode.json
    data/base58_keys_invalid.json
    data/base58_keys_valid.json
    data/script_invalid.json
    data/script_valid.json
    data/sig_canonical.json
    data/sig_noncanonical.json
    data/tx_invalid.json
    data/tx_valid.json
)
    
foreach(JSON_FILE ${JSON_FILES})
    string(REGEX REPLACE ".json" ".json.h" JSON_HEADER ${JSON_FILE})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${JSON_FILE}
                   ${CMAKE_CURRENT_BINARY_DIR}/${JSON_HEADER})
endforeach()

set(RAW_FILES data/alertTests.raw)

foreach(RAW_FILE ${RAW_FILES})
    string(REGEX REPLACE ".raw" ".raw.h" RAW_HEADER ${RAW_FILE})
    # configure_file doesn't like parsing binary data, so copy it:
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${RAW_FILE} DESTINATION
              ${CMAKE_CURRENT_BINARY_DIR}/data)
    file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${RAW_FILE}
                ${CMAKE_CURRENT_BINARY_DIR}/${RAW_HEADER})
endforeach()

include_directories(${CMAKE_SOURCE_DIR}/src
                    ${CMAKE_SOURCE_DIR}/src/leveldb
                    ${CMAKE_SOURCE_DIR}/src/leveldb/include
                    ${CMAKE_SOURCE_DIR}/src/leveldb/helpers
                    ${CMAKE_CURRENT_BINARY_DIR}
		    )

add_definitions("-DHAS_BOOST -DHAVE_WORKING_BOOST_SLEEP")
add_definitions(-DBOOST_TEST_DYN_LINK)

include_directories(${BDB_INCLUDE}
                    ${Boost_INCLUDE_DIRS}
                    ${OPENSSL_INCLUDE_DIRS}
                    )

add_executable(test_bitcoin ${CMAKE_CURRENT_SOURCE_DIR}/test_bitcoin.cpp
               ${DIR_SOURCES}
               ${JSON_TEST_FILES}
               ${RAW_TEST_FILES}
	       )

target_link_libraries(test_bitcoin bitcoin
                      ${CMAKE_THREAD_LIBS_INIT}
	      	      ${BDB_LIBRARY}
	              ${Boost_LIBRARIES}
	      	      ${OPENSSL_LIBRARIES}
	      	      )

enable_testing()
add_test(TestBitcoin test_bitcoin)


