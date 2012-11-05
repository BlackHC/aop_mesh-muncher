#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <boost/timer/timer.hpp>
#include <iterator>

#include <Eigen/Eigen>
#include <GL/glew.h>
#include <unsupported/Eigen/OpenGLSupport>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Debug.h"

#include <memory>

using namespace Eigen;

#include "camera.h"
#include "cameraInputControl.h"
#include <verboseEventHandlers.h>

#include "make_nonallocated_shared.h"

#include "sgsSceneRenderer.h"
#include "optixRenderer.h"

#include "debugWindows.h"

#include "boost/format.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/algorithm/string.hpp>

// allows us to modify a scene at runtime
struct SGSSceneRuntime {
	SGSScene *scene;

	static const int NO_MODEL = -1;

	SGSSceneRuntime( SGSScene *scene ) : scene( scene ) {}

	int findTexture( const std::string &name ) {
		for( int textureIndex = 0 ; textureIndex < scene->textures.size() ; textureIndex++ ) {
			if( scene->textures[ textureIndex ].name == name ) {
				return textureIndex;
			}
		}
		return SGSScene::NO_TEXTURE;
	}

	int findModel( const std::string &name ) {
		for( int modelIndex = 0 ; modelIndex < scene->models.size() ; modelIndex++ ) {
			if( scene->modelNames[ modelIndex ] == name ) {
				return modelIndex;
			}
		}

		return NO_MODEL;
	}

	int importModel( const SGSScene &otherScene, const int otherModelIndex ) {
		const auto &modelName = otherScene.modelNames[ otherModelIndex ];
		// check whether the object already exists
		{
			const int modelIndex = findModel( modelName );
			if( modelIndex != NO_MODEL ) {
				return modelIndex;
			}
		}

		const auto &otherModel = otherScene.models[ otherModelIndex ];
		auto model = createModel( modelName );

		// copy the bounding box and sphere
		model.bounding = otherModel.bounding;
		// copy the sub models
		const int numSubModels = otherModel.numSubObjects;
		for( int subModelIndex = 0 ; subModelIndex < numSubModels ; subModelIndex++ ) {
			const auto &otherSubModel = otherScene.subObjects[ otherModel.startSubObject + subModelIndex ];
			auto subModel = createSubObject(
				otherSubModel.subModelName,
				allocateVertices( otherSubModel.numVertices ),
				allocateIndices( otherSubModel.numIndices ),
				otherSubModel.material
			);
			// copy the texture if needed
			{
				const int otherTextureIndex = otherSubModel.material.textureIndex[0];
				if( otherTextureIndex != SGSScene::NO_TEXTURE ) {
					const auto &otherTexture = otherScene.textures[ otherTextureIndex ];

					int textureIndex = findTexture( otherTexture.name );
					if( textureIndex == SGSScene::NO_TEXTURE ) {
						textureIndex = pushTexture( otherTexture );
					}

					subModel.material.textureIndex[0] = textureIndex;
				}
			}
			// copy the bounding box and sphere
			subModel.bounding = otherSubModel.bounding;

			// copy the vertex data
			{
				const auto beginVertex = otherScene.vertices.begin() + otherSubModel.startVertex;
				const auto endVertex = beginVertex + otherSubModel.numVertices;
				pushVertices( subModel, beginVertex, endVertex );
			}
			// copy the index data
			{
				const auto beginIndex = otherScene.indices.begin() + otherSubModel.startIndex;
				const auto endIndex = beginIndex + otherSubModel.numIndices;
				const int offset = subModel.startVertex - otherSubModel.startVertex;
				pushIndices( offset, subModel, beginIndex, endIndex );
			}

			pushSubObject( model, subModel );
		}

		return pushModel( model );
	}

	static SGSScene::Material createMaterial( unsigned char r, unsigned char g, unsigned char b ) {
		SGSScene::Material material;
		material.diffuse.r = r;
		material.diffuse.g = g;
		material.diffuse.b = b;

		material.ambient = material.specular = material.diffuse;

		material.specularSharpness = 15.0;

		material.doubleSided = true;
		material.wireFrame = false;

		material.alpha = 255;
		material.alphaType = SGSScene::Material::AT_MATERIAL;

		material.textureIndex[0] = SGSScene::NO_TEXTURE;

		return material;
	}

