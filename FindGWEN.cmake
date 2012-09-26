# Andreas Kirsch

SET(GWEN_DIR CACHE PATH "GWEN root directory")

SET(GWEN_BIN_DIR ${GWEN_DIR}/bin)
SET(GWEN_LIBRARY_DIR ${GWEN_DIR}/lib)
SET(GWEN_INCLUDE_DIR ${GWEN_DIR}/include)

MARK_AS_ADVANCED(GWEN_LIBRARY_DIR GWEN_INCLUDE_DIR)

FIND_LIBRARY(GWEN_LIBRARY_RELEASE	gwen.lib   PATHS ${GWEN_LIBRARY_DIR})
FIND_LIBRARY(GWEN_LIBRARY_DEBUG		gwen-d.lib  PATHS ${GWEN_LIBRARY_DIR})

SET(GWEN_LIBRARY 	debug ${GWEN_LIBRARY_DEBUG}
					optimized ${GWEN_LIBRARY_RELEASE})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GWEN DEFAULT_MSG GWEN_LIBRARY GWEN_INCLUDE_DIR)

FUNCTION(GWEN_COPY_SKIN)
	IF(${ARGC} EQUAL 0)
		SET(TargetDirectory ${CMAKE_BINARY_DIR})
	ELSE(${ARGC} EQUAL 0)
		Set(TargetDirectory ${ARGV0})
	ENDIF(${ARGC} EQUAL 0)

	ADD_CUSTOM_TARGET(
			GwenCopySkin
			COMMAND ${CMAKE_COMMAND} -E copy ${GWEN_BIN_DIR}/DefaultSkin.png ${TargetDirectory}
			COMMENT "Copying GWEN's default skin to '${TargetDirectory}'"
			VERBATIM
		)
ENDFUNCTION(GWEN_COPY_SKIN)