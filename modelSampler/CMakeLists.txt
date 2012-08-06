CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(modelSampler)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

FIND_PACKAGE(OpenGL)

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${ANTTWEAKBAR_INCLUDE_DIR} ${NIVEN_INCLUDE_DIR} ${SFML_INCLUDE_DIR} ${EIGEN3_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${GLEW_INCLUDE_DIR} ../gtest/ ../positionSolver/)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(modelSampler
	${GLEW_SOURCE_FILE}
	../positionSolver/debugRender.h
	../positionSolver/cameraInputControl.h
	../positionSolver/camera.h
	../positionSolver/eventHandling.h
	modelSampler.cpp 
	materialLibrary.cpp
	materialLibrary.h
	objModel.cpp
	objModel.h
	grid.h
	../gtest/gtest-all.cc
	)

ADD_EXECUTABLE(modelSamplerTests
	gridTests.cpp
	shearMatrixTests.cpp
	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_GEOMETRY_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${NIVEN_VOLUME_LIBRARY})

TARGET_LINK_LIBRARIES(modelSampler ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(modelSampler ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(modelSampler ${OPENGL_LIBRARIES})