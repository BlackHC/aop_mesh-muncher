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
)

# ../gtest/gtest_main.cc
CUDA_COMPILE_PTX( PTX_FILES raytracer.cu terrainMesh.cu objectMesh.cu probeTracer.cu  pinholeSelection.cu )

CUDA_ADD_EXECUTABLE(optix_sgsSceneViewer
	${GLEW_SOURCE_FILE}
	../glutil/debugRender.h
	../framework/cameraInputControl.h
	../framework/camera.h
	../framework/eventHandling.h
	../framework/eigenProjectionMatrices.h
	../framework/debugWindows.h
	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../wml/wml.h
	../wml/leanTextProcessing.h
	../framework/glslPipeline.h
	../framework/glslPipeline.cpp
	sgsScene.h
	sgsSceneRenderer.h
	sgsSceneRenderer.cpp
	sgsSceneViewer.cpp
	../texturePacker/Rect.cpp
	../texturePacker/Rect.h
	../texturePacker/MaxRectsBinPack.cpp
	../texturePacker/MaxRectsBinPack.h
	optixProgramInterface.h
	optixProgramHelpers.h
	optixProgramHelpers.cpp
	optixRendering.cpp
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