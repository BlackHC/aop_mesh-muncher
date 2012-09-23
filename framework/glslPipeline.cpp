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

std::vector< const Shader * > ShaderCollection::resolveDependencies( const Shader *shader ) const {
	std::vector< const Shader * > moduleStack;

	std::vector< const Shader * > resolved;

	moduleStack.push_back( shader );

	while( !moduleStack.empty() ) {
		const Shader *current = moduleStack.back();
		moduleStack.pop_back();

		if( boost::find( moduleStack, current ) != moduleStack.end() ) {
			current->error( "has cyclic dependency!" );
		}

		const auto unresolvedDependencies = current->dependencies |
			boost::adaptors::transformed( 
			[&, this] ( const std::string &name ) -> const Shader * {
				auto shader = get( name.c_str() );
				if( !shader ) {
					current->error( (boost::format( "'%s' not found in collection!") % name).str() );
				}
				if( shader->type != Shader::ST_MODULE ) {
					current->error( (boost::format( "'%s' is not a module!") % name).str() );
				}
				return shader;
				return nullptr;
		} ) |
			boost::adaptors::filtered( [&] (const Shader *shader ) { return boost::find( resolved, current ) == resolved.end(); } );

		if( boost::empty( unresolvedDependencies ) ) {
			// all dependencies already resolved, so we can add it
			// just dont add the shader itself, because it usually is not a module
			if( current != shader )
				resolved.push_back( current );
		}
		else {
			boost::push_back( moduleStack, unresolvedDependencies | boost::adaptors::reversed );
		}
	}

	return resolved;
}
