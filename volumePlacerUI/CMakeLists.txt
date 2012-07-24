CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(volumePlacerUI)

FIND_PACKAGE(AntTweakBar REQUIRED)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${ANTTWEAKBAR_INCLUDE_DIR} ${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ../gtest/ ../densityPyramid/ ../calibrateVolumeBB ../volumePlacer)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(volumePlacerUI
	antTweakBarEventHandler.h
	contextHelper.h
	volumePlacerUI.cpp 
	materialLibrary.cpp
	materialLibrary.h
	objModel.cpp
	objModel.h
	anttwbargroup.cpp
	anttwbargroup.h
	volumePlacer.h
	ui.h
	../calibrateVolumeBB/volumeCalibration.h 
	../densityPyramid/memoryBlockStorage.h
	../densityPyramid/findDistances.cpp 
	../densityPyramid/findDistances.h
	../densityPyramid/mipVolume.h
	../densityPyramid/cache.h
	../densityPyramid/utility.h
	../gtest/gtest-all.cc
	)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_VOLUME_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${ANTTWEAKBAR_LIBRARY})

ANTTWEAKBAR_COPY_BINARIES()