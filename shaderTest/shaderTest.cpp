
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window.hpp>
#include <GL/glew.h>

#include <iostream>
#include <vector>

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
	}

	~glProgramBuilder() {
		if( fail && program ) {
			glDeleteProgram( program );
		}
	}
};

////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
	// Create the main window
	sf::Window window(sf::VideoMode(640, 480, 32), "SFML Window", sf::Style::Default, sf::ContextSettings(32));

	glewInit();

	// Create a clock for measuring the time elapsed
	sf::Clock clock;

	// Set the color and depth clear values
	glClearDepth(1.f);
	glClearColor(0.f, 0.f, 0.f, 0.f);

	// Enable Z-buffer read and write
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// Setup a perspective projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(90.f, 1.f, 1.f, 500.f);

	glProgramBuilder programBuilder;

	programBuilder.
		attachShader(
			glShaderBuilder( GL_VERTEX_SHADER ).
				addSource( "varying vec4 viewPos; void main() { gl_FrontColor = gl_Color; viewPos = gl_ModelViewMatrix * gl_Vertex; gl_Position = ftransform(); }" ).
				compile().
				handle
		).
		attachShader( 
			glShaderBuilder( GL_FRAGMENT_SHADER ).
				addSource( "varying vec4 viewPos; void main() { gl_FragColor = gl_Color * -100.0 / viewPos.z; }" ).
				compile().
				handle
		).
		link().
		dumpInfoLog( std::cout );

	// Start the game loop
	while (window.isOpen())
	{
		// Process events
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Close window : exit
			if (event.type == sf::Event::Closed)
				window.close();

			// Escape key : exit
			if ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Escape))
				window.close();

			// Resize event : adjust viewport
			if (event.type == sf::Event::Resized)
				glViewport(0, 0, event.size.width, event.size.height);
	   }

		// Activate the window before using OpenGL commands.
		// This is useless here because we have only one window which is
		// always the active one, but don't forget it if you use multiple windows
		window.setActive();

		// Clear color and depth buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Apply some transformations
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(0.f, 0.f, -200.f);
		glRotatef(clock.getElapsedTime().asSeconds() * 50, 1.f, 0.f, 0.f);
		glRotatef(clock.getElapsedTime().asSeconds() * 30, 0.f, 1.f, 0.f);
		glRotatef(clock.getElapsedTime().asSeconds() * 90, 0.f, 0.f, 1.f);

		glUseProgram( programBuilder.program );

		// Draw a cube
		glBegin(GL_QUADS);

			glColor3f(1.f, 0.f, 0.f);
			glVertex3f(-50.f, -50.f, -50.f);
			glVertex3f(-50.f,  50.f, -50.f);
			glVertex3f( 50.f,  50.f, -50.f);
			glVertex3f( 50.f, -50.f, -50.f);

			glColor3f(1.f, 0.f, 0.f);
			glVertex3f(-50.f, -50.f, 50.f);
			glVertex3f(-50.f,  50.f, 50.f);
			glVertex3f( 50.f,  50.f, 50.f);
			glVertex3f( 50.f, -50.f, 50.f);

			glColor3f(0.f, 1.f, 0.f);
			glVertex3f(-50.f, -50.f, -50.f);
			glVertex3f(-50.f,  50.f, -50.f);
			glVertex3f(-50.f,  50.f,  50.f);
			glVertex3f(-50.f, -50.f,  50.f);

			glColor3f(0.f, 1.f, 0.f);
			glVertex3f(50.f, -50.f, -50.f);
			glVertex3f(50.f,  50.f, -50.f);
			glVertex3f(50.f,  50.f,  50.f);
			glVertex3f(50.f, -50.f,  50.f);

			glColor3f(0.f, 0.f, 1.f);
			glVertex3f(-50.f, -50.f,  50.f);
			glVertex3f(-50.f, -50.f, -50.f);
			glVertex3f( 50.f, -50.f, -50.f);
			glVertex3f( 50.f, -50.f,  50.f);

			glColor3f(0.f, 0.f, 1.f);
			glVertex3f(-50.f, 50.f,  50.f);
			glVertex3f(-50.f, 50.f, -50.f);
			glVertex3f( 50.f, 50.f, -50.f);
			glVertex3f( 50.f, 50.f,  50.f);

		glEnd();

		// Finally, display the rendered frame on screen
		window.display();
	}

	return EXIT_SUCCESS;
}
