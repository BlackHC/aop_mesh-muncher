#include "voxelizer.h"

#include "glHelpers.h"
#include <Eigen/Eigen>
#include <unsupported/Eigen/OpenGLSupport>

using namespace Eigen;

#include <boost/timer/timer.hpp>
#include <boost/format.hpp>

#include "eigenProjectionMatrices.h"

#include <yaml-cpp/yaml.h>

struct MyShader {
	std::string filename;
	std::string programName;

	YAML::Node definition;

	GLuint program;

	MyShader() : program( 0 ) {}
	~MyShader() {
		if( program ) {
			glDeleteProgram( program );
		}
	}

	virtual void setLocations() {}

	void init( const char *filename, const char *programName = "" ) {
		this->filename = filename;
		this->programName = programName;

		// load the shader
		reload();
	}

	void reload() {
		glProgramBuilder programBuilder;

		while( true ) {
			while( true ) {
				try {
					definition = YAML::LoadFile( filename );
					if( !programName.empty() ) {
						definition = definition[ programName ];
					}
					break;
				}
				catch(const YAML::Exception& e) {
					std::cerr << e.what() << "\n";
					__debugbreak();
				}
			}
			
			const char *versionText = "#version 420 compatibility\n";

			glShaderBuilder vertexShader( GL_VERTEX_SHADER );

			vertexShader.	
				addSource( versionText ).
				addSource( 
				"#define VERTEX_SHADER 1\n"
				"#define FRAGMENT_SHADER 0\n"
				"#define GEOMETRY_SHADER 0\n"
				"#line 1\n"
				);

			glShaderBuilder fragmentShader( GL_FRAGMENT_SHADER );

			fragmentShader.
				addSource( versionText ).
				addSource( 
				"#define VERTEX_SHADER 0\n"
				"#define FRAGMENT_SHADER 1\n"
				"#define GEOMETRY_SHADER 0\n"
				"#line 1\n"
				);

			glShaderBuilder geometryShader( GL_GEOMETRY_SHADER );

			geometryShader.
				addSource( versionText ).
				addSource( 
				"#define VERTEX_SHADER 0\n"
				"#define FRAGMENT_SHADER 0\n"
				"#define GEOMETRY_SHADER 1\n"
				"#line 1\n"
				);

			bool hasVertexShader = false, hasFragmentShader = false, hasGeometryShader = false;

			for( auto entry = definition.begin() ; entry != definition.end() ; ++entry ) {
				const std::string sourceType = entry->first.as<std::string>();
				const std::string source = entry->second.as<std::string>();
				if( sourceType == "global" ) {
					fragmentShader.addSource( source.c_str() );
					geometryShader.addSource( source.c_str() );
					vertexShader.addSource( source.c_str() );
				}
				else if( sourceType == "fragment" ) {
					hasFragmentShader = true;
					if( !source.empty() )
						fragmentShader.addSource( source.c_str() );
				}
				else if( sourceType == "vertex" ) {
					hasVertexShader = true;
					if( !source.empty() )
						vertexShader.addSource( source.c_str() );
				}
				else if( sourceType == "geometry" ) {
					hasGeometryShader = true;
					if( !source.empty() )
						geometryShader.addSource( source.c_str() );
				}
			}
			
			fragmentShader.compile();
			geometryShader.compile();
			vertexShader.compile();

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

struct SplatShader : MyShader {
	void init() {
		MyShader::init( "voxelizer.glsl", "splat" );
	}

	GLuint mainAxisProjection[3];
	GLuint mainAxisPermutation[3];

	GLuint volumeChannels[4];

	void setLocations() {
		for( int i = 0 ; i < 3 ; ++i ) {
			mainAxisProjection[i] = glGetUniformLocation( program, boost::str( boost::format( "mainAxisProjection[%i]" ) % i ).c_str() );
			mainAxisPermutation[i] = glGetUniformLocation( program, boost::str( boost::format( "mainAxisPermutation[%i]" ) % i ).c_str() );
		}
		for( int i = 0 ; i < 4 ; ++i ) {
			volumeChannels[i] = glGetUniformLocation( program, boost::str( boost::format( "volumeChannels[%i]" ) % i ).c_str() );
		}
	}
};

struct MuxerShader : MyShader {
	void init() {		
		MyShader::init( "voxelizer.glsl", "muxer" );
	}

	GLuint volumeChannels[4], volume, sizeHelper;

	void setLocations() {
		for( int i = 0 ; i < 4 ; ++i ) {
			volumeChannels[i] = glGetUniformLocation( program, boost::str( boost::format( "volumeChannels[%i]" ) % i ).c_str() );
		}
		volume = glGetUniformLocation( program, "volume" );
		sizeHelper = glGetUniformLocation( program, "sizeHelper" );
	}
};

void voxelizeScene( const SimpleIndexMapping3 &indexMapping3, ColorGrid &grid, std::function<void()> renderScene ) {
	boost::timer::auto_cpu_timer timer;

	glPushAttrib( GL_ALL_ATTRIB_BITS );
	glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

	glPixelStorei( GL_PACK_ALIGNMENT, 1 );

	SplatShader splatShader;
	splatShader.init();

	unsigned __int32 *volumeChannelsData[4];

	splatShader.apply();

	GLuint volumeChannels[4];
	glGenTextures( 4, volumeChannels );
	for( int i = 0 ; i < 4 ; ++i ) {
		volumeChannelsData[i] = new unsigned __int32[ indexMapping3.count ];
		memset( volumeChannelsData[i], 0, sizeof( __int32 ) * indexMapping3.count );

		glBindTexture( GL_TEXTURE_3D, volumeChannels[i] );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().z(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, volumeChannelsData[i] );

		glBindImageTexture( i, volumeChannels[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI );

		glUniform1i( splatShader.volumeChannels[i], i );
	}

	glBindTexture( GL_TEXTURE_3D, 0 );

	GLuint fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );

	GLuint renderbuffer;

	glGenRenderbuffers( 1, &renderbuffer );
	glBindRenderbuffer( GL_RENDERBUFFER, renderbuffer );
	int maxSize = indexMapping3.getSize().maxCoeff();
	glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA, maxSize, maxSize );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer );
	// disable framebuffer operations
	glDisable( GL_DEPTH_TEST );
	glDrawBuffer( GL_NONE );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	glDepthMask( GL_FALSE );

	// GL_PROJECTION is not needed
	int permutations[3][3] = { {1,2,0}, {2,0,1}, {0,1,2} };
	for( int i = 0 ; i < 3 ; ++i ) {
		int *permutation = permutations[i];
		const Vector3i permutedSize = permute( indexMapping3.getSize(), permutation );
		auto projection = Eigen::createOrthoProjectionMatrixLH( Vector2f::Zero(), permutedSize.head<2>().cast<float>(), 0.0f, (float) permutedSize.z() ); 
		glUniform( splatShader.mainAxisProjection[i], projection );
		glUniform( splatShader.mainAxisPermutation[i], unpermutedToPermutedMatrix( permutation ).topLeftCorner<3,3>().matrix() );

		glViewportIndexedf( i, 0, 0, (float) permutedSize.x(), (float) permutedSize.y() );
	}

	glMatrixMode( GL_MODELVIEW );
	glLoadMatrix( indexMapping3.positionToIndex );

	renderScene();

	MuxerShader muxerShader;
	muxerShader.init();

	muxerShader.apply();

	Color4ub *volumeData;
	GLuint volume;
	glGenTextures( 1, &volume );

	volumeData = new Color4ub[ indexMapping3.count ];
	glBindTexture( GL_TEXTURE_3D, volume );
	glTexStorage3D( GL_TEXTURE_3D, 1, GL_RGBA8, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().z() );

	for( int i = 0 ; i < 4 ; ++i ) {
		glBindImageTexture( i, volumeChannels[i], 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI );
		glUniform1i( muxerShader.volumeChannels[i], i );
	}
	glBindImageTexture( 4, volume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8 );
	glUniform1i( muxerShader.volume, 4 );

	glUniform3i( muxerShader.sizeHelper, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().x() * indexMapping3.getSize().y() );

	glEnableClientState( GL_VERTEX_ARRAY );
	float zero[3] = {0.0, 0.0, 0.0};
	glVertexPointer( 3, GL_FLOAT, 0, &zero );

	glEnable( GL_RASTERIZER_DISCARD );
	glDrawArraysInstanced( GL_POINTS, 0, 1, indexMapping3.count );
	glDisable( GL_RASTERIZER_DISCARD );

	glGetTexImage( GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, volumeData );

	glBindTexture( GL_TEXTURE_3D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glUseProgram( 0 );

	glPopClientAttrib();
	glPopAttrib();

	delete[] volumeChannelsData[0];
	delete[] volumeChannelsData[1];
	delete[] volumeChannelsData[2];
	delete[] volumeChannelsData[3];

	glDeleteTextures( 4, volumeChannels );
	glDeleteTextures( 1, &volume );

	glDeleteFramebuffers( 1, &fbo );
	glDeleteRenderbuffers( 1, &renderbuffer );

	grid = ColorGrid( indexMapping3, volumeData );
}