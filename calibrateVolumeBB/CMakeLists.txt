CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(calibrateVolumeBB)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(calibrateVolumeBB calibrateVolumeBB.cpp volumeCalibration.h)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(calibrateVolumeBB ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(calibrateVolumeBB ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(calibrateVolumeBB ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(calibrateVolumeBB ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(calibrateVolumeBB ${NIVEN_VOLUME_LIBRARY})