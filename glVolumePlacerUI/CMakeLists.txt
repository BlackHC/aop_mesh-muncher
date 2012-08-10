CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(glVolumePlacerUI)

FIND_PACKAGE(AntTweakBar REQUIRED)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

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
	)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(glVolumePlacerUI
	${GLEW_SOURCE_FILE}
	antTweakBarEventHandler.h
	ptree_serializer.h
	anttwbarcollection.h
	contextHelper.h
	glVolumePlacerUI.cpp 
	anttwbargroup.cpp
	anttwbargroup.h
	volumePlacer.h
	twEventSFML20.cpp
	ui.h
	../modelSampler/depthSampler.h
	../modelSampler/eigenProjectionMatrices.h
	../modelSampler/grid.h
	../positionSolver/camera.h
	../positionSolver/cameraInputControl.h
	../positionSolver/debugRender.h
	../positionSolver/eventHandling.h
	../objScene/objSceneGL.h
	../objScene/objSceneGL.cpp
	../objScene/materialLibrary.h
	../objScene/materialLibrary.cpp
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_GEOMETRY_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${NIVEN_VOLUME_LIBRARY})

TARGET_LINK_LIBRARIES(glVolumePlacerUI ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(glVolumePlacerUI ${OPENGL_LIBRARIES})