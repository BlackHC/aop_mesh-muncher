#pragma once

#define SERIALIZER_SUPPORT_STL
#define SERIALIZER_SUPPORT_EIGEN
#include "serializer.h"

#include <string>
#include <unordered_map>

#define BOOST_RESULT_OF_USE_DECLTYPE
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include <list>
#include <set>

#include "shaderPrograms.h"

#include <exception>

struct Program;

struct Shader {
	const Program *currentProgram;

	enum Type {
		ST_SURFACE,
		ST_VERTEX,
		ST_MODULE
	};

	struct Member {
		std::string name;
		std::string type;
		
		SERIALIZER_PAIR_IMPL( name, type );
	};

	Type type;
	std::string code;
	std::string name;
	std::vector< Member > uniforms, outputs, inputs;
	std::vector< std::string > dependencies;
	
	SERIALIZER_IMPL( name, (code)(uniforms)(outputs)(inputs)(dependencies), (type) );

	bool validate() const {
		// check inputs and outputs
		switch( type ) {
		case ST_MODULE:
			return inputs.empty() && outputs.empty();
		case ST_SURFACE:
		case ST_VERTEX:
			return true;
		}
	}

	std::string getUniformDecls() const {
		std::string out;

		out.append( "// uniforms\n" );
		for( auto uniform = uniforms.cbegin() ; uniform != uniforms.cend() ; ++uniform ) {
			out.append( boost::str( boost::format( "uniform %s %s;\n" ) % uniform->type % uniform->name ) );
		}
		out.append( "\n\n" );

		return out;
	}
	/*
	std::string getOutputStructDecl() const {
		std::string out;
		out.append(  boost::str( boost::format( "struct %sOutput {\n" ) % getUpperPrefix() ) );
		for( auto member = outputs.cbegin() ; member != outputs.cend() ; ++member ) {
			out.append( boost::str( boost::format( "\t%s %s;\n" ) % member->type % member->name ) );
		}
		if( type == ST_MESH ) {
			// default members
			out.append( 
					"\t//default members\n"
					"\tvec4 position;\n"
				);
		}
		out.append( "};\n\n" );
		return out;
	}

	std::string getOutputDecls() const {
		std::string out;
		for( auto member = outputs.cbegin() ; member != outputs.cend() ; ++member ) {
			out.append( boost::str( boost::format( "out %s %s_%s;\n" ) % member->type % getPrefix() % member->name ) );
		}
		out.append( "\n\n" );
		return out;
	}
	std::string getCopyStructToOutputsCode( const char *structName ) const {
	std::string out;
	out.append( "\n\t// copy to outputs\n" );
	for( auto member = outputs.cbegin() ; member != outputs.cend() ; ++member ) {
	out.append( boost::str( boost::format( "\t%s_%s = %s.%s;\n" ) % getPrefix() % member->name % structName % member->name ) );
	}
	if( type == ST_MESH ) {
	// default members
	out.append( boost::str( boost::format( "\n\tgl_Position = %s.position;\n" ) % structName ) );
	}
	out.append( "\n" );
	return out;
	}

	*/

	std::string getSimpleOutputDecls() const {
		std::string out;
		for( auto member = outputs.cbegin() ; member != outputs.cend() ; ++member ) {
			out.append( boost::str( boost::format( "out %s %s;\n" ) % member->type % member->name ) );
		}
		out.append( "\n\n" );
		return out;
	}

	std::string getSimpleInputDecls() const {
		std::string out;
		for( auto member = inputs.cbegin() ; member != inputs.cend() ; ++member ) {
			out.append( boost::str( boost::format( "in %s %s;\n" ) % member->type % member->name ) );
		}
		out.append( "\n\n" );
		return out;
	}

	void error( const std::string &error ) const {
		throw std::logic_error( (boost::format( "Shader '%s': %s" ) % name % error).str() );
	}

	Shader() : currentProgram( nullptr ) {}

	int uniform( const char *name ) const;
};

SERIALIZER_REFLECTION( Shader::Type,
	(("surfaceShader", Shader::ST_SURFACE))
	(("module", Shader::ST_MODULE))
	(("vertexShader", Shader::ST_VERTEX))
	)

struct ShaderCollection {
	std::list<Shader> shaders;

	Shader * get( const char *name ) {
		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			if( shader->name == name ) {
				return &*shader;
			}
		}

		return nullptr;
	}

	const Shader * get( const char *name ) const {
		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			if( shader->name == name ) {
				return &*shader;
			}
		}

		return nullptr;
	}

	Shader * operator [] ( const char *name ) {
		return get( name );
	}

	const Shader * operator [] ( const char *name ) const {
		return get( name );
	}

	template< typename Reader >
	void serializer_read( Reader &reader ) {
		Serializer::read( reader, shaders );
	}

	template< typename Writer >
	void serializer_write( Writer &writer ) const {
		Serializer::write( writer, shaders );
	}

	std::vector< const Shader * > resolveDependencies( const Shader *shader ) const {
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
};

