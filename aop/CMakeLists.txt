CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
# has to before we set the new project
SET(CUDA_GENERATED_OUTPUT_DIR "${PROJECT_BINARY_DIR}")

PROJECT(aop)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")
# get rid of /RTC
STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

# windows.h sucks..
# and turn on multiprocessor compilation
# and disable some warnings (size_t to int conversion, elements of array 'array' will be default initialized and the SCL_UNSECURE_WARNING)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX /MP /wd4267 /wd4351 /wd4996")
# this is needed because sfml graphics uses stbi, too
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FORCE:MULTIPLE")

ADD_OPENMP_FLAGS()

FIND_PACKAGE(OpenGL)
FIND_PACKAGE(Cuda)
FIND_PACKAGE(OptiX)
FIND_PACKAGE(AntTweakBar REQUIRED)

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
	../generic
	../gtest
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
	../framework/contextHelper.h

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	../framework/antTweakBarEventHandler.h
	../framework/twEventSFML20.cpp
	../framework/anttwbarcollection.h
	../framework/anttwbarui.h
	../framework/anttwbarui.cpp

	../glutil/Debug.cpp
	../glutil/Debug.h
	../glutil/shaderPrograms.h
	../glutil/glObjectWrappers.h
	../glutil/debugRender.h

	../cielab/cielab.h
	../cielab/cielab.cpp

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
	../build/voxelizer.shaders

	../generic/sort_permute_iter.h

	aopAntTWBarUIHelpers.h

	aopApplication.h
	aopSettings.h
	aopSettingsStorage.cpp
	aopSettingsStorage.h
	aopMain.cpp

	aopMainUI.h
	aopMainUI.cpp

	aopTimedLog.h
	aopTimedLog.cpp

	aopCandidateSidebarUI.h
	aopCandidateSidebarUI.cpp

	aopModelTypesUI.h
	aopModelTypesUI.cpp

	aopCameraViewsUI.h
	aopCameraViewsUI.cpp

	aopQueryVolumesUI.h
	aopQueryVolumesUI.cpp

	probeGenerator.h
	probeGenerator.cpp
	sgsInterface.h
	sgsInterface.cpp
	optixEigenInterop.h

	visualizations.h
	visualizations.cpp

	modelDatabase.h
	modelDatabase.cpp
	modelDatabaseStorage.h
	modelDatabaseStorage.cpp

	queryResult.h
	queryResultStorage.h

	probeDatabase.h
	probeDatabase.cpp
	probeDatabaseQueries.h
	probeDatabaseStorage.h
	probeDatabaseStorage.cpp

	neighborhoodDatabase.h
	neighborhoodDatabase.cpp
	neighborhoodDatabaseStorage.h
	neighborhoodDatabaseStorage.cpp

	editor.h
	editor.cpp

	viewportContext.h
	viewportContext.cpp

	widgets.h
	widgets.cpp

	modelButtonWidget.h
	modelButtonWidget.cpp

	validation.h
	validation.cpp
	validationStorage.h
	validationStorage.cpp
)


ADD_EXECUTABLE(Validate_aop_neighborhood
	queryResult.h
	queryResultStorage.h

	modelDatabase.h
	modelDatabase.cpp
	modelDatabaseStorage.h
	modelDatabaseStorage.cpp

	neighborhoodDatabase.h
	neighborhoodDatabase.cpp
	neighborhoodDatabaseStorage.h
	neighborhoodDatabaseStorage.cpp

	probeGenerator.h
	probeGenerator.cpp

	validationTools.h
	validation.h
	validation.cpp
	validationStorage.h
	validationStorage.cpp

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	validation_neighborhood.cpp
)


ADD_EXECUTABLE(Validate_aop_combineResults
	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	validation_combineResults.cpp
)

ADD_EXECUTABLE(Validate_aop_probes
	queryResult.h
	queryResultStorage.h

	modelDatabase.h
	modelDatabase.cpp
	modelDatabaseStorage.h
	modelDatabaseStorage.cpp

	probeGenerator.h
	probeGenerator.cpp

	probeDatabase.h
	probeDatabase.cpp
	probeDatabaseQueries.h
	probeDatabaseStorage.h
	probeDatabaseStorage.cpp

	validationTools.h
	validation.h
	validation.cpp
	validationStorage.h
	validationStorage.cpp

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	validation_probes.cpp
)


