CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Test_framework)

# niven is built with RTTI disabled (/GR-) and unicode-aware
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR- /D_UNICODE")

# Must link both against niven and Boost
INCLUDE_DIRECTORIES(
	${ANTTWEAKBAR_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${SOIL_INCLUDE_DIR}
	${OptiX_INCLUDE}
	${Boost_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIR}
	${SFML_INCLUDE_DIR}
	../gtest
	)

ADD_EXECUTABLE(Test_framework
	anttwbarui.h
	anttwbarui.cpp

	mathUtility.h

	flatImmutableMultiMap.h

	progressTracker.h
	progressTracker.cpp

	test_flatImmutableMultiMap.cpp
	test_rayIntersections.cpp
	test_progressTracker.cpp
	test_antTWBarUI_exp.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

TARGET_LINK_LIBRARIES(Test_framework ${SFML_LIBRARIES})

ADD_EXECUTABLE(Test_framework_antTWBarUI
	eventHandling.h
	verboseEventHandlers.h
	antTweakBarEventHandler.h
	twEventSFML20.cpp
	anttwbarcollection.h
	anttwbarui.h
	anttwbarui.cpp

	mathUtility.h

	test_antTWBarUI.cpp

	../gtest/gtest_main.cc
	../gtest/gtest-all.cc
	)

TARGET_LINK_LIBRARIES(Test_framework_antTWBarUI ${ANTTWEAKBAR_LIBRARY})
TARGET_LINK_LIBRARIES(Test_framework_antTWBarUI ${SFML_LIBRARIES})