	static SGSScene::Material createMaterial( const Eigen::Vector3f &rgb ) {
		return createMaterial( rgb[0] * 255, rgb[1] * 255, rgb[2] * 255 );
	}

	int allocateVertices( int numVertices ) {
		const int firstVertex = scene->vertices.size();
		scene->vertices.reserve( firstVertex + numVertices );
		return firstVertex;
	}

	int allocateIndices( int numIndices ) {
		const int firstIndex = scene->indices.size();
		scene->indices.reserve( firstIndex + numIndices );
		return firstIndex;
	}

	SGSScene::Vertex createPrimitiveVertex( const Eigen::Vector3f &position, const Eigen::Vector3f &normal ) {
		SGSScene::Vertex vertex;
		Eigen::Vector3f::Map( vertex.position ) = position;
		Eigen::Vector3f::Map( vertex.normal ) = normal;
		vertex.uv[0][0] = vertex.uv[0][1] = 0.0f;
		return vertex;
	}

	SGSScene::Vertex createPrimitiveVertex( const float *position, const float *normal ) {
		SGSScene::Vertex vertex;
		Eigen::Vector3f::Map( vertex.position ) = Eigen::Vector3f::Map( position );
		Eigen::Vector3f::Map( vertex.normal ) = Eigen::Vector3f::Map( normal );
		vertex.uv[0][0] = vertex.uv[0][1] = 0.0f;
		return vertex;
	}

	SGSScene::SubObject createSubObject( const std::string &name, int firstVertex, int firstIndex, const SGSScene::Material &material ) {
		SGSScene::SubObject subObject;

		subObject.subModelName = name;

		subObject.material = material;

		subObject.startVertex = firstVertex;
		subObject.startIndex = firstIndex;

		subObject.numVertices = 0;
		subObject.numIndices = 0;

		return subObject;
	}

	SGSScene::Model createModel( const std::string &name ) {
		SGSScene::Model model;

		model.startSubObject = scene->subObjects.size();
		model.numSubObjects = 0;

		scene->modelNames.push_back( name );

		return model;
	}

	void calculateBoundingBox( SGSScene::SubObject &subObject ) {
		Eigen::AlignedBox3f bbox;

		for( int i = 0 ; i < subObject.numVertices ; i++ ) {
			bbox.extend( Eigen::Vector3f::Map( scene->vertices[ subObject.startVertex + i ].position ) );
		}

		Eigen::Vector3f::Map( subObject.bounding.box.min ) = bbox.min();
		Eigen::Vector3f::Map( subObject.bounding.box.max ) = bbox.max();

		Eigen::Vector3f::Map( subObject.bounding.sphere.center ) = bbox.center();
		subObject.bounding.sphere.radius = bbox.sizes().norm() * 0.5;
	}

	void calculateBoundingBox( SGSScene::Model &model ) {
		Eigen::AlignedBox3f bbox;

		for( int i = 0 ; i < model.numSubObjects ; i++ ) {
			const auto &subObject = scene->subObjects[ model.startSubObject + i ];

			bbox.extend( Eigen::Vector3f::Map( subObject.bounding.box.min ) );
			bbox.extend( Eigen::Vector3f::Map( subObject.bounding.box.max ) );
		}

		Eigen::Vector3f::Map( model.bounding.box.min ) = bbox.min();
		Eigen::Vector3f::Map( model.bounding.box.max ) = bbox.max();

		Eigen::Vector3f::Map( model.bounding.sphere.center ) = bbox.center();
		model.bounding.sphere.radius = bbox.sizes().norm() * 0.5;
	}

	void pushSingleTriangleVertex( SGSScene::SubObject &subObject, const SGSScene::Vertex &vertex ) {
		scene->indices.push_back( scene->indices.size() );
		scene->vertices.push_back( vertex );

		subObject.numIndices++;
		subObject.numVertices++;
	}

	int pushVertex( SGSScene::SubObject &subObject, const SGSScene::Vertex &vertex ) {
		scene->vertices.push_back( vertex );

		return subObject.numVertices++;
	}

	template< typename RandomIterator >
	int pushVertices( SGSScene::SubObject &subObject, RandomIterator begin, RandomIterator end ) {
		std::copy( begin, end, std::back_inserter( scene->vertices ) );

		return subObject.numVertices += end - begin;
	}

