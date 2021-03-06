
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} ${CMAKE_MODULE_PATH})

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost REQUIRED COMPONENTS chrono timer system program_options)

FIND_PACKAGE(Niven REQUIRED COMPONENTS Core)
FIND_PACKAGE(Eigen3)
FIND_PACKAGE(GLEW)
FIND_PACKAGE(OpenMP REQUIRED)
FIND_PACKAGE(SOIL)
FIND_PACKAGE(GWEN)

FUNCTION(ADD_OPENMP_FLAGS)
	IF(OPENMP_FOUND)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}" PARENT_SCOPE)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" PARENT_SCOPE)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}" PARENT_SCOPE)
	ELSE(OPENMP_FOUND)
		MESSAGE("OPENMP NOT FOUND")
	ENDIF(OPENMP_FOUND)
ENDFUNCTION(ADD_OPENMP_FLAGS)

# inline any suitable function
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob2" )

SET(SFML_STATIC_LIBRARIES TRUE CACHE BOOL "using static libraries" FORCE)
FIND_PACKAGE(SFML COMPONENTS System Window Graphics)

ADD_SUBDIRECTORY(densityPyramid)
#ADD_SUBDIRECTORY(blockMipmaps)
#ADD_SUBDIRECTORY(simpleBoxScenario)
#ADD_SUBDIRECTORY(sampleBoxSphere)
#ADD_SUBDIRECTORY(volumePlacer)
ADD_SUBDIRECTORY(minimumDensityClustering)
ADD_SUBDIRECTORY(volumePlacerUI)
ADD_SUBDIRECTORY(calibrateVolumeBB)

ADD_SUBDIRECTORY(positionSolver)

ADD_SUBDIRECTORY(modelSampler)
ADD_SUBDIRECTORY(modelExport)

ADD_SUBDIRECTORY(glVolumePlacerUI)
ADD_SUBDIRECTORY(glVoxelVolumePlacerUI)

ADD_SUBDIRECTORY(cielab)
ADD_SUBDIRECTORY(volume)

ADD_SUBDIRECTORY(glVoxelizer)

ADD_SUBDIRECTORY(wml)
ADD_SUBDIRECTORY(serializer)

ADD_SUBDIRECTORY(sgsScene)

ADD_SUBDIRECTORY(ctp)

ADD_SUBDIRECTORY(aop)

ADD_SUBDIRECTORY(generic)
ADD_SUBDIRECTORY(framework)

# Adds a new build target which copies the niven binaries to
# the output folder
NIVEN_COPY_BINARIES(${CMAKE_BINARY_DIR})
ANTTWEAKBAR_COPY_BINARIES()
GWEN_COPY_SKIN()