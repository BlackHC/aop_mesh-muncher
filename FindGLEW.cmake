#
# Try to find GLEW library and include path.
# Once done this will define
#
# GLEW_FOUND
# GLEW_INCLUDE_PATH
# GLEW_SOURCE_FILE
# 

FIND_PATH( GLEW_ROOT_DIR NAMES GL/glew.h src/glew.c $ENV{PROGRAMFILES}/GLEW )

FIND_PATH( GLEW_INCLUDE_DIR GL/glew.h
	${GLEW_ROOT_DIR}/include
	DOC "The directory where GL/glew.h resides")

FIND_FILE( GLEW_SOURCE_FILE glew.c
	$ENV{PROGRAMFILES}/GLEW/src
	${GLEW_ROOT_DIR}/src
	DOC "The path to glew.c")		

# define GLEW_STATIC since we're adding the source code directly
add_definitions( -DGLEW_STATIC )
	
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLEW DEFAULT_MSG GLEW_SOURCE_FILE GLEW_INCLUDE_DIR)