	void pushIndex( SGSScene::SubObject &subObject, int index ) {
		scene->indices.push_back( index );
		subObject.numIndices++;
	}

	template< typename RandomIterator >
	void pushIndices( int offset, SGSScene::SubObject &subObject, RandomIterator begin, RandomIterator end ) {
		std::transform( begin, end, std::back_inserter( scene->indices ), [offset] (const int index) { return index + offset; } );
		subObject.numIndices += end - begin;
	}

	void pushQuad(
		SGSScene::SubObject &subObject,
		const SGSScene::Vertex &vertexA,
		const SGSScene::Vertex &vertexB,
		const SGSScene::Vertex &vertexC,
		const SGSScene::Vertex &vertexD
	) {
		const int firstVertex = scene->vertices.size();

		pushVertex( subObject, vertexA );
		pushVertex( subObject, vertexB );
		pushVertex( subObject, vertexC );
		pushVertex( subObject, vertexD );

		pushIndex( subObject, firstVertex );
		pushIndex( subObject, firstVertex + 1 );
		pushIndex( subObject, firstVertex + 2 );
		pushIndex( subObject, firstVertex + 2 );
		pushIndex( subObject, firstVertex + 3 );
		pushIndex( subObject, firstVertex );
	}

	void pushSubObject( SGSScene::Model &model, const SGSScene::SubObject &subObject ) {
		scene->subObjects.push_back( subObject );
		model.numSubObjects++;
	}

	int pushModel( const SGSScene::Model &model ) {
		const int modelIndex = scene->models.size();
		scene->models.push_back( model );
		return modelIndex;
	}

	int pushTexture( const SGSScene::Texture &texture ) {
		const int textureIndex = scene->textures.size();
		scene->textures.push_back( texture );
		return textureIndex;
	}

	static std::string getSimpleMaterialName( const SGSScene::Material &material ) {
		return boost::str( boost::format( "%i %i %i" ) % int( material.diffuse.r ) % int( material.diffuse.g ) % int( material.diffuse.b ) );
	}

	int addBoxModel( const Eigen::Vector3f &size, const SGSScene::Material &material ) {
		const auto name = boost::str( boost::format( "Box %f %f %f %s" ) % size.x() % size.y() % size.z() % getSimpleMaterialName( material ) );
		{
			int modelIndex = findModel( name );
			if( modelIndex != NO_MODEL ) {
				return modelIndex;
			}
		}
		auto model = createModel( name );

		static GLfloat n[6][3] =
		{
			{-1.0, 0.0, 0.0},
			{0.0, 1.0, 0.0},
			{1.0, 0.0, 0.0},
			{0.0, -1.0, 0.0},
			{0.0, 0.0, 1.0},
			{0.0, 0.0, -1.0}
		};
		static GLint faces[6][4] =
		{
			{0, 1, 2, 3},
			{3, 2, 6, 7},
			{7, 6, 5, 4},
			{4, 5, 1, 0},
			{5, 6, 2, 1},
			{7, 4, 0, 3}
		};
		GLfloat v[8][3];
		GLint i;

		v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size.x() / 2;
		v[4][0] = v[5][0] = v[6][0] = v[7][0] = size.x() / 2;
		v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size.y() / 2;
		v[2][1] = v[3][1] = v[6][1] = v[7][1] = size.y() / 2;
		v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size.z() / 2;
		v[1][2] = v[2][2] = v[5][2] = v[6][2] = size.z() / 2;

		auto subObject = createSubObject(
			"Faces",
			allocateVertices( 4*6 ),
			allocateIndices( 2*3*6 ),
			material
		);

		for (i = 0; i < 6; ++i ) {
			const int firstVertex = subObject.startVertex;

			pushQuad(
				subObject,
				createPrimitiveVertex( &v[faces[i][0]][0], &n[i][0] ),
				createPrimitiveVertex( &v[faces[i][1]][0], &n[i][0] ),
				createPrimitiveVertex( &v[faces[i][2]][0], &n[i][0] ),
				createPrimitiveVertex( &v[faces[i][3]][0], &n[i][0] )
			);
		}

		calculateBoundingBox( subObject );

		pushSubObject( model, subObject );

		calculateBoundingBox( model );

		return pushModel( model );
	}

