set(HEADERS
    i2psam.h
  )

set(SOURCES
    i2psam.cpp
  )



set(DIR_SOURCES)
foreach(file ${SOURCES})
    set(DIR_SOURCES ${DIR_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()

set(DIR_HEADERS)
foreach(file ${HEADERS})
    set(DIR_HEADERS ${DIR_HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(i2psam STATIC ${DIR_HEADERS} ${DIR_SOURCES})
