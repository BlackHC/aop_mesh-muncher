#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <iterator>

#include <gtest.h>
#include <Eigen/Eigen>

#include "debugRender.h"

#include <SFML/Window.hpp>

#include "objModel.h"

#include <memory>

using namespace Eigen;

// common chunk struct
namespace Serialize {
	template<typename T>
	void writeTyped( FILE *fileHandle, const T &d ) {
		fwrite( &d, sizeof(T), 1, fileHandle );
	}

	template<typename T>
	void readTyped( FILE *fileHandle, T &d ) {
		fread( &d, sizeof(T), 1, fileHandle );
	}

	template<typename V>
	void writeTyped( FILE *fileHandle, const std::vector<V> &d ) {
		int num = (int) d.size();
		writeTyped( fileHandle, num );
		for( int i = 0 ; i < num ; ++i ) {
			writeTyped( fileHandle, d[i] );
		}
	}

	template<typename V>
	void readTyped( FILE *fileHandle, std::vector<V> &d ) {
		int num;
		readTyped( fileHandle, num );
		d.resize( num );

		for( int i = 0 ; i < num ; ++i ) {
			readTyped( fileHandle, d[i] );
		}
	}
}

namespace Storage {
struct RawChunk {
	char name[16];
	size_t headerSize;
	size_t rawDataSize;
};

struct RawFile {
	char magic[6]; // CHUNKS
	char intSize;
	int numChunks;
};

namespace Input {
template<typename HeaderStruct, typename DataType>
struct Chunk {
	const HeaderStruct *header;
	const DataType *data;

	bool load( const RawChunk *chunk ) {
		if( chunk->headerSize != sizeof(HeaderStruct) || chunk->rawDataSize % sizeof(DataType) != 0 ) {
			return false;
		}

		header = (const HeaderStruct *) (((const char*) chunk) + sizeof( RawChunk ));
		data = (const DataType *) (((const char*) header) + chunk->headerSize);

		return true;
	}
};

struct File {
	const RawFile *rawFile;
	const RawChunk *first;

	bool load( const RawFile *file ) {
		if( memcmp( file->magic, "CHUNKS", 6 ) != 0 || file->intSize != sizeof( int ) ) {
			return false;
		}

		rawFile = file;
		first = reinterpret_cast<const RawChunk*>( reinterpret_cast<const char*>( file ) + sizeof( RawFile ) );		
	}

	class Iterator {
		int numChunks;
		const RawChunk *current;
		int currentIndex;

		Iterator( const RawChunk *first, int numChunks ) : numChunks( numChunks ), current( first ), currentIndex( 0 ) {}

		friend class File;
	public:
		operator bool() {
			return currentIndex < numChunks;
		}

		const RawChunk * operator ->() {
			return current;
		}

		const RawChunk & operator *() {
			return *current;
		} 

		Iterator & operator ++() {
			if( ++currentIndex >= numChunks ) {
				current = nullptr;
				return *this;
			}
			current = reinterpret_cast<const RawChunk *>( reinterpret_cast<const char*>( current ) + current->headerSize + current->rawDataSize );
		}
	};

	Iterator getIterator() const {
		return Iterator( first, rawFile->numChunks );
	}
};

RawFile *readFile( const char *filename ) {
	FILE *handle = fopen( filename, "rb" );
	if( !handle ) {
		return nullptr;
	}

	fseek( handle, 0, SEEK_END );
	size_t size = ftell( handle );
	fseek( handle, 0, SEEK_SET );

	char *data = new char[size];
	if( fread( data, size, 1, handle ) != 1 ) {
		delete[] data;
		return nullptr;
	}

	return reinterpret_cast<RawFile*>( data );
}
}

namespace Output {
	struct File {
		FILE *handle;

		File() : handle( nullptr ) {}
		~File() {
			if( handle ) {
				fclose( handle );
			}
		}

		bool open( const char *filename, int numChunks ) {
			handle = fopen( filename, "wb" );
			if( !handle ) {
				return false;
			}

			RawFile rawFile;
			memcpy( rawFile.magic, "CHUNKS", 6 );
			rawFile.intSize = sizeof( int );
			rawFile.numChunks = numChunks;

			Serialize::writeTyped( handle, rawFile );
		}

