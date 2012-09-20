#include "glslPipeline.h"

Program *Program::currentProgram = nullptr;

// TODO: move these functions into their own file [9/19/2012 kirschan2]
int Shader::uniform( const char *name ) const {
	if( !currentProgram ) {
		return -1;
	}

	auto location = currentProgram->uniformLocations.find( name );
	if( location != currentProgram->uniformLocations.end() ) {
		return location->second;
	}

	return -1;
}

void loadShaderCollection( ShaderCollection &library, const char *filename ) {
	ShaderCollection subCollection;

	for( bool success = false ; !success ; ) {
		subCollection.shaders.clear();

		try
		{
			Serializer::TextReader reader( filename );
			Serializer::read( reader, library );

			success = true;
		}
		catch (std::exception &e)
		{
			std::cerr << e.what();
			__debugbreak();
		}
	}

	library.shaders.splice( library.shaders.end(), subCollection.shaders );
}