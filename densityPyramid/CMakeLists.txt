CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(densityPyramid)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

ADD_EXECUTABLE(densityPyramid densityPyramid.cpp mipVolume.h cache.h utility.h)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(densityPyramid ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(densityPyramid ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(densityPyramid ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(densityPyramid ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(densityPyramid ${NIVEN_VOLUME_LIBRARY})