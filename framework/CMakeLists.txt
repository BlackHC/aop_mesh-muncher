CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Test_framework)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	../gtest
	)

ADD_EXECUTABLE(Test_framework
	mathUtility.h

	progressTracker.h
	progressTracker.cpp

	test_rayIntersections.cpp
	test_progressTracker.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

TARGET_LINK_LIBRARIES(Test_framework ${SFML_LIBRARIES})
