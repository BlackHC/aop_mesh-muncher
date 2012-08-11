/*#include "anttwbargroup.h"
#include "antTweakBarEventHandler.h"
*/

#include <iostream>

#include <vector>
#include <memory>
#include "make_nonallocated_shared.h"

#include <SFML/Window.hpp>
#include <AntTweakBar.h>

int TW_CALL TwEventSFML20(const sf::Event *event);

#include <GL/glew.h>

#include "objSceneGL.h"

#include "grid.h"

#include "debugRender.h"
#include "camera.h"
#include "cameraInputControl.h"

#include "eventHandling.h"

#include "depthSampler.h"

#include "volumePlacer.h"

#include <Eigen/Eigen>
using namespace Eigen;

#include "glHelpers.h"

Vector3i ceil( const Vector3f &v ) {
	return Vector3i( ceil( v[0] ), ceil( v[1] ), ceil( v[2] ) );
}

Vector3i floor( const Vector3f &v ) {
	return Vector3i( floor( v[0] ), floor( v[1] ), floor( v[2] ) );
}

struct AntTweakBarEventHandler : EventHandler {
	virtual bool handleEvent( const sf::Event &event ) 
	{
		return TwEventSFML20( &event );
	}
};

#include "ptree_serializer.h"
#include "anttwbarcollection.h"

#include "ui.h"

//#include <boost/property_tree/ptree.hpp>

namespace AntTWBarGroupTypes {
	template<>
	struct TypeMapping< Eigen::Vector3i > {
		static int Type;
	};

	int TypeMapping< Eigen::Vector3i >::Type;
}

template<typename S>
struct EigenSummarizer {
	static void summarize( char *summary, size_t maxLength, const S* object) {
		std::ostringstream out;
		out << *object << '\0';
		out.str().copy( summary, maxLength );
	}
};

struct ObjectTemplate {
	int id;

	// centered around 0.0
	Vector3f bbSize;
	Vector3f color;

	void Draw() {
		DebugRender::ImmediateCalls bbox;
		bbox.begin();
		bbox.setColor( color );
		bbox.drawAABB( -bbSize * 0.5, bbSize * 0.5 );
		bbox.end();
	}
};

/*
template<ptree_serializer_mode mode, typename S, int N>
void ptree_serializer_exchange( ptree_serializer &tree, Eigen::Matrix<S, N, 1> &data ) {
	ptree_serialize<mode>( tree, "x", data[0] );
	if( N >= 2 )
		ptree_serialize<mode>( tree, "y", data[1] );
	if( N >= 3 )
		ptree_serialize<mode>( tree, "z", data[2] );
	if( N >= 4 )
		ptree_serialize<mode>( tree, "w", data[2] );
}
*/

template<ptree_serializer_mode mode>
void ptree_serializer_exchange( ptree_serializer &tree, Eigen::Vector3f &data ) {
	ptree_serialize<mode>( tree, "x", data[0] );
	ptree_serialize<mode>( tree, "y", data[1] );
	ptree_serialize<mode>( tree, "z", data[2] );
}

template<ptree_serializer_mode mode>
void ptree_serializer_exchange( ptree_serializer &tree, ObjectTemplate &data ) {
	ptree_serialize<mode>( tree, "id", data.id );
	ptree_serialize<mode>( tree, "bbSize", data.bbSize );
	ptree_serialize<mode>( tree, "color", data.color );
}

struct ObjectInstance {
	ObjectTemplate *objectTemplate;

	Vector3f position;

	static ObjectInstance FromFrontTopLeft( const Vector3f &anchor, ObjectTemplate *objectTemplate ) {
		ObjectInstance instance;

		instance.objectTemplate = objectTemplate;
		instance.position = anchor + 0.5 * objectTemplate->bbSize;

		return instance;
	}

	Cubef GetBBox() const {
		return Cubef::fromMinSize( position - objectTemplate->bbSize * 0.5, objectTemplate->bbSize );
	}

	virtual void Draw() {
		glPushMatrix();
		glTranslate( position );
		objectTemplate->Draw();
		glPopMatrix();
	}
};

// TODO: blub
ObjectTemplate *base = nullptr;

