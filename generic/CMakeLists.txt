CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Test_generic)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	../gtest
	)

ADD_EXECUTABLE(Test_generic

	sort_permute_iter.h
	sort_permute_iter_test.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)
