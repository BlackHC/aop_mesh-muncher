#pragma once

#include <GL/glew.h>

struct glShaderBuilder {
	std::vector< const char * > sourceBuffers;
	std::vector< int > sourceBufferLengths;

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
		sourceBuffers.push_back( buffer );
		sourceBufferLengths.push_back( length );
		return *this;
	}

	glShaderBuilder & addSource( const char *buffer ) {
		sourceBuffers.push_back( buffer );
		sourceBufferLengths.push_back( strlen( buffer ) );
		return *this;
	}

	glShaderBuilder & compile() {
		handle = glCreateShader( type );
		if( !handle ) {
			return *this;
		}

		// set sources
		glShaderSource( handle, sourceBuffers.size(), &sourceBuffers.front(), &sourceBufferLengths.front() );

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