CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Test_grid)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${EIGEN3_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	../gtest
	../framework
	)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(Test_volume
	../framework/mathUtility.h

	grid.h
	gridTests.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

