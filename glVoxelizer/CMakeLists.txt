CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(glVoxelizer)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")
ADD_OPENMP_FLAGS()

FIND_PACKAGE(OpenGL)

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${ANTTWEAKBAR_INCLUDE_DIR}
	${NIVEN_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	../volume/
	../positionSolver/
	../glVoxelVolumePlacerUI/
	../glutil/
	../wml
	../framework
	)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(glVoxelizer
	${GLEW_SOURCE_FILE}
	../positionSolver/debugRender.h
	../positionSolver/cameraInputControl.h
	../positionSolver/camera.h
	../positionSolver/eventHandling.h
	../glVoxelVolumePlacerUI/glHelpers.h
	../volume/gridStorage.h
	../volume/grid.h
	../wml/wml.h
	voxelizer.h
	voxelizer.cpp
	glVoxelizer.cpp
	../gtest/gtest-all.cc
	../build/voxelizer.glsl
	../glutil/Debug.cpp
	../glutil/Debug.h
	)

#ADD_EXECUTABLE(glVoxelizerTests
#	${GLEW_SOURCE_FILE}
#	gridTests.cpp
	#../gtest/gtest_main.cc
	#../gtest/gtest-all.cc
	#)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(glVoxelizer ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelizer ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(glVoxelizer ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(glVoxelizer ${Boost_LIBRARIES})