CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(cielabConverter)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${EIGEN3_INCLUDE_DIR}
	../gtest/
)

ADD_DEFINITIONS(-DCIELAB_GTEST_UNIT_TESTS)

ADD_EXECUTABLE(cielabConverter
	cielab.cpp
	cielab.h
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
)
