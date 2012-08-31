CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(glVoxelVolumePlacerUI)

FIND_PACKAGE(AntTweakBar REQUIRED)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

ADD_OPENMP_FLAGS()

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${ANTTWEAKBAR_INCLUDE_DIR}
	${NIVEN_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	../positionSolver
	../modelSampler
	../objScene
	../cielab
	../volume
	)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(glVoxelVolumePlacerUI
	${GLEW_SOURCE_FILE}
	make_nonallocated_shared.h
	antTweakBarEventHandler.h
	serializer.h
	anttwbarcollection.h
	contextHelper.h
	glVoxelVolumePlacerUI.cpp 
	anttwbargroup.cpp
	anttwbargroup.h
	volumePlacer.h
	twEventSFML20.cpp
	glHelpers.h
	ui.h
	../modelSampler/colorAndDepthSampler.h
	../modelSampler/eigenProjectionMatrices.h
	../volume/grid.h
	../positionSolver/camera.h
	../positionSolver/cameraInputControl.h
	../positionSolver/debugRender.h
	../positionSolver/eventHandling.h
	../positionSolver/positionSolver.h
	../objScene/objSceneGL.h
	../objScene/objSceneGL.cpp
	../objScene/materialLibrary.h
	../objScene/materialLibrary.cpp
	../build/shader.glsl
	../cielab/cielab.h
	../cielab/cielab.cpp
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_GEOMETRY_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${NIVEN_VOLUME_LIBRARY})

TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(glVoxelVolumePlacerUI ${Boost_LIBRARIES})
