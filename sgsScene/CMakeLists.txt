CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
# has to before we set the new project
SET(CUDA_GENERATED_OUTPUT_DIR "${PROJECT_BINARY_DIR}")

PROJECT(optix_sgsSceneViewer)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# windows.h sucks..
# and turn on multiprocessor compilation
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX /MP")
# this is needed because sfml graphics uses stbi, too
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FORCE:MULTIPLE")

ADD_OPENMP_FLAGS()

FIND_PACKAGE(OpenGL)
FIND_PACKAGE(Cuda)
FIND_PACKAGE(OptiX)

MESSAGE( "CUDA PTX Target: '${CUDA_GENERATED_OUTPUT_DIR}'" )

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${ANTTWEAKBAR_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${SOIL_INCLUDE_DIR}
	${OptiX_INCLUDE}
	../framework
	../glutil
	../serializer
	../wml
	../texturePacker
	../volume
)

# ../gtest/gtest_main.cc
CUDA_COMPILE_PTX( PTX_FILES raytracer.cu terrainMesh.cu objectMesh.cu probeTracer.cu  pinholeSelection.cu )

CUDA_ADD_EXECUTABLE(optix_sgsSceneViewer
	${GLEW_SOURCE_FILE}
	../framework/cameraInputControl.h
	../framework/camera.h

	../framework/eventHandling.h
	../framework/eigenProjectionMatrices.h

	../framework/debugWindows.h
	../framework/verboseEventHandlers.h

	../framework/glslPipeline.h
	../framework/glslPipeline.cpp

	../framework/mathUtility.h
	../framework/contextHelper.h

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../glutil/debugRender.h

	../wml/wml.h
	../wml/leanTextProcessing.h

	../texturePacker/Rect.cpp
	../texturePacker/Rect.h
	../texturePacker/MaxRectsBinPack.cpp
	../texturePacker/MaxRectsBinPack.h

	sgsScene.h
	sgsSceneRenderer.h
	sgsSceneRenderer.cpp
	sgsSceneViewer.cpp

	optixProgramInterface.h
	optixProgramHelpers.h
	optixProgramHelpers.cpp
	optixRendering.cpp
	optixRenderer.h
	rendering.h
	${PTX_FILES}
	../build/sgsScene.shaders
	../build/voxelizer.shaders
)

CUDA_ADD_EXECUTABLE(optix_sgsSceneVoxelizer
	${GLEW_SOURCE_FILE}
	../framework/cameraInputControl.h
	../framework/camera.h

	../framework/eventHandling.h
	../framework/eigenProjectionMatrices.h

	../framework/debugWindows.h
	../framework/verboseEventHandlers.h

	../framework/glslPipeline.h
	../framework/glslPipeline.cpp

	../framework/mathUtility.h
	../framework/contextHelper.h

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../glutil/debugRender.h

	../wml/wml.h
	../wml/leanTextProcessing.h

	../texturePacker/Rect.cpp
	../texturePacker/Rect.h
	../texturePacker/MaxRectsBinPack.cpp
	../texturePacker/MaxRectsBinPack.h

	sgsScene.h
	sgsSceneRenderer.h
	sgsSceneRenderer.cpp
	main_sgsSceneVoxelizer.cpp

	optixProgramInterface.h
	optixProgramHelpers.h
	optixProgramHelpers.cpp
	optixRendering.cpp
	optixRenderer.h
	rendering.h
	${PTX_FILES}
	../build/sgsScene.shaders
	../build/voxelizer.shaders
)

CUDA_ADD_EXECUTABLE(optix_sgsSceneProbePOC
	${GLEW_SOURCE_FILE}
	../glutil/debugRender.h
	../framework/cameraInputControl.h
	../framework/camera.h
	../framework/eventHandling.h
	../framework/eigenProjectionMatrices.h
	../framework/debugWindows.h
	../framework/verboseEventHandlers.h
	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../wml/wml.h
	../wml/leanTextProcessing.h
	../framework/glslPipeline.h
	../framework/glslPipeline.cpp
	../volume/grid.h
	sgsScene.h
	sgsSceneRenderer.h
	sgsSceneRenderer.cpp
	sgsSceneProbePOC.cpp
	../texturePacker/Rect.cpp
	../texturePacker/Rect.h
	../texturePacker/MaxRectsBinPack.cpp
	../texturePacker/MaxRectsBinPack.h
	optixProgramInterface.h
	optixProgramHelpers.h
	optixProgramHelpers.cpp
	optixRendering.cpp
	optixRenderer.h
	rendering.h
	${PTX_FILES}
	../build/sgsScene.shaders
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneViewer ${optix_LIBRARY})

TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneVoxelizer ${optix_LIBRARY})

TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(optix_sgsSceneProbePOC ${optix_LIBRARY})