	int addSphereModel( float radius, const SGSScene::Material &material, int u = 10, int v = 20 ) {
		const auto name = boost::str( boost::format( "Sphere %f %i %i %s" ) % radius % u % v % getSimpleMaterialName( material ) );
		{
			const int modelIndex = findModel( name );
			if( modelIndex != NO_MODEL ) {
				return modelIndex;
			}
		}

		auto model = createModel( name );
		auto subObject = createSubObject(
			"Faces",
			allocateVertices( u*v*4 ),
			allocateIndices( u*v*2*3 ),
			material
		);

		for( int i = 0 ; i < u ; i++ ) {
			for( int j = 0 ; j < v ; j++ ) {
				// cos | sin | cos
				// cos | 1   | sin
#define U(x) float((x)*M_PI/u - M_PI / 2)
#define V(x) float((x)*2*M_PI/v)
#define P(u_r,v_r) Eigen::Vector3f( cos(U(u_r)) * cos(V(v_r)), sin(U(u_r)), cos(U(u_r)) * sin(V(v_r)) )

				pushQuad(
					subObject,
					createPrimitiveVertex( radius * P(i,j), P(i,j) ),
					createPrimitiveVertex( radius * P(i + 1,j), P(i + 1,j) ),
					createPrimitiveVertex( radius * P(i + 1,j + 1), P(i + 1,j + 1) ),
					createPrimitiveVertex( radius * P(i,j + 1), P(i,j + 1) )
				);
#undef U
#undef V
#undef P
			}
		}

		calculateBoundingBox( subObject );
		pushSubObject( model, subObject );

		calculateBoundingBox( model );
		return pushModel( model );
	}

	void addInstance( int modelIndex, const Eigen::Affine3f &transformation ) {
		SGSScene::Object object;
		object.modelId = modelIndex;
		Eigen::Matrix4f::Map( object.transformation ) = transformation.matrix();
		scene->objects.push_back( object );
	}
};

DebugRender::CombinedCalls selectionDR;

void selectObjectsByModelID( SGSSceneRenderer &renderer, int modelIndex ) {
	SGSSceneRenderer::InstanceIndices indices = renderer.getModelInstances( modelIndex );

	selectionDR.begin();
	selectionDR.setColor( Eigen::Vector3f::UnitX() );

	for( auto instanceIndex = indices.begin() ; instanceIndex != indices.end() ; ++instanceIndex ) {
		auto transformation = renderer.getInstanceTransformation( *instanceIndex );
		auto boundingBox = renderer.getUntransformedInstanceBoundingBox( *instanceIndex );

		selectionDR.setTransformation( transformation.matrix() );
		selectionDR.drawAABB( boundingBox.min(), boundingBox.max() );
	}

	selectionDR.end();
}

#if 1
struct SceneDeclaration {
	struct BoxDeclaration {
		float size[3];
		float color[3];

		BoxDeclaration() {}

		BoxDeclaration( const Vector3f &size, const Vector3f &color ) {
			Vector3f::Map( this->size ) = size;
			Vector3f::Map( this->color ) = color;
		}

		SERIALIZER_DEFAULT_IMPL( (size)(color) )
	};
	struct SphereDeclaration {
		float radius;
		float color[3];
		int u, v;

		SphereDeclaration() : u( 10 ), v( 20 ) {}

		SphereDeclaration( float radius, const Vector3f &color, int u = 10, int v = 20 ) {
			this->radius = radius;
			Vector3f::Map( this->color ) = color;

			this->u = u;
			this->v = v;
		}

		SERIALIZER_DEFAULT_IMPL( (radius)(color)(u)(v) )
	};

	struct ModelImports {
		std::string scene;

		std::vector< std::string > modelNames;

		ModelImports() {}
		ModelImports( const std::string &scene ) : scene( scene ) {}

		SERIALIZER_FIRST_KEY_IMPL( (scene)(modelNames) );
	};

	struct Instance {
		std::string modelName;

		float position[3];
		float axis[3];
		float degrees;

		Instance() : position(), axis(), degrees() {}

		SERIALIZER_FIRST_KEY_IMPL( (modelName)(position)(axis)(degrees) );
	};

	std::vector<BoxDeclaration> boxes;
	std::vector<SphereDeclaration> spheres;
	std::vector<ModelImports> modelImports;
	std::vector<Instance> instances;