template<ptree_serializer_mode mode>
void ptree_serializer_exchange( ptree_serializer &tree, ObjectInstance &data ) {
	if( mode == PSM_WRITING ) {
		ptree_serializer_put( tree, "templateId", data.objectTemplate->id );
	}
	else if( mode == PSM_READING ) {
		int id;
		ptree_serializer_get( tree, "templateId", id );
		data.objectTemplate = base + id;
	}

	ptree_serialize<mode>( tree, "position", data.position );
}

struct CameraPosition {
	Vector3f position;
	Vector3f direction;

	CameraPosition( const Vector3f &position, const Vector3f &direction ) : position( position ), direction( direction ) {}
	CameraPosition() {}
};

struct Application {
	ObjSceneGL objScene;
	GLuint viewerPPLProgram;
	Camera camera;
	CameraInputControl cameraInputControl;
	sf::Window window;

	EventDispatcher eventDispatcher;

	// probes
	Grid voxelGrid;
	std::unique_ptr<Probes> probes_;

	ProbeDatabase probeDatabase_;

	ProbeDatabase::SparseCandidateInfos results_;

	// anttweakbar 
	AntTweakBarEventHandler antTweakBarEventHandler;
	std::unique_ptr<AntTWBarGroup> ui_, candidateResultsUI_;

	// anttweakbar fields
	bool showProbes;
	bool showMatchedProbes;
	bool dontUnfocus;

	float maxDistance_;

	Cubei targetCube_;

	AntTWBarCollection<CameraPosition> cameraPositions_;
	AntTWBarCollection<Cubei> targetVolumes_;

	// anttweakbar callbacks
	AntTWBarGroup::VariableCallback<Vector3i> minCallback_, sizeCallback_;
	AntTWBarGroup::ButtonCallback writeStateCallback_;
	AntTWBarGroup::ButtonCallback findCandidatesCallback_;

	// debug visualizations
	DebugRender::CombinedCalls probeVisualization;
	DebugRender::CombinedCalls probeVolume;

	std::vector<DebugRender::CombinedCalls> matchedProbes_;

	// object data
	std::vector<ObjectTemplate> objectTemplates_;
	std::vector<ObjectInstance> objectInstances_;

	// preview
	struct Transformation {
		Matrix4f world;
		Matrix4f view;
		Matrix4f projection;
	} previewTransformation_;

	int activeProbe_;
	std::vector<UIButton> uiButtons_;
	UIManager uiManager_;
	
	void loadScene() {
		objScene.Init( "two_boxes.obj" );
	}

	void initCamera() {
		camera.perspectiveProjectionParameters.aspect = 640.0 / 480.0;
		camera.perspectiveProjectionParameters.FoV_y = 75.0;
		camera.perspectiveProjectionParameters.zNear = 0.1;
		camera.perspectiveProjectionParameters.zFar = 500.0;
	}

	void initEverything() {
		window.create( sf::VideoMode( 640, 480 ), "Position Solver", sf::Style::Default, sf::ContextSettings(32) );
		window.setActive( true );
		glewInit();

		initCamera();

		// input camera input control
		cameraInputControl.init( make_nonallocated_shared(camera), make_nonallocated_shared(window) );
		eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( cameraInputControl ) );

		// TODO: move
		maxDistance_ = 128.0;

		UnorderedDistanceContext::setDirections();

