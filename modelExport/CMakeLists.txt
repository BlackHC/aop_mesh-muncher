CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(modelExport)

FIND_PACKAGE(AntTweakBar REQUIRED)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${ANTTWEAKBAR_INCLUDE_DIR} ${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ../gtest/)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(modelExport
	contextHelper.h
	modelExport.cpp 
	materialLibrary.cpp
	materialLibrary.h
	objModel.cpp
	objModel.h
	../gtest/gtest-all.cc
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(modelExport ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(modelExport ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(modelExport ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(modelExport ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(modelExport ${NIVEN_VOLUME_LIBRARY})
TARGET_LINK_LIBRARIES(modelExport ${ANTTWEAKBAR_LIBRARY})