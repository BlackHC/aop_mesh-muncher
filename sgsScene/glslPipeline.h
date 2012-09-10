#pragma once

#include "serializer.h"

#include <string>
#include <unordered_map>
#include <boost/range/algorithm/find.hpp>
#include <list>

#include "shaderPrograms.h"

#include <exception>

struct Program;

struct Shader {
	const Program *currentProgram;

	enum Type {
		ST_SURFACE,
		ST_TARGET,
		ST_MESH,
		ST_OBJECT,
	};

	struct Member {
		std::string name;
		std::string type;
		
		SERIALIZER_PAIR_IMPL( name, type );
	};

	Type type;
	std::string code;
	std::string name;
	std::vector< Member > uniforms, outputs;
	
	SERIALIZER_IMPL( name, (code)(uniforms), (type) );

	std::string getUniformDecls() const {
		std::string out;
		for( auto uniform = uniforms.cbegin() ; uniform != uniforms.cend() ; ++uniform ) {
			out.append( boost::str( boost::format( "uniform %s %s;\n" ) % uniform->type % uniform->name ) );
		}
		return out;
	}

	const char *getPrefix() const {
		switch( type ) {
		case ST_SURFACE:
			return "surface";
		case ST_TARGET:
			return "target";
		case ST_MESH:
			return "mesh";
		case ST_OBJECT:
			return "object";
		default:
			throw std::invalid_argument( "unsupported shader type!" );
		}
	}

	std::string getOutputStructDecl() const {
		std::string out;
		out.append(  boost::str( boost::format( "struct %sOutputs {\n" ) % getPrefix() ) );
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
		out.append( "\n\t// copy to outputs" );
		for( auto member = outputs.cbegin() ; member != outputs.cend() ; ++member ) {
			out.append( boost::str( boost::format( "\t%s_%s = %s.%s;\n" ) % getPrefix() % member->name % structName % member->name ) );
		}
		if( type == ST_MESH ) {
			// default members
			out.append( boost::str( boost::format( "\tgl_Position = %s.position;\n" ) % structName ) );
		}
		out.append( "\n" );
		return out;
	}

	Shader() : currentProgram( nullptr ) {}

	int uniform( const char *name );
};

SERIALIZER_REFLECTION( Shader::Type,
	(("surfaceShader", Shader::ST_SURFACE))
	(("targetShader", Shader::ST_TARGET ))
	(( "meshShader", Shader::ST_MESH ))
	(("objectShader", Shader::ST_OBJECT ))
	)

struct ShaderCollection {
	std::list<Shader> shaders;

	Shader * operator [] ( const char *name ) {
		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			if( shader->name == name ) {
				return &*shader;
			}
		}

		return nullptr;
	}

	template< typename Reader >
	void serializer_read( Reader &reader ) {
		Serializer::read( reader, shaders );
	}

	template< typename Writer >
	void serializer_write( Writer &writer ) const {
		Serializer::write( writer, shaders );
	}
};

struct Program {
	static Program *currentProgram;

	Shader *surfaceShader;
	Shader *meshShader;

	GLuint program;

	std::unordered_map< std::string, GLuint > uniformLocations;

	Program() : surfaceShader( nullptr ), meshShader( nullptr ) {}

	bool build() {
		GLuint fragmentShader = GLUtil::glShaderBuilder( GL_FRAGMENT_SHADER ).
			addSource( "#version 420 compatibility\n\n" ).
			addSource( surfaceShader->getUniformDecls().c_str() ).
			addSource( "\n#line 1\n" ).
			addSource( surfaceShader->code.c_str() ).
			addSource( 
					"\n"
					"void main() {\n"
					"\tgl_FragColor = surfaceShader();\n"
					"}\n"
				).
			compile().dumpInfoLog( std::cout ).
			handle;

		GLuint vertexShader = GLUtil::glShaderBuilder( GL_VERTEX_SHADER ).
			addSource( "#version 420 compatibility\n\n" ).
			addSource( meshShader->getUniformDecls().c_str() ).
			addSource( "\n#line 1\n" ).
			addSource( meshShader->getOutputDecls().c_str() ).
			addSource( meshShader->getOutputStructDecl().c_str() ).
			addSource( meshShader->code.c_str() ).			
			addSource( 
					("\n"
					"void main() {\n"
					"\t MeshOutput output = meshShader();\n\n"
					+ meshShader->getCopyStructToOutputsCode( "output" ) +
					"}\n").c_str()
				).
			compile().dumpInfoLog( std::cout ).
			handle;

		GLUtil::glProgramBuilder programBuilder;
		
		program = programBuilder.attachShader( fragmentShader ).attachShader( vertexShader ).link().dumpInfoLog( std::cout ).deleteShaders().program;

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
		surfaceShader->currentProgram = this;
	}

	static void useFixed() {
		glUseProgram( 0 );
		if( currentProgram ) {
			currentProgram->surfaceShader->currentProgram = nullptr;
		}
		currentProgram = nullptr;
	}
};

Program *Program::currentProgram = nullptr;

int Shader::uniform( const char *name ) {
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