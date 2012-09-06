CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(sgsSceneViewer)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")
ADD_OPENMP_FLAGS()

FIND_PACKAGE(OpenGL)

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${ANTTWEAKBAR_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${SOIL_INCLUDE_DIR}
	../framework
	../glutil
	../serializer
	../wml
)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(sgsSceneViewer
	${GLEW_SOURCE_FILE}
	../glutil/debugRender.h
	../framework/cameraInputControl.h
	../framework/camera.h
	../framework/eventHandling.h
	../framework/eigenProjectionMatrices.h
	../glutil/Debug.cpp
	../glutil/Debug.h
	../wml/wml.h
	../wml/leanTextProcessing.h
	sgsScene.h
	sgsSceneRender.h
	sgsSceneViewer.cpp
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(sgsSceneViewer ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(sgsSceneViewer ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(sgsSceneViewer ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(sgsSceneViewer ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(sgsSceneViewer ${SOIL_LIBRARY})