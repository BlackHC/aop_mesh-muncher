CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(shard1)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
FIND_PACKAGE(Niven REQUIRED COMPONENTS Core)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(${NIVEN_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

ADD_EXECUTABLE(shard1 shard1.cpp)

# The test application uses niven core, so link against it
TARGET_LINK_LIBRARIES(shard1 ${NIVEN_CORE_LIBRARY})
TARGET_LINK_LIBRARIES(shard1 ${NIVEN_ENGINE_LIBRARY})
TARGET_LINK_LIBRARIES(shard1 ${NIVEN_RENDER_LIBRARY})
TARGET_LINK_LIBRARIES(shard1 ${NIVEN_IMAGE_LIBRARY})
TARGET_LINK_LIBRARIES(shard1 ${NIVEN_VOLUME_LIBRARY})

# Adds a new build target which copies the niven binaries to
# the output folder
NIVEN_COPY_BINARIES(${CMAKE_BINARY_DIR})