	SERIALIZER_DEFAULT_IMPL( (boxes)(spheres)(modelImports)(instances) )
};

SceneDeclaration readSceneDeclaration( const char *filename ) {
	Serializer::TextReader reader( filename );

	SceneDeclaration sceneDeclaration;
	Serializer::read( reader, sceneDeclaration );

	return sceneDeclaration;
}

void writeSceneDeclaration( const char *filename, const SceneDeclaration &sceneDeclaration ) {
	Serializer::TextWriter writer( filename );

	Serializer::write( writer, sceneDeclaration );
}

using namespace boost;
namespace po = boost::program_options;
using namespace std;

void real_main( int argc, const char **argv ) {
	//vector< string > args = po::split_winmain( "exampleSceneDecls.wml testScene.sgsScene" );
	vector< string > args;
	for( int i = 1 ; i < argc ; i++ ) {
		args.push_back( argv[ i ] );
	}

	string sceneDeclFilename, targetFilename, importSceneFilename;
	bool echoDecl = false;

	po::options_description desc( "Program options" );
	desc.add_options()
		( "help,?", "produce this help message" )
		( "sceneDecls", po::value< std::string >( &sceneDeclFilename )->default_value( "sceneDecls.wml" ), "model declaration file to use" )
		( "target", po::value< std::string >( &targetFilename ), "target file" )
		( "createExample", "dump an example model declaration" )
		( "createSceneDecls", po::value< std::string >( &importSceneFilename ), "create a identity decl file for the scene")
		( "echo", po::bool_switch( &echoDecl ), "echo the decl on the console after reading it" )
	;

	po::positional_options_description p;
	p.add( "sceneDecls", 1 );
	p.add( "target", 1 );

	po::variables_map vm;
	po::store( po::command_line_parser( args ).options(desc).positional(p).run(), vm );
	po::notify(vm);

	if( vm.count( "help" ) || argc == 1 ) {
		std::cout << desc;
		return;
	}
	else if( vm.count( "createExample" ) ) {
		SceneDeclaration sceneDeclaration;

		sceneDeclaration.boxes.push_back( SceneDeclaration::BoxDeclaration( Eigen::Vector3f::Constant( 5.0 ), Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );
		sceneDeclaration.boxes.push_back( SceneDeclaration::BoxDeclaration( Eigen::Vector3f::Constant( 10.0 ), Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );

		sceneDeclaration.spheres.push_back( SceneDeclaration::SphereDeclaration( 5.0, Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );
		sceneDeclaration.spheres.push_back( SceneDeclaration::SphereDeclaration( 10.0, Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );

		writeSceneDeclaration( "exampleSceneDecls.wml", sceneDeclaration );
		cout << "Successfully wrote 'exampleSceneDecls.wml'!\n";
		return;
	}
	else if( vm.count( "createSceneDecls" ) ) {
		if( targetFilename.empty() ) {
			targetFilename = algorithm::erase_last_copy( importSceneFilename, ".sgsScene" ) + ".wml";
		}

		SGSScene sourceScene;
		{
			Serializer::BinaryReader reader( importSceneFilename.c_str() );
			Serializer::read( reader, sourceScene );
		}

		SceneDeclaration sceneDeclaration;
		sceneDeclaration.modelImports.push_back( SceneDeclaration::ModelImports( importSceneFilename ) );
		auto &modelImport = sceneDeclaration.modelImports.front();

		for( int modelIndex = 0 ; modelIndex < sourceScene.models.size() ; modelIndex++ ) {
			modelImport.modelNames.push_back( sourceScene.modelNames[ modelIndex ] );
		}

		for( int instanceIndex = 0 ; instanceIndex < sourceScene.objects.size() ; instanceIndex++ ) {
			const auto &instance = sourceScene.objects[ instanceIndex ];

			const auto &modelName = sourceScene.modelNames[ instance.modelId ];
			const auto transformation = Eigen::Affine3f( Eigen::Matrix4f::Map( sourceScene.objects[ instanceIndex ].transformation ) );
			const Vector3f translation = transformation.translation();
			const AngleAxisf angleAxis( transformation.linear() );

			SceneDeclaration::Instance instanceDecl;
			instanceDecl.modelName = modelName;
			for( int i = 0 ; i < 3 ; i++ ) {
				instanceDecl.position[i] = translation[i];	
			}
			
			const auto &axis = angleAxis.axis();
			for( int i = 0 ; i < 3 ; i++ ) {
				instanceDecl.axis[i] = axis[i];
			}
			instanceDecl.degrees = angleAxis.angle() / M_PI * 180.0f;

			sceneDeclaration.instances.push_back( instanceDecl );
		}

		writeSceneDeclaration( targetFilename.c_str(), sceneDeclaration );
		cout << "Successfully wrote '" << targetFilename << "'!\n";
		return;
	}

	if( targetFilename.empty() ) {
		targetFilename = algorithm::erase_last_copy( sceneDeclFilename, ".wml" ) + ".sgsScene";
	}

	// read the decls
	SceneDeclaration sceneDeclaration;
	{
		Serializer::TextReader reader( sceneDeclFilename.c_str() );
		if( echoDecl ) {
			cout << "Decls:\n" << wml::emit( reader.root );
		}

		Serializer::read( reader, sceneDeclaration );
	}

	{
		cout << "creating target scene..\n";
		SGSScene targetScene;

		cout << "creating primitive models..\n";
		SGSSceneRuntime runtime( &targetScene );
		for (auto box = sceneDeclaration.boxes.begin() ; box != sceneDeclaration.boxes.end() ; ++box ) {
			runtime.addBoxModel( Vector3f::Map( box->size ), runtime.createMaterial( Vector3f::Map( box->color ) ) );
		}

		for (auto sphere = sceneDeclaration.spheres.begin() ; sphere != sceneDeclaration.spheres.end() ; ++sphere ) {
			runtime.addSphereModel( sphere->radius, runtime.createMaterial( Vector3f::Map( sphere->color ) ), sphere->u, sphere->v );
		}

		cout << "importing models..\n";
		for( auto modelImports = sceneDeclaration.modelImports.begin() ; modelImports != sceneDeclaration.modelImports.end() ; ++modelImports ) {
			cout << "\timporting from scene '" << modelImports->scene << "'\n";

			SGSScene sourceScene;
			{
				Serializer::BinaryReader reader( modelImports->scene.c_str() );
				Serializer::read( reader, sourceScene );
			}

			SGSSceneRuntime sourceRuntime( &sourceScene );

			for( auto modelName = modelImports->modelNames.begin() ; modelName != modelImports->modelNames.end() ; ++modelName ) {
				const int modelIndex = sourceRuntime.findModel( *modelName );
				if( modelIndex != SGSSceneRuntime::NO_MODEL ) {
					cout << "\t\t" << *modelName << "\n";
					runtime.importModel( sourceScene, modelIndex );
				}
				else {
					cout << "\t! " << *modelName << " not found!\n";
				}
			}
		}

		cout << "creating instances..\n";
		for( auto instance = sceneDeclaration.instances.begin() ; instance != sceneDeclaration.instances.end() ; ++instance ) {
			const int modelIndex = runtime.findModel( instance->modelName );
			if( modelIndex == SGSSceneRuntime::NO_MODEL ) {
				cout << "\t! " << instance->modelName << " not found!\n";
				continue;
			}

			const float radians = instance->degrees / 180.0 * M_PI;
			const AngleAxisf angleAxis( radians, Vector3f::Map( instance->axis ) );
			const Affine3f transformation = Translation3f( Vector3f::Map( instance->position ) ) * angleAxis;

			runtime.addInstance( modelIndex, transformation );
		}

		cout << "writing target scene '" << targetFilename << "'\n";
		{
			Serializer::BinaryWriter writer( targetFilename.c_str() );

			Serializer::write( writer, targetScene );
		}

		cout << "Target scene successfully written!\n";
	}
}
#else
void real_main( int argc, const char **argv ) {
	sf::RenderWindow window( sf::VideoMode( 640, 480 ), "sgsSceneCreator", sf::Style::Default, sf::ContextSettings(24, 8, 0, 4, 2, false,true, false) );
	glewInit();

	glutil::RegisterDebugOutput( glutil::STD_OUT );

	Camera camera;
	camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
	camera.perspectiveProjectionParameters.FoV_y = 75.0;
	camera.perspectiveProjectionParameters.zNear = 0.05;
	camera.perspectiveProjectionParameters.zFar = 500.0;

	CameraInputControl cameraInputControl;
	cameraInputControl.init( make_nonallocated_shared(camera) );

	// Activate the window for OpenGL rendering
	window.setActive();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	// The main loop - ends as soon as the window is closed
	sf::Clock frameClock, clock;

	SGSSceneRenderer sgsSceneRenderer;
	OptixRenderer optixRenderer;
	SGSScene sgsScene;
	RenderContext renderContext;
	renderContext.setDefault();

	{
		boost::timer::auto_cpu_timer timer( "SGSSceneRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		sgsSceneRenderer.reloadShaders();

		/*const char *scenePath = "P:\\sgs\\sg_and_sgs_source\\survivor\\__GameData\\Editor\\Save\\Survivor_original_mission_editorfiles\\test\\scene.glscene";
		{
			Serializer::BinaryReader reader( scenePath );
			Serializer::read( reader, sgsScene );
		}*/

		SGSSceneRuntime runtime;
		runtime.scene = &sgsScene;

		Eigen::Affine3f transformation;
		transformation.setIdentity();

		const int boxIndex = runtime.addBoxModel( Eigen::Vector3f::Constant( 5.0 ), runtime.createMaterial( 255, 128, 128 ) );
		const int sphereIndex = runtime.addSphereModel( 5.0, runtime.createMaterial( 128, 128, 255 ) );

#if 1
		runtime.addInstance(
			boxIndex,
			transformation
		);

		runtime.addInstance(
			sphereIndex,
			Eigen::Affine3f( Eigen::Translation3f( Eigen::Vector3f::Constant( -10.0 ) ) )
		);
#endif

		{
			Serializer::BinaryWriter writer( "testScene.sgsScene" );
			Serializer::write( writer, sgsScene );
			return;
		}

		const char *cachePath = "scene.sgsRendererCache";
		sgsSceneRenderer.processScene( make_nonallocated_shared( sgsScene ), cachePath );
	}
	{
		boost::timer::auto_cpu_timer timer( "OptixRenderer: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		optixRenderer.init( make_nonallocated_shared( sgsSceneRenderer ) );
	}

	EventSystem eventSystem;
	EventDispatcher eventDispatcher;
	eventSystem.setRootHandler( make_nonallocated_shared( eventDispatcher ) );
	eventSystem.exclusiveMode.window = make_nonallocated_shared( window );
	eventDispatcher.addEventHandler( make_nonallocated_shared( cameraInputControl ) );

	EventDispatcher verboseEventDispatcher;
	eventDispatcher.addEventHandler( make_nonallocated_shared( verboseEventDispatcher ) );

	registerConsoleHelpAction( verboseEventDispatcher );

	KeyAction reloadShadersAction( "reload shaders", sf::Keyboard::R, [&] () { sgsSceneRenderer.reloadShaders(); } );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( reloadShadersAction ) );

	BoolVariableToggle showBoundingSpheresToggle( "show bounding spheres",sgsSceneRenderer.debug.showBoundingSpheres, sf::Keyboard::B );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showBoundingSpheresToggle ) );

	BoolVariableToggle showTerrainBoundingSpheresToggle( "show terrain bounding spheres",sgsSceneRenderer.debug.showTerrainBoundingSpheres, sf::Keyboard::N );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( showTerrainBoundingSpheresToggle ) );

	BoolVariableToggle updateRenderListsToggle( "updateRenderLists",sgsSceneRenderer.debug.updateRenderLists, sf::Keyboard::C );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( updateRenderListsToggle ) );

	IntVariableControl disabledObjectIndexControl( "disabledModelIndex", renderContext.disabledModelIndex, -1, sgsScene.modelNames.size(), sf::Keyboard::Numpad7, sf::Keyboard::Numpad1 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledObjectIndexControl ) );

	IntVariableControl disabledInstanceIndexControl( "disabledInstanceIndex",renderContext.disabledInstanceIndex, -1, sgsScene.objects.size(), sf::Keyboard::Numpad9, sf::Keyboard::Numpad3 );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disabledInstanceIndexControl ) );


	DebugRender::CombinedCalls probeDumps;
	KeyAction dumpProbeAction( "dump probe", sf::Keyboard::P, [&] () {
		// dump a probe at the current position and view direction
		const Eigen::Vector3f position = camera.getPosition();
		const Eigen::Vector3f direction = camera.getDirection();

		std::vector< OptixRenderer::Probe > probes;
		std::vector< OptixRenderer::ProbeSample > probeSamples;

		OptixRenderer::Probe probe;
		Eigen::Vector3f::Map( &probe.position.x ) = position;
		Eigen::Vector3f::Map( &probe.direction.x ) = direction;

		probes.push_back( probe );

		optixRenderer.sampleProbes( probes, probeSamples, renderSample );

		probeDumps.append();
		probeDumps.setPosition( position );
		//glColor4ubv( &probeSamples.front().color.x );
		probeDumps.drawVectorCone( probeSamples.front().distance * direction, probeSamples.front().distance * 0.25, 1 + probeSamples.front().occlusion );
		probeDumps.end();
	} );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( dumpProbeAction ) );

	KeyAction disableObjectAction( "disable models shot", sf::Keyboard::Numpad4, [&] () {
		// dump a probe at the current position and view direction
		const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };

		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( 0.0f ) );

		OptixRenderer::SelectionResults selectionResults;
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, viewerContext, renderContext );

		renderContext.disabledModelIndex = selectionResults.front().modelIndex;
		std::cout << "object: " << selectionResults.front().modelIndex << "\n";
	} );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disableObjectAction ) );

	KeyAction disableInstanceAction( "disable instance shot", sf::Keyboard::Numpad6, [&] () {
		// dump a probe at the current position and view direction
		const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };

		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( 0.0f ) );

		OptixRenderer::SelectionResults selectionResults;
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, viewerContext, renderContext );

		renderContext.disabledInstanceIndex = selectionResults.front().objectIndex;
		std::cout << "instance: " << selectionResults.front().objectIndex << "\n";
	} );
	verboseEventDispatcher.addEventHandler( make_nonallocated_shared( disableInstanceAction ) );

	DebugWindowManager debugWindowManager;

