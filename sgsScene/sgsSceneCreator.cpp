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

		return -1;
	}

	/*int addColorTexture( int r, int g, int b ) {
		const std::string name = boost::str( boost::format( "color %i %i %i" ) % r % g % b );

		int textureIndex = findTexture( name );
		if( textureIndex == SGSScene::NO_TEXTURE ) {
			SGSScene::Texture texture;
			texture.name = name;
			texture.rawContent
		}
	}*/

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

		material.textureIndex[0] = -1;

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

		model.startSubObject = scene->models.size();
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

	void pushIndex( SGSScene::SubObject &subObject, int index ) {
		scene->indices.push_back( index );
		subObject.numIndices++;
	}

	void pushQuad(
		SGSScene::SubObject &subObject,
		const SGSScene::Vertex &vertexA,
		const SGSScene::Vertex &vertexB,
		const SGSScene::Vertex &vertexC,
		const SGSScene::Vertex &vertexD
	) {
		int firstVertex = scene->vertices.size();

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

	static std::string getSimpleMaterialName( const SGSScene::Material &material ) {
		return boost::str( boost::format( "%i %i %i" ) % int( material.diffuse.r ) % int( material.diffuse.g ) % int( material.diffuse.b ) );
	}

	int addBoxModel( const Eigen::Vector3f &size, const SGSScene::Material &material ) {
		const auto name = boost::str( boost::format( "Box %f %f %f %s" ) % size.x() % size.y() % size.z() % getSimpleMaterialName( material ) );
		{
			int modelIndex = findModel( name );
			if( modelIndex != -1 ) {
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
			if( modelIndex != -1 ) {
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
struct ModelDeclarations {
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

	/*struct ModelImports {
		std::string scene;

		std::vector< std::string > modelName;

		SERIALIZER_DEFAULT_IMPL( (scene)(modelName) );
	};*/

	std::vector<BoxDeclaration> boxes;
	std::vector<SphereDeclaration> spheres;
	//std::vector<ModelImports> modelImports;

	//SERIALIZER_DEFAULT_IMPL( (boxes)(spheres)(modelImports) )
	SERIALIZER_DEFAULT_IMPL( (boxes)(spheres) )
};

ModelDeclarations readModelDeclarations( const char *filename ) {
	Serializer::TextReader reader( filename );

	ModelDeclarations modelDeclarations;
	Serializer::read( reader, modelDeclarations );

	return modelDeclarations;
}

void writeModelDeclarations( const char *filename, const ModelDeclarations &modelDeclarations ) {
	Serializer::TextWriter writer( filename );

	Serializer::write( writer, modelDeclarations );
}

using namespace boost;
namespace po = boost::program_options;
using namespace std;

void real_main( int argc, const char **argv ) {
	//vector< string > args = po::split_winmain( "exampleModelDecls.wml testScene.sgsScene" );
	vector< string > args;
	for( int i = 1 ; i < argc ; i++ ) {
		args.push_back( argv[ i ] );
	}
	
	string modelDeclFilename, targetSceneFilename;
	
	po::options_description desc( "Program options" );
	desc.add_options()
		( "help,?", "produce this help message" )
		( "modelDecls", po::value< std::string >( &modelDeclFilename )->default_value( "modelDecls.wml" ), "model declaration file to use" )
		( "target", po::value< std::string >( &targetSceneFilename ), "target scene file" )
		( "createExample", "dump an example model declaration" )
	;

	po::positional_options_description p;
	p.add( "modelDecls", 1 );
	p.add( "target", 1 );

	po::variables_map vm;
	po::store( po::command_line_parser( args ).options(desc).positional(p).run(), vm );
	po::notify(vm);

	if( vm.count( "help" ) ) {
		std::cout << desc;
		return;
	}
	else if( vm.count( "createExample" ) ) {
		ModelDeclarations modelDeclarations;

		modelDeclarations.boxes.push_back( ModelDeclarations::BoxDeclaration( Eigen::Vector3f::Constant( 5.0 ), Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );
		modelDeclarations.boxes.push_back( ModelDeclarations::BoxDeclaration( Eigen::Vector3f::Constant( 10.0 ), Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );

		modelDeclarations.spheres.push_back( ModelDeclarations::SphereDeclaration( 5.0, Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );
		modelDeclarations.spheres.push_back( ModelDeclarations::SphereDeclaration( 10.0, Eigen::Vector3f( 1.0f, 0.5f, 0.5f ) ) );

		writeModelDeclarations( "exampleModelDecls.wml", modelDeclarations );
		cout << "Successfully wrote 'exampleModelDecls.wml'!\n";
		return;
	}

	if( targetSceneFilename.empty() ) {
		targetSceneFilename = algorithm::erase_last_copy( modelDeclFilename, ".wml" ) + ".sgsScene";
	}

	// read the decls
	ModelDeclarations modelDeclarations;
	{
		Serializer::TextReader reader( modelDeclFilename.c_str() );
		cout << "Decls:\n" << wml::emit( reader.root );

		Serializer::read( reader, modelDeclarations );
	}

	{
		cout << "creating target scene..\n";
		SGSScene targetScene;

		cout << "creating primitive models..\n";
		SGSSceneRuntime runtime( &targetScene );
		for (auto box = modelDeclarations.boxes.begin() ; box != modelDeclarations.boxes.end() ; ++box ) {
			runtime.addBoxModel( Vector3f::Map( box->size ), runtime.createMaterial( Vector3f::Map( box->color ) ) );
		}

		for (auto sphere = modelDeclarations.spheres.begin() ; sphere != modelDeclarations.spheres.end() ; ++sphere ) {
			runtime.addSphereModel( sphere->radius, runtime.createMaterial( Vector3f::Map( sphere->color ) ), sphere->u, sphere->v );
		}

		/*cout << "importing models..\n";
		std::map< std::string, SGSScene > importedScenes;
		for( */

		cout << "writing target scene '" << targetSceneFilename << "'\n";
		{
			Serializer::BinaryWriter writer( targetSceneFilename.c_str() );

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
		std::vector< OptixRenderer::ProbeContext > probeContexts;

		OptixRenderer::Probe probe;
		Eigen::Vector3f::Map( &probe.position.x ) = position;
		Eigen::Vector3f::Map( &probe.direction.x ) = direction;

		probes.push_back( probe );

		optixRenderer.sampleProbes( probes, probeContexts, renderContext );

		probeDumps.append();
		probeDumps.setPosition( position );
		//glColor4ubv( &probeContexts.front().color.x );
		probeDumps.drawVectorCone( probeContexts.front().distance * direction, probeContexts.front().distance * 0.25, 1 + probeContexts.front().hitCounter );
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