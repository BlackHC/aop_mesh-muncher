CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
# has to before we set the new project
SET(CUDA_GENERATED_OUTPUT_DIR "${PROJECT_BINARY_DIR}")

PROJECT(aop)

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
#	${GWEN_INCLUDE_DIR}
	../framework
	../glutil
	../serializer
	../wml
	../texturePacker
	../volume
	../sgsScene
)

# ../gtest/gtest_main.cc
CUDA_COMPILE_PTX( PTX_FILES ../sgsScene/raytracer.cu ../sgsScene/terrainMesh.cu ../sgsScene/objectMesh.cu ../sgsScene/probeTracer.cu  ../sgsScene/pinholeSelection.cu )

CUDA_ADD_EXECUTABLE(aop
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

	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../glutil/debugRender.h

	../wml/wml.h
	../wml/leanTextProcessing.h

	../sgsScene/sgsScene.h
	../sgsScene/sgsSceneRenderer.h
	../sgsScene/sgsSceneRenderer.cpp

	../sgsScene/optixProgramInterface.h
	../sgsScene/optixProgramHelpers.h
	../sgsScene/optixProgramHelpers.cpp
	../sgsScene/optixRendering.cpp
	../sgsScene/optixRenderer.h
	../sgsScene/rendering.h

	../texturePacker/Rect.cpp
	../texturePacker/Rect.h
	../texturePacker/MaxRectsBinPack.cpp
	../texturePacker/MaxRectsBinPack.h

	${PTX_FILES}
	../build/sgsScene.shaders

	aopMain.cpp
	probeGenerator.h
	probeGenerator.cpp
	sgsInterface.h
	sgsInterface.cpp
	optixEigenInterop.h
	probeVisualization.cpp
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(aop ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(aop ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(aop ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(aop ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(aop ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(aop ${optix_LIBRARY})

#TARGET_LINK_LIBRARIES(aop ${GWEN_LIBRARY})