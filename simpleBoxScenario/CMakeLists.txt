CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(simpleBoxScenario)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ../gtest/ ../densityPyramid/)

# ../gtest/gtest_main.cc
ADD_EXECUTABLE(simpleBoxScenario simpleBoxScenario.cpp ../densityPyramid/findDistances.cpp ../densityPyramid/findDistances.h ../densityPyramid/mipVolume.h ../densityPyramid/cache.h ../densityPyramid/utility.h ../gtest/gtest-all.cc ../densityPyramid/memoryBlockStorage.h)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(simpleBoxScenario ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(simpleBoxScenario ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(simpleBoxScenario ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(simpleBoxScenario ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(simpleBoxScenario ${NIVEN_VOLUME_LIBRARY})