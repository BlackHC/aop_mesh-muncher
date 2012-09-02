CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(wmlTest)

INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	../gtest
	)

ADD_EXECUTABLE(wmlTest
	wml.h
	leanTextProcessing.h
	wmlTest.cpp
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

TARGET_LINK_LIBRARIES(wmlTest ${Boost_LIBRARIES})