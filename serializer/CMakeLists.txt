CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(serializerTest)

INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	../gtest
	../wml
	)

ADD_EXECUTABLE(serializerTest
	serializer.h
	serializer_eigen.h
	serializer_std.h
	serializerTests.cpp
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	../wml/wml.h
	)

TARGET_LINK_LIBRARIES(serializerTest ${Boost_LIBRARIES})