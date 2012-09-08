CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(ctp)

INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	../gtest
	../wml
	)

ADD_EXECUTABLE(ctpTest
	../wml/leanTextProcessing.h
	ctp.h
	ctp.cpp
	ctpTest.cpp
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

ADD_EXECUTABLE(ctp
	ctp.h
	ctp.cpp
	ctpMain.cpp
	../wml/leanTextProcessing.h
	)

TARGET_LINK_LIBRARIES(ctpTest ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(ctp ${Boost_LIBRARIES})