#if 0
	TextureVisualizationWindow optixWindow;
	optixWindow.init( "Optix Version" );
	optixWindow.texture = optixRenderer.debugTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( optixWindow ) );
#endif
#if 0
	TextureVisualizationWindow mergedTextureWindow;
	mergedTextureWindow.init( "merged object textures" );
	mergedTextureWindow.texture = sgsSceneRenderer.mergedTexture;

	debugWindowManager.windows.push_back( make_nonallocated_shared( mergedTextureWindow ) );
#endif

	sf::Text renderDuration;
	renderDuration.setPosition( 0, 0 );
	renderDuration.setCharacterSize( 10 );

	/*Instance testInstance;
	testInstance.modelId = 1;
	testInstance.transformation.setIdentity();
	sgsSceneRenderer.addInstance( testInstance );*/

	while (true)
	{
		// Activate the window for OpenGL rendering
		window.setActive();

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

			eventSystem.processEvent( event );
		}

		if( !window.isOpen() ) {
			break;
		}

		eventSystem.update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

		{
			boost::timer::cpu_timer renderTimer;
			sgsSceneRenderer.renderShadowmap( renderContext );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );
			glMultMatrix( camera.getViewTransformation().matrix() );

			glMatrixMode( GL_MODELVIEW );
			// TODO: move this into the render functions [9/23/2012 kirschan2]
			glLoadIdentity();

			sgsSceneRenderer.renderSceneView( camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition(), renderContext );

			probeDumps.render();

			selectObjectsByModelID( sgsSceneRenderer, renderContext.disabledModelIndex );
			glDisable( GL_DEPTH_TEST );
			selectionDR.render();
			glEnable( GL_DEPTH_TEST );

			const ViewerContext viewerContext = { camera.getProjectionMatrix() * camera.getViewTransformation().matrix(), camera.getPosition() };
			optixRenderer.renderPinholeCamera( viewerContext, renderContext );

			// End the current frame and display its contents on screen
			renderDuration.setString( renderTimer.format() );
			window.pushGLStates();
			window.resetGLStates();
			window.draw( renderDuration );
			window.popGLStates();
			window.display();

			debugWindowManager.update();
		}

	}
};
#endif

void main( int argc, const char **argv ) {
	try {
		real_main( argc, argv );
	}
	catch( std::exception &e) {
		std::cout << e.what() << "\n";
	}
}