		template<typename HeaderStruct, typename DataType>
		void pushChunk( const char *name, const HeaderStruct &headerStruct, int dataCount, const DataType *data ) {
			RawChunk rawChunk;
			strncpy( rawChunk.name, name );
			rawChunk.headerSize = sizeof( HeaderStruct);
			rawChunk.rawDataSize = dataCount * sizeof( data );

			Serialize::writeTyped(rawChunk);
			Serialize::writeTyped(headerStruct);
			fwrite( data, sizeof( DataType ), dataCount, handle );
		}

		void close() {
			fclose( handle );
			handle = nullptr;
		}
	};
}
}

struct Grid {
	Vector3i size;
	int count;

	Vector3f offset;
	float resolution;

	Grid( const Vector3i &size, const Vector3f &offset, float resolution ) : size( size ), offset( offset ), resolution( resolution ) {
		count = size[0] * size[1] * size[2];
	}

	int getIndex( const Vector3i &index3 ) {
		return index3[0] + size[0] * index3[1] + (size[0] * size[1]) * index3[2];
	}

	Vector3i getIndex3( int index ) {
		int x = index % size[0];
		index /= size[0];
		int y = index % size[1];
		index /= size[1];
		int z = index;
		return Vector3i( x,y,z );
	}

	Vector3f getPosition( const Vector3i &index3 ) {
		return offset + index3.cast<float>() * resolution;
	}
};

const Vector3i indexToCubeCorner[] = {
	Vector3i( 0,0,0 ), Vector3i( 1,0,0 ), Vector3i( 0,1,0 ), Vector3i( 0,0,1 ),
	Vector3i( 1,1,0 ), Vector3i( 0,1,1 ), Vector3i( 1,0,1 ), Vector3i( 1,1,1 )
};

inline float squaredMinDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distance;
	for( int i = 0 ; i < 3 ; i++ ) {
		if( point[i] > max[i] ) {
			distance[i] = point[i] - max[i];
		}
		else if( point[i] > min[i] ) {
			distance[i] = 0.f;
		}
		else {
			distance[i] = min[i] - point[i];
		}
	}
	return distance.squaredNorm();
}

inline float squaredMaxDistanceAABoxPoint( const Vector3f &min, const Vector3f &max, const Vector3f &point ) {
	Vector3f distanceA = (point - min).cwiseAbs();
	Vector3f distanceB = (point - max).cwiseAbs();
	Vector3f distance = distanceA.cwiseMax( distanceB );

	return distance.squaredNorm();
}

#include "camera.h"
#include "cameraInputControl.h"

using namespace niven;

struct null_deleter {
	template<typename T>
	void operator() (T*) {}
};

template<typename T>
std::shared_ptr<T> shared_from_stack(T &object) {
	return std::shared_ptr<T>( &object, null_deleter() );
}

void main() {
	sf::Window window( sf::VideoMode( 640, 480 ), "Position Solver", sf::Style::Default, sf::ContextSettings(32) );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 1.0;
	camera.perspectiveProjectionParameters.zFar = 500.0;
	//camera.position.z() = 5;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( shared_from_stack(camera), shared_from_stack(window) );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	ObjSceneGL objScene;
	//objScene.Init( IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );
	objScene.Init( IO::Path( "P:\\BlenderScenes\\lawn_garage_house.obj" ) );

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;
	while (window.isOpen())
	{
		// Event processing
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Request for closing the window
			if (event.type == sf::Event::Closed)
				window.close();

			if( event.type == sf::Event::Resized ) {
				camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
				glViewport( 0, 0, event.size.width, event.size.height );
			}

			cameraInputControl.handleEvent( event );
		}

		cameraInputControl.update( frameClock.restart().asSeconds(), false );

		// Activate the window for OpenGL rendering
		window.setActive();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// OpenGL drawing commands go here...
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( camera.getProjectionMatrix() );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( camera.getViewTransformation().matrix() );

		objScene.Draw();

		// End the current frame and display its contents on screen
		window.display();
	}

};