ADD_EXECUTABLE(Test_aop
	queryResult.h
	queryResultStorage.h

	probeDatabase.h
	probeDatabase.cpp
	probeDatabaseStorage.cpp

	neighborhoodDatabase.h
	neighborhoodDatabase.cpp
	neighborhoodDatabaseStorage.cpp

	probeGenerator.h
	probeGenerator.cpp

	../framework/logger.h
	../framework/logger.cpp

	../framework/progressTracker.h
	../framework/progressTracker.cpp

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	test_probeDatabase.cpp
	test_neighborhoodDatabase.cpp
	test_probeGenerator.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
)

ADD_EXECUTABLE(Test_aop_widgets
	${GLEW_SOURCE_FILE}

	../framework/eventHandling.h

	../framework/verboseEventHandlers.h

	../framework/mathUtility.h
	../framework/contextHelper.h

	../framework/autoTimer.h
	../framework/autoTimer.cpp

	../framework/antTweakBarEventHandler.h
	../framework/twEventSFML20.cpp
	../framework/anttwbarcollection.h
	../framework/anttwbarui.h
	../framework/anttwbarui.cpp

	viewportContext.h
	viewportContext.cpp

	test_widgets.cpp

	widgets.h
	widgets.cpp
)

SET_SOURCE_FILES_PROPERTIES(
	probeDatabase.cpp
	PROPERTIES
	COMPILE_FLAGS "/Ox /Ob2 /Oi /Ot"
)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(aop ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(aop ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(aop ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(aop ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(aop ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(aop ${optix_LIBRARY})

#TARGET_LINK_LIBRARIES(aop ${GWEN_LIBRARY})

TARGET_LINK_LIBRARIES(Validate_aop_neighborhood ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(Validate_aop_neighborhood ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(Validate_aop_neighborhood ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(Validate_aop_neighborhood ${optix_LIBRARY})

TARGET_LINK_LIBRARIES(Validate_aop_probes ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(Validate_aop_probes ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(Validate_aop_probes ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(Validate_aop_probes ${optix_LIBRARY})

TARGET_LINK_LIBRARIES(Validate_aop_combineResults ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(Validate_aop_combineResults ${SFML_LIBRARIES})

TARGET_LINK_LIBRARIES(Test_aop ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(Test_aop ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(Test_aop ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(Test_aop ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(Test_aop ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(Test_aop ${optix_LIBRARY})

TARGET_LINK_LIBRARIES(Test_aop_widgets ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(Test_aop_widgets ${SFML_LIBRARIES})
TARGET_LINK_LIBRARIES(Test_aop_widgets ${OPENGL_LIBRARIES})

TARGET_LINK_LIBRARIES(Test_aop_widgets ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(Test_aop_widgets ${SOIL_LIBRARY})
TARGET_LINK_LIBRARIES(Test_aop_widgets ${optix_LIBRARY})

SOURCE_GROUP( "aop" REGULAR_EXPRESSION "[^/\\]*$" )
SOURCE_GROUP( "build" REGULAR_EXPRESSION "../build/[^/\\]*" )
SOURCE_GROUP( "framework" REGULAR_EXPRESSION "../framework/[^/]*" )
SOURCE_GROUP( "sgsScene" REGULAR_EXPRESSION "../sgsScene/[^/]*" )
SOURCE_GROUP( "glutil" REGULAR_EXPRESSION "../glutil/[^/]*" )
SOURCE_GROUP( "wml" REGULAR_EXPRESSION "../wml/[^/]*" )
SOURCE_GROUP( "texturePacker" REGULAR_EXPRESSION "../texturePacker/[^/]*" )
SOURCE_GROUP( "generic" REGULAR_EXPRESSION "../generic/[^/]*" )
SOURCE_GROUP( "cielab" REGULAR_EXPRESSION "../cielab/[^/]*" )
