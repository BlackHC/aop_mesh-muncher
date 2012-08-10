CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(shaderTest)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

FIND_PACKAGE(OpenGL)

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${SFML_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${GLEW_INCLUDE_DIR} )

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(shaderTest
	${GLEW_SOURCE_FILE}
	shaderTest.cpp 
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(shaderTest ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(shaderTest ${OPENGL_LIBRARIES})