struct Program {
	static Program *currentProgram;

	Shader *surfaceShader;
	Shader *vertexShader;

	GLuint program;

	std::unordered_map< std::string, GLuint > uniformLocations;

	Program() : surfaceShader( nullptr ), vertexShader( nullptr ), program( 0 ) {}
	~Program() {
		if( program ) {
			glDeleteProgram( program );
		}
	}

	static void mergeDependencies( std::vector< const Shader * > &merged, const std::vector< const Shader * > &source ) {
		int searchSize = merged.size();
		for(int index = 0 ; index < source.size() ; index++ ) {
			if( std::find( merged.begin(), merged.end() + searchSize, source[ index ] ) == merged.end() ) {
				merged.push_back( source[ index ] );
			}
		}
	}

	void error( const std::string &error ) const {
		std::string shaderNames;
		if( surfaceShader ) {
			shaderNames.append( surfaceShader->name );
			shaderNames.push_back( ' ' );
		}
		if( vertexShader ) {
			shaderNames.append( vertexShader->name );
		}

		throw std::logic_error( boost::str( boost::format( "Program %s: %s" ) % shaderNames % error ) );
	}

	static std::string getDependencyCode( const std::vector< const Shader * > &dependencies ) {
		std::string out;
		
		out.append( "// dependencies\n\n" );
		for(int index = 0 ; index < dependencies.size() ; index++ ) {
			const Shader *module = dependencies[ index ];
			out.append( boost::str( boost::format( "// %s\n" ) % module->name ) );
			out.append( module->getUniformDecls() );
			out.append( module->code );
		}

		out.append( "\n\n" );

		return out;
	}

	void reset() {
		if( program ) {
			glDeleteProgram( program );
			program = 0;

			uniformLocations.clear();
		}

	}

	bool build( const ShaderCollection &collection ) {
		reset();

		GLUtil::glProgramBuilder programBuilder;

		if( surfaceShader ) {
			GLuint fragmentShaderHandle = GLUtil::glShaderBuilder( GL_FRAGMENT_SHADER ).
				addSource( "#version 420 compatibility\n\n" ).
				addSource( getDependencyCode( collection.resolveDependencies( surfaceShader ) ) ).
				addSource( surfaceShader->getUniformDecls() ).
				addSource( "\n#line 1\n" ).
				addSource( surfaceShader->getSimpleInputDecls() ).
				addSource( surfaceShader->code ).
				addSource( 
						"\n"
						"void main() {\n"
						"\tgl_FragColor = surfaceShader();\n"
						"}\n"
					).
				compile().
				alwaysKeep().
				handle;

			programBuilder.attachShader( fragmentShaderHandle );
		}

		if( vertexShader ) {
			GLuint vertexShaderHandle = GLUtil::glShaderBuilder( GL_VERTEX_SHADER ).
				addSource( "#version 420 compatibility\n\n" ).
				addSource( getDependencyCode( collection.resolveDependencies( vertexShader ) ) ).
				addSource( vertexShader->getUniformDecls() ).
				addSource( "\n#line 1\n" ).
				addSource( vertexShader->getSimpleInputDecls() ).
				addSource( vertexShader->getSimpleOutputDecls() ).
				addSource( vertexShader->code.c_str() ).			
				addSource( 
						"\n"
						"void main() {\n"
						"\tmeshShader();\n"
						"}\n"
					).
				compile().
				alwaysKeep().
				handle;
			
			programBuilder.attachShader( vertexShaderHandle );
		}		
		
		program = programBuilder.link().dumpInfoLog( std::cout ).deleteShaders().program;

		if( programBuilder.fail ) {
			return false;
		}
		
		// store all uniform locations
		extractUniformLocations();

		return true;
	}

	void extractUniformLocations() {
		int numActiveUniforms;
		glGetProgramiv( program, GL_ACTIVE_UNIFORMS, &numActiveUniforms );

		const int bufferSize = 512;
		char buffer[ bufferSize ];

		for( int uniformIndex = 0 ; uniformIndex < numActiveUniforms ; ++uniformIndex ) {
			glGetActiveUniformName( program, uniformIndex, bufferSize, nullptr, buffer );

			uniformLocations[ std::string( buffer ) ] = uniformIndex;
		}
	}

	void use() {
		glUseProgram( program );
		currentProgram = this;
		vertexShader->currentProgram = this;
	}

	static void useFixed() {
		glUseProgram( 0 );
		if( currentProgram ) {
			currentProgram->surfaceShader->currentProgram = nullptr;
		}
		currentProgram = nullptr;
	}
};

void loadShaderCollection( ShaderCollection &library, const char *filename );