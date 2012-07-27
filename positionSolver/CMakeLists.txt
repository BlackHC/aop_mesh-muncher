CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(positionSolver)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${ANTTWEAKBAR_INCLUDE_DIR} ${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ../gtest/)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(positionSolver
	positionSolver.cpp 
	../gtest/gtest-all.cc
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(positionSolver ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(positionSolver ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(positionSolver ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(positionSolver ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(positionSolver ${NIVEN_VOLUME_LIBRARY})
TARGET_LINK_LIBRARIES(positionSolver ${ANTTWEAKBAR_LIBRARY})