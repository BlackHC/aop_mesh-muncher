#pragma once

#include <GL/glew.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>

struct glShaderBuilder {
	std::string source;

	GLenum type;
	GLuint handle;

	bool fail;
	std::string infoLog;

	glShaderBuilder( GLenum type ) {
		this->type = type;
		this->handle = 0;
		this->fail = true;
	}

	glShaderBuilder & addSource( const char *buffer, int length ) {
		source.append( buffer, length );
		return *this;
	}

	glShaderBuilder & addSource( const char *buffer ) {
		source.append( buffer );
		return *this;
	}

	glShaderBuilder & compile() {
		handle = glCreateShader( type );
		if( !handle ) {
			return *this;
		}

		// set sources
		const char *sourcePointer = source.c_str();
		glShaderSource( handle, (GLsizei) 1, &sourcePointer, nullptr );

		glCompileShader( handle );

		GLint status;
		glGetShaderiv( handle, GL_COMPILE_STATUS, &status );
		fail = status == GL_FALSE;

		return *this;
	}

	glShaderBuilder & extractInfoLog() {
		GLint infoLogLength;
		glGetShaderiv( handle, GL_INFO_LOG_LENGTH, &infoLogLength );

		infoLog.resize( infoLogLength + 1 );

		glGetShaderInfoLog( handle, infoLogLength, NULL, &infoLog.front() );

		return *this;
	}

	glShaderBuilder & dumpInfoLog( std::ostream &out ) {
		extractInfoLog();
		out << infoLog;
		return *this;
	}

	~glShaderBuilder() {
		if( fail && handle ) {
			glDeleteShader( handle );
		}
	}
};

struct glProgramBuilder {
	std::vector<GLuint> shaders;

	GLuint program;
	bool fail;

	std::string infoLog;

	glProgramBuilder() {
		program = 0;
		fail = true;
	}

	glProgramBuilder & attachShader( GLuint shader ) {
		if( shader ) {
			shaders.push_back( shader );
		}
		else {
			// TODO:
		}

		return *this;
	}

	glProgramBuilder & link() {
		program = glCreateProgram();
		if( !program ) {
			return *this;
		}

		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			glAttachShader( program, *shader );
		}

		glLinkProgram( program );

		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			glDetachShader( program, *shader );
		}

		GLint status;
		glGetProgramiv( program, GL_LINK_STATUS, &status );
		fail = status == GL_FALSE;

		return *this;
	}

	glProgramBuilder & extractInfoLog() {
		GLint infoLogLength;
		glGetProgramiv( program, GL_INFO_LOG_LENGTH, &infoLogLength );

		infoLog.resize( infoLogLength );

		glGetProgramInfoLog( program, infoLogLength, NULL, &infoLog.front() );

		return *this;
	}

	glProgramBuilder & dumpInfoLog( std::ostream &out ) {
		extractInfoLog();
		out << infoLog;
		return *this;
	}

	glProgramBuilder & deleteShaders() {
		for( auto shader = shaders.begin() ; shader != shaders.end() ; ++shader ) {
			glDeleteShader( *shader );
		}
		shaders.clear();
		return *this;
	}

	~glProgramBuilder() {
		if( fail && program ) {
			glDeleteProgram( program );
		}
	}
};

struct Shader {
	std::string filename;
	std::string include;
	std::string source;
	
	bool hasGeometryShader;
	bool hasFragmentShader;
	bool hasVertexShader;

	GLuint program;

	Shader() : program( 0 ), hasGeometryShader( false  ), hasFragmentShader( true ), hasVertexShader( true ) {}
	~Shader() {
		if( program ) {
			glDeleteProgram( program );
		}
	}

	virtual void setLocations() {}

	void init( const char *filename, const char *include = "" ) {
		this->filename = filename;
		this->include = include;

		// load the shader
		reload();
	}

	// helper
	void readSource() {
		// http://www.gamedev.net/topic/353162-reading-a-whole-file-into-a-string-with-ifstream/
		std::ifstream file( filename );
		source = std::string( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	}

	void reload() {
		glProgramBuilder programBuilder;

		while( true ) {
			if( !filename.empty() ) {
				readSource();
			}

			const char *versionText = "#version 420 compatibility\n";

			glShaderBuilder vertexShader( GL_VERTEX_SHADER );

			vertexShader.	
				addSource( versionText ).
				addSource( include.c_str() ).
				addSource( 
					"#define VERTEX_SHADER 1\n"
					"#define FRAGMENT_SHADER 0\n"
					"#define GEOMETRY_SHADER 0\n"
					"#line 1\n"
				).
				addSource( source.c_str() ).
				compile();

			glShaderBuilder fragmentShader( GL_FRAGMENT_SHADER );

			fragmentShader.
				addSource( versionText ).
				addSource( include.c_str() ).
				addSource( 
					"#define VERTEX_SHADER 0\n"
					"#define FRAGMENT_SHADER 1\n"
					"#define GEOMETRY_SHADER 0\n"
					"#line 1\n"
				).				
				addSource( source.c_str() ).
				compile();

			glShaderBuilder geometryShader( GL_GEOMETRY_SHADER );

			geometryShader.
				addSource( versionText ).
				addSource( include.c_str() ).
				addSource( 
					"#define VERTEX_SHADER 0\n"
					"#define FRAGMENT_SHADER 0\n"
					"#define GEOMETRY_SHADER 1\n"
					"#line 1\n"
				).				
				addSource( source.c_str() ).
				compile();
			
			if( hasVertexShader ) {
				programBuilder.attachShader( vertexShader.handle );
			}

			if( hasFragmentShader ) {
				programBuilder.attachShader( fragmentShader.handle );
			}

			if( hasGeometryShader ) {
				programBuilder.attachShader( geometryShader.handle );
			}

			programBuilder.
				link().
				deleteShaders().
				dumpInfoLog( std::cout );

			if( !programBuilder.fail ) {
				break;
			}
			else {
				__debugbreak();
			}
		}

		program = programBuilder.program;
		setLocations();
	}

	void apply() {
		glUseProgram( program );
	}
};