		readState();

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);

		glProgramBuilder programBuilder;
		programBuilder.
			attachShader(
				glShaderBuilder( GL_VERTEX_SHADER ).
					addSource( "varying vec3 viewPos; void main() { gl_FrontColor = gl_Color; viewPos = gl_ModelViewMatrix * gl_Vertex; gl_Position = ftransform(); }" ).
					compile().
					handle
			).
			attachShader( 
				glShaderBuilder( GL_FRAGMENT_SHADER ).
					addSource( "varying vec3 viewPos; void main() { vec3 normal = cross( dFdx( viewPos ), dFdy( viewPos ) ); gl_FragColor = vec4( gl_Color.rgb * max( 0.0, 2.0 / length( viewPos ) * abs( dot( normalize( normal ), normalize( viewPos ) ) ) ) + 0.1, 1.0 ); }" ).
					compile().
					handle
			).
			link().
			deleteShaders().
			dumpInfoLog( std::cout );

		viewerPPLProgram = programBuilder.program;

		loadScene();
		initProbes();

		initObjectData();

		initAntTweakBar();
		initAntTweakBarUI();

		initPreviewTransformation();
		initPreviewUI();
	}

	void initPreviewTransformation() {
		// init preview transformation
		previewTransformation_.projection = Eigen::createPerspectiveProjectionMatrix( 
			75.0f,
			1.0f,
			0.1f, 10.0f);
		previewTransformation_.view = Eigen::createViewerMatrix( Vector3f( 0.0, 0.0, 2.0 ), -Vector3f::UnitZ(), Vector3f::UnitY() );
	}

	void initPreviewUI() 
	{
		initPreviewTransformation();

		uiManager_.Init();

		uiButtons_.resize( objectTemplates_.size() );
		matchedProbes_.resize( objectTemplates_.size() );

		for( int i = 0 ; i < uiButtons_.size() ; i++ ) {
			uiManager_.elements.push_back( &uiButtons_[i] );
			uiButtons_[i].SetVisible( false );
			uiButtons_[i].onFocus = [=] () { activeProbe_ = i; };
			uiButtons_[i].onUnfocus = [=] () { if( !dontUnfocus ) activeProbe_ = -1; };
		}

		eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( uiManager_ ) );

		activeProbe_ = -1;
	}

	void initObjectData() {
#if 0
		// init object templates
		{
			ObjectTemplate t;
			t.bbSize = layerCalibration_.getSize( probes_->getSize( Vector3i( 4, 1, 1) ) );
			t.color = Color3f( 1.0, 1.0, 1.0 );
			t.id = 0;
			oµµbjectTemplates_.push_back( t );

			t.bbSize = layerCalibration_.getSize( probes_->getSize( Vector3i( 3, 1, 1 ) ) );
			t.color = Color3f( 0.0, 1.0, 1.0 );
			t.id++;
			objectTemplates_.push_back( t );
		}

		// create object instances
		{
			objectInstances_.push_back( ObjectInstance::FromFrontTopLeft( layerCalibration_.getPosition( probes_->getPosition( Vector3i( 0, 0, 0 ) ) ), &objectTemplates_[0] ) );
			objectInstances_.push_back( ObjectInstance::FromFrontTopLeft( layerCalibration_.getPosition( probes_->getPosition( Vector3i( 13, 0, 0 ) ) ), &objectTemplates_[0] ) );

			objectInstances_.push_back( ObjectInstance::FromFrontTopLeft( layerCalibration_.getPosition( probes_->getPosition( Vector3i( 5, 5, 5 ) ) ), &objectTemplates_[1] ) );
			objectInstances_.push_back( ObjectInstance::FromFrontTopLeft( layerCalibration_.getPosition( probes_->getPosition( Vector3i( 8, 8, 8 ) ) ), &objectTemplates_[1] ) );
		}

		ptree_serializer tree;
		ptree_serialize<PSM_WRITING>( tree, "objectTemplates", objectTemplates_);
		ptree_serialize<PSM_WRITING>( tree, "objectInstances", objectInstances_ );
		boost::property_tree::info_parser::write_info( "scenario.info", tree );
#else	
		ptree_serializer tree;
		boost::property_tree::info_parser::read_info( "scenario.info", tree );
		ptree_serialize<PSM_READING>( tree, "objectTemplates", objectTemplates_ );
		base = &objectTemplates_.front();
		ptree_serialize<PSM_READING>( tree, "objectInstances", objectInstances_ );
#endif

		// fill probe database
		for( int i = 0 ; i < objectInstances_.size() ; i++ ) {
			const Cubef bbox = objectInstances_[i].GetBBox();
			const Cubei volumeCoords( floor( voxelGrid.getIndex3( bbox.minCorner ) ), ceil( voxelGrid.getIndex3( bbox.maxCorner ) ) );

			probeDatabase_.addObjectInstanceToDatabase( *probes_, volumeCoords, objectInstances_[i].objectTemplate->id );
		}
	}

	void drawScene() {
		glUseProgram( viewerPPLProgram );
		objScene.Draw();
		glUseProgram( 0 );
	}

	void drawEverything() {
		drawScene();

		Vector3f minCorner = voxelGrid.getPosition( targetCube_.minCorner );
		Vector3f maxCorner = voxelGrid.getPosition( targetCube_.maxCorner );
		DebugRender::ImmediateCalls targetVolume;
		targetVolume.begin();
		targetVolume.setColor( Vector3f::Unit(1) );
		targetVolume.drawAABB( minCorner, maxCorner );
		targetVolume.end();

		probeVolume.render();
		if( showProbes ) {
			probeVisualization.render();
		}

		for( int i = 0 ; i < objectInstances_.size() ; i++ ) {
			objectInstances_[i].Draw();
		}

		if( showMatchedProbes && activeProbe_ != -1 ) {
			matchedProbes_[activeProbe_].render();
		}

		TwDraw();

		drawPreview();

		// setup a 2d drawing context
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glOrtho( 0.0, window.getSize().x, window.getSize().y, 0.0, -1.0, 1.0 );
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

		glDisable( GL_DEPTH_TEST );

		uiManager_.Draw();

		glEnable( GL_DEPTH_TEST );
	}

	void initAntTweakBar() {
		TwInit(  TW_OPENGL, NULL );
		TwWindowSize( window.getSize().x, window.getSize().y );

		AntTWBarGroupTypes::TypeMapping<Vector3i>::Type = AntTWBarGroupTypes::Struct<Eigen::Vector3i,AntTWBarGroupTypes::Vector3i, EigenSummarizer<Eigen::Vector3i> >( "Vector3i" ).
			add( "x", &AntTWBarGroupTypes::Vector3i::x ).
			add( "y", &AntTWBarGroupTypes::Vector3i::y ).
			add( "z", &AntTWBarGroupTypes::Vector3i::z ).
			define();

		eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( antTweakBarEventHandler ) );
	}

	void initProbes() {
		voxelGrid.init( Vector3i( 1024, 1024, 1024 ), Vector3f( -18.4209571, -7.4209571, -7.4209571 ), 7.4209571 / 256.0 );
		probes_ = std::unique_ptr<Probes>( new Probes( Vector3i( 127, 83, 83 ), Vector3i( 1025, 346, 346 ), 16) );

		if( !probes_->readFromFile( "probes.data" ) ) {
			sampleProbes( voxelGrid, *probes_, std::bind( &Application::drawScene, this ) );			
		}

		visualizeProbes();
	}

	void initAntTweakBarUI() {
		ui_ = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup("UI") );

		minCallback_.getCallback = [&](Vector3i &v) { v = targetCube_.minCorner; };
		minCallback_.setCallback = [&](const Vector3i &v) { targetCube_ = Cubei::fromMinSize( v, targetCube_.getSize() ); };
		ui_->addVarCB("Min target", minCallback_ );

		sizeCallback_.getCallback = [&](Vector3i &v) { v = targetCube_.getSize(); };
		sizeCallback_.setCallback = [&](const Vector3i &v) { targetCube_ = Cubei::fromMinSize( targetCube_.minCorner, v ); };
		ui_->addVarCB("Size target", sizeCallback_ );

		ui_->addVarRW( "Max probe distance", maxDistance_, "min=0" );

		showProbes = false;
		ui_->addVarRW( "Show probes", showProbes );

		showMatchedProbes = true;
		ui_->addVarRW( "Show matched probes", showMatchedProbes );

		dontUnfocus = true;
		ui_->addVarRW( "Fix selection", dontUnfocus );

		writeStateCallback_.callback = std::bind(&Application::writeState, this);
		ui_->addButton("Write state", writeStateCallback_ );

		findCandidatesCallback_.callback = std::bind(&Application::Do_findCandidates, this);
		ui_->addButton("Find candidates", findCandidatesCallback_ );
				
		candidateResultsUI_ = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup( "Candidates", ui_.get() ) );

		cameraPositions_.onAction = [=] ( const CameraPosition &camPos ) {
			camera.setPosition( camPos.position );
			camera.lookAt( camPos.direction, Vector3f::UnitY() );
		};
		cameraPositions_.getSummary = [=] ( const CameraPosition &camPos, int ) {
			return AntTWBarGroup::format( "View %f %f %f", camPos.position[0], camPos.position[1], camPos.position[2] );
		};
		cameraPositions_.getNewItem = [=] (){ 
			return CameraPosition( camera.getPosition(), camera.getDirection() );
		};
		cameraPositions_.init( "Camera views", nullptr );

		targetVolumes_.onAction = [=] ( const Cubei &tV ) {
			targetCube_ = tV;
		};
		targetVolumes_.getSummary = [=] ( const Cubei &tV, int ) -> std::string {
			Vector3i size = tV.getSize();
			return AntTWBarGroup::format( "(%i,%i,%i) %ix%ix%i", tV.minCorner[0], tV.minCorner[1], tV.minCorner[2], size[0], size[1], size[2] );
		};
		targetVolumes_.getNewItem = [=] () {
			return targetCube_;
		};
		targetVolumes_.init( "Target cubes", nullptr );
	}

	void visualizeProbes() {
		probeVisualization.begin();
#if 1
		const float visSize = 0.05;
		for( RangedIterator3 iter(Vector3i::Zero(), probes_->probeDims) ; iter.hasMore() ; ++iter ) {
			const Vector3f probePosition = voxelGrid.getPosition( probes_->getPosition( *iter ) );
			const Probe &probe = probes_->get(*iter);

			probeVisualization.setPosition( probePosition );

			for( int i = 0 ; i < Probe::DistanceContext::numSamples ; ++i ) {
				const Vector3f &direction = probe.distanceContext.directions[i];
				const float distance = probe.distanceContext.distances[i];

				const Vector3f &vector = direction * (1.0 - distance / maxDistance_);
				
				probeVisualization.setColor( Vector3f::Unit(2) * vector.norm() );
				probeVisualization.drawVector( vector * visSize );				
			}			
		}
#endif
		probeVisualization.end();

		probeVolume.begin();
		probeVolume.setColor( Vector3f::Unit(0) );
		probeVolume.drawAABB( voxelGrid.getPosition( probes_->min ), voxelGrid.getPosition( probes_->min + probes_->size ) );
		probeVolume.end();
	}

	void visualizeMatchedProbes(int index) {
		const ProbeDatabase::CandidateInfo &info = results_[index].second;

		matchedProbes_[index].begin();
		int begin = 0;
		for( int i = 0 ; i < info.matchesPositionEndOffsets.size() ; ++i ) {
			const Vector3f position = voxelGrid.getPosition( probes_->getPosition( info.matchesPositionEndOffsets[i].first ) );

			const int end = info.matchesPositionEndOffsets[i].second;
			const int count = end - begin;
			matchedProbes_[index].setPosition( position );
			matchedProbes_[index].setColor( Vector3f( 1.0 - float(count) / info.maxSingleMatchCount, 1.0, 0.0 ) );
			matchedProbes_[index].drawAbstractSphere( 0.05 );

			begin = end;
		}
		matchedProbes_[index].end();
	}

	void readState() {
		using namespace Serialize;

		FILE *file = fopen( "state", "rb" );

		if( !file ) {
			return;
		}

		readTyped( file, targetCube_ );
		readTyped( file, showProbes );		
		readTyped( file, showMatchedProbes );
		readTyped( file, dontUnfocus );

		Vector3f position, viewDirection;
		readTyped( file, position );
		readTyped( file, viewDirection );

		camera.setPosition( position );
		camera.lookAt( viewDirection, Vector3f::UnitY() );

		readTyped( file, cameraPositions_.collection );
		readTyped( file, cameraPositions_.collectionLabels );

		readTyped( file, targetVolumes_.collection );
		readTyped( file, targetVolumes_.collectionLabels );
	}

	void writeState() {
		using namespace Serialize;

		FILE *file = fopen( "state", "wb" );
		writeTyped( file, targetCube_ );
		writeTyped( file, showProbes );
		writeTyped( file, showMatchedProbes );
		writeTyped( file, dontUnfocus );

		writeTyped( file, camera.getPosition() );
		writeTyped( file, camera.getDirection() );

		writeTyped( file, cameraPositions_.collection );
		writeTyped( file, cameraPositions_.collectionLabels );

		writeTyped( file, targetVolumes_.collection );
		writeTyped( file, targetVolumes_.collectionLabels );

		fclose( file );
	}

	void Do_findCandidates() {
		ProbeMatchSettings settings;
		settings.maxDelta = probes_->step;
		settings.maxDistance = maxDistance_;

		results_ = probeDatabase_.findCandidates( *probes_, targetCube_ );

		activeProbe_ = 0;

		candidateResultsUI_->clear();
		for( int i = 0 ; i < results_.size() ; ++i ) {
			const ProbeDatabase::CandidateInfo &info = results_[i].second;
			candidateResultsUI_->addVarRO( AntTWBarGroup::format( "Candidate %i", results_[i].first ), info.score );

			visualizeMatchedProbes(i);
		}
	}

	void drawPreview() {
		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( previewTransformation_.projection );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( previewTransformation_.view );
		glMultMatrix( previewTransformation_.world );

		const int maxPreviewWidth = window.getSize().x / 10;
		const int maxTotalPreviewHeight = (maxPreviewWidth + 10) * results_.size();
		const int previewSize = (maxTotalPreviewHeight < window.getSize().y) ? maxPreviewWidth : (window.getSize().y - 20) / results_.size() - 10;

		Vector2i topLeft( window.getSize().x - previewSize - 10, 10 );
		Vector2i size( previewSize, previewSize );

		glEnable( GL_SCISSOR_TEST );

		for( int i = 0 ; i < results_.size() ; ++i, topLeft.y() += previewSize + 10 ) {
			ObjectTemplate &objectTemplate = objectTemplates_[ results_[i].first ];

			int flippedBottom = window.getSize().y - (topLeft.y() + size.y());
			glViewport( topLeft.x(), flippedBottom, size.x(), size.y() );
			glScissor( topLeft.x(), flippedBottom, size.x(), size.y() );
			
			glClear( GL_DEPTH_BUFFER_BIT );

			glPushMatrix();
			glScale( Vector3f::Constant( 2.0 / objectTemplate.bbSize.maxCoeff() ) );
			objectTemplate.Draw();
			glPopMatrix();

			UIButton &button = uiButtons_[i];
			button.area.min() = topLeft;
			button.area.max() = topLeft + size;
			button.SetVisible( true );
		}

		glViewport( 0, 0, window.getSize().x, window.getSize().y );
		glDisable( GL_SCISSOR_TEST );
	}

	typedef float Seconds;
	void update( const Seconds deltaTime, const Seconds elapsedTime ) {
		cameraInputControl.update( deltaTime, false );

		float angle = elapsedTime * 2 * Math::PI / 10.0;
		previewTransformation_.world = Affine3f(AngleAxisf( angle, Vector3f::UnitY() )).matrix();		
	}

	void main() {
		initEverything();
	
		// The main loop - ends as soon as the window is closed
		sf::Clock frameClock, clock;		
		while (window.isOpen())
		{
			// Event processing
			sf::Event event;
			while (window.pollEvent(event))
			{
				// Request for closing the window
				if (event.type == sf::Event::Closed) {
					window.close();
					return;
				}

				if( event.type == sf::Event::Resized ) {
					camera.perspectiveProjectionParameters.aspect = float( event.size.width ) / event.size.height;
					glViewport( 0, 0, event.size.width, event.size.height );
				}

				if( !cameraInputControl.getCapture() ) {
					eventDispatcher.handleEvent( event );
				}
				else {
					cameraInputControl.handleEvent( event );
				}
			}

			update( frameClock.restart().asSeconds(), clock.getElapsedTime().asSeconds() );

			// Activate the window for OpenGL rendering
			window.setActive();

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// OpenGL drawing commands go here...
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( camera.getProjectionMatrix() );

			glMatrixMode( GL_MODELVIEW );
			glLoadMatrix( camera.getViewTransformation().matrix() );

			drawEverything();

			// End the current frame and display its contents on screen
			window.display();
		}
	}
};

int main (int /* argc */, char* /* argv */ [])
{
	Application app;
	app.main();

	return 0;
}