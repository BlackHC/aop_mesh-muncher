CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(wmlTest)

INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	../gtest
	)

ADD_EXECUTABLE(wmlTest
	wml.h
	wml_node.h
	wml_detail_parser.h
	wml_detail_emitter.h
	leanTextProcessing.h
	wmlTest.cpp
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

ADD_EXECUTABLE(reemitter
	wml.h
	wml_node.h
	wml_detail_parser.h
	wml_detail_emitter.h
	leanTextProcessing.h
	reemitter.cpp
	)

TARGET_LINK_LIBRARIES(wmlTest ${Boost_LIBRARIES})