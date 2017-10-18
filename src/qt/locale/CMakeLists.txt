
set(CONTRIB_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
string(REGEX REPLACE "/contrib/cmake" "" CMAKE_CURRENT_SOURCE_DIR
       ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB BITCOIN_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.ts)

# Generate intermediate Qt-specific files from .ui and .h
qt4_add_translation(BITCOIN_QM_FILES ${BITCOIN_TS_FILES})
add_custom_target(bitcoin_translations DEPENDS ${BITCOIN_QM_FILES})
#set(BITCOIN_QT_TRANSLATIONS ${QT_TRANSLATIONS})


