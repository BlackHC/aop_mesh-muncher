CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(volumePlacerUI)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

SET(OBJMODEL_INCLUDE_DIR P:/niven-viewer/include)
SET(OBJMODEL_LIBRARY_DIR P:/niven-viewer/lib)
SET(OBJMODEL_LIBRARY
	optimized 	${OBJMODEL_LIBRARY_DIR}/objmodel.lib
	debug		${OBJMODEL_LIBRARY_DIR}/objmodeld.lib)

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ../gtest/ ../densityPyramid/ ../calibrateVolumeBB)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(volumePlacerUI volumePlacerUI.cpp ../densityPyramid/findDistances.cpp ../calibrateVolumeBB/volumeCalibration.h ../densityPyramid/findDistances.h ../densityPyramid/mipVolume.h ../densityPyramid/cache.h ../densityPyramid/utility.h ../gtest/gtest-all.cc ../densityPyramid/memoryBlockStorage.h)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(volumePlacerUI ${NIVEN_VOLUME_LIBRARY})

TARGET_LINK_LIBRARIES(volumePlacerUI ${OBJMODEL_LIBRARY})
INCLUDE_DIRECTORIES(${OBJMODEL_INCLUDE_DIR})