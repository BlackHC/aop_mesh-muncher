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

#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>

Vector3i ceil( const Vector3f &v ) {
	return Vector3i( ceil( v[0] ), ceil( v[1] ), ceil( v[2] ) );
}

Vector3i floor( const Vector3f &v ) {
	return Vector3i( floor( v[0] ), floor( v[1] ), floor( v[2] ) );
}

AlignedBox3f AlignedBox3f_fromMinSize( const Vector3f &min, const Vector3f &size ) {
	return AlignedBox3f( min, min + size );
}

AlignedBox3f AlignedBox3f_fromCenterSize( const Vector3f &center, const Vector3f &size ) {
	const Vector3f halfSize = size / 2;
	return AlignedBox3f( center - halfSize, center + halfSize );
}

struct AntTweakBarEventHandler : EventHandler {
	virtual bool handleEvent( const sf::Event &event ) 
	{
		return TwEventSFML20( &event );
	}
};

#include "anttwbarcollection.h"

#include "ui.h"

#include "contextHelper.h"

#include "serializer.h"

namespace Serializer {
	template< typename Reader, typename Scalar >
	void read( Reader &reader, Eigen::Matrix< Scalar, 3, 1 > &value ) {
		typedef Scalar (*ArrayPointer)[3];
		ArrayPointer array = (ArrayPointer) &value[0];
		read( reader, *array );
	}

	template< typename Emitter, typename Scalar >
	void write( Emitter &emitter, const Eigen::Matrix<Scalar, 3, 1> &value ) {
		typedef const Scalar (*ArrayPointer)[3];
		ArrayPointer array = (ArrayPointer) &value[0];
		write( emitter, *array );
	}

	template< typename Reader >
	void read( Reader &reader, AlignedBox3f &value ) {
		get( reader, "min", value.min() );
		get( reader, "max", value.max() );
	}

	template< typename Emitter >
	void write( Emitter &emitter, const AlignedBox3f &value ) {
		put( emitter, "min", value.min() );
		put( emitter, "max", value.max() );
	}

	template void write< TextEmitter, float >( TextEmitter &, const Eigen::Matrix< float, 3, 1 > & );
}

struct RenderContext : AsExecutionContext<RenderContext> {
	bool solidObjects;
	int disableObjectIndex;
	int disableTemplateId;
	bool disableObjects;

	void setDefault() {
		solidObjects = false;

		disableObjects = false;
		disableObjectIndex = -1;
		disableTemplateId = -1;
	}
};

struct ObjectTemplate {
	int id;

	// centered around 0.0
	Vector3f bbSize;
	Vector3f color;

	void Draw() {
		RenderContext renderContext;

		if( renderContext.disableTemplateId == id ) {
			return;
		}

		DebugRender::ImmediateCalls bbox;
		bbox.begin();
		bbox.setColor( color );
		bbox.drawAABB( -bbSize * 0.5, bbSize * 0.5, !renderContext.solidObjects );
		bbox.end();
	}
};

// TODO: blub
ObjectTemplate *objectTemplateBase = nullptr;

#define PUT_MEMBER( reader, value, member ) put( reader, #member, value.member )
#define GET_MEMBER( reader, value, member ) get( reader, #member, value.member )

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, ObjectTemplate &value ) {
		GET_MEMBER( reader, value, id );
		GET_MEMBER( reader, value, bbSize );
		GET_MEMBER( reader, value, color );
	}

	template< typename Emitter >
	void write( Emitter &emitter, const ObjectTemplate &value ) {
		PUT_MEMBER( emitter, value, id );
		PUT_MEMBER( emitter, value, bbSize );
		PUT_MEMBER( emitter, value, color );
	}
}

struct ObjectInstance {
	int templateId;
	Vector3f position;

	ObjectInstance() : templateId( 0 ), position( Vector3f::Zero() ) {}

	ObjectTemplate &getTemplate() const {
		return objectTemplateBase[ templateId ];
	}

	AlignedBox3f getBBox() const {
		return AlignedBox3f_fromCenterSize( position, getTemplate().bbSize );
	}

	virtual void draw() {
		glPushMatrix();
		glTranslate( position );
		getTemplate().Draw();
		glPopMatrix();
	}
};

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, ObjectInstance &value ) {
		GET_MEMBER( reader, value, templateId );
		GET_MEMBER( reader, value, position );		
	}

	template< typename Emitter >
	void write( Emitter &emitter, const ObjectInstance &value ) {
		PUT_MEMBER( emitter, value, templateId );
		PUT_MEMBER( emitter, value, position );
	}
}

struct CameraPosition {
	Vector3f position;
	Vector3f direction;

	CameraPosition( const Vector3f &position, const Vector3f &direction ) : position( position ), direction( direction ) {}
	CameraPosition() {}
};

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, CameraPosition &value ) {
		get( reader, "position", value.position );
		get( reader, "direction", value.direction );
	}

	template< typename Emitter >
	void write( Emitter &emitter, const CameraPosition &value ) {
		put( emitter, "position", value.position );
		put( emitter, "direction", value.direction );
	}
}

namespace Serializer {
	template< typename Reader >
	void read( Reader &reader, AntTWBarLabel &value ) {
		read( reader, value.value );
	}

	template< typename Emitter >
	void write( Emitter &emitter, const AntTWBarLabel &value ) {
		write( emitter, value.value );
	}
}

enum ProbeMask {
	MASK_NONE,
	MASK_ALL_SIBLINGS,
	MASK_ALL_OBJECTS
};

namespace Serializer {
	template<> 
	struct EnumReflection<ProbeMask> {
		struct LabelValue {
			const char *label;
			ProbeMask value;
		};
		static const LabelValue labelValuePairs[3];
		//static const char* labels[3];
	};

	//const char *EnumSimpleReflection<ProbeMask>::labels[3] = { "none", "all_siblings", "all_objects" };
	const EnumReflection<ProbeMask>::LabelValue EnumReflection<ProbeMask>::labelValuePairs[3] = {
		{"none", MASK_NONE},
		{"all_siblings", MASK_ALL_SIBLINGS},
		{"all_objects", MASK_ALL_OBJECTS}
	};

	//template boost::enable_if_c< boost::is_enum< ProbeMask >::value && sizeof( EnumSimpleReflection<ProbeMask>::labels[0] ) != 0 >::type write( TextEmitter &emitter, const ProbeMask &value );
}

#define ANTTWBARGROUPTYPES_DEFINE_CUSTOM_TYPE( globalType ) \
	namespace AntTWBarGroupTypes {\
		template<> \
		struct TypeMapping< globalType > { \
			static int Type; \
		}; \
		\
		int TypeMapping< globalType >::Type; \
	}

ANTTWBARGROUPTYPES_DEFINE_CUSTOM_TYPE( ProbeMask );

namespace AntTWBarGroupTypes {
	template<>
	struct TypeMapping< Eigen::Vector3i > {
		static int Type;
	};

	template<>
	struct TypeMapping< Eigen::Vector3f > {
		static int Type;
	};

	template<>
	struct TypeMapping< ObjectInstance > {
		static int Type;
	};

	int TypeMapping< Eigen::Vector3i >::Type;
	int TypeMapping< Eigen::Vector3f >::Type;
	int TypeMapping< ObjectInstance >::Type;
}

template<typename S>
struct EigenSummarizer {
	static void summarize( char *summary, size_t maxLength, const S* object) {
		std::ostringstream out;
		out << *object << '\0';
		out.str().copy( summary, maxLength );
	}
};

struct Application {
	ObjSceneGL objScene;
	GLuint viewerPPLProgram;
	Camera camera;
	CameraInputControl cameraInputControl;
	sf::Window window;

	EventDispatcher eventDispatcher;

	// probes
	ProbeDatabase probeDatabase_;
	ProbeDatabase::SparseCandidateInfos results_;

	// anttweakbar 
	AntTweakBarEventHandler antTweakBarEventHandler;
	std::unique_ptr<AntTWBarGroup> ui_, candidateResultsUI_;

	// anttweakbar fields
	bool showProbes;
	bool showMatchedProbes;
	bool dontUnfocus;
	bool showPrototype;
	bool solidObjects;
	ProbeMask probeMask;
	bool maskAllObjectsOnFind;
	
	float maxDistance_;
	float gridResolution_;

	AlignedBox3f targetCube_;

	AntTWBarCollection<CameraPosition> cameraPositions_;
	AntTWBarCollection<AlignedBox3f> targetVolumes_;

	// anttweakbar callbacks
	AntTWBarGroup::VariableCallback<Vector3f> minCallback_, sizeCallback_;
	AntTWBarGroup::ButtonCallback writeStateCallback_;
	AntTWBarGroup::ButtonCallback findCandidatesCallback_;
	AntTWBarGroup::ButtonCallback refillProbeDatabaseCallback;
	AntTWBarGroup::ButtonCallback writeObjectsCallback;

	// debug visualizations
	DebugRender::CombinedCalls probeVisualization;

	std::vector<DebugRender::CombinedCalls> matchedProbes_;

	// object data
	std::vector<ObjectTemplate> objectTemplates_;
	AntTWBarEditableCollection<ObjectInstance> objectInstances_;

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
		objScene.Init( "lawn_garage_house.obj" );
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

		EnvironmentContext::setDirections();

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
					addSource( "varying vec3 viewPos; void main() { vec3 normal = cross( dFdx( viewPos ), dFdy( viewPos ) ); gl_FragColor = vec4( gl_Color.rgb * max( 0.0, 0.4 + 5.0 / length( viewPos ) * abs( dot( normalize( normal ), normalize( viewPos ) ) ) ), 1.0 ); }" ).
					compile().
					handle
			).
			link().
			deleteShaders().
			dumpInfoLog( std::cout );

		viewerPPLProgram = programBuilder.program;

		loadScene();

		readState();
		readObjects();

		refillProbeDatabase();

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
			0.1f, 100.0f);
		previewTransformation_.view = Eigen::createViewerMatrix( Vector3f( 0.0, 0.0, 8.0 ), -Vector3f::UnitZ(), Vector3f::UnitY() );
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

	void refillProbeDatabase() {
		// reset the database
		probeDatabase_ = ProbeDatabase();

		RenderContext renderContext;
		renderContext.solidObjects = true;
		if( probeMask == MASK_ALL_OBJECTS ) {
			renderContext.disableObjects = true;
		}

		ProbeSettings probeSettings;
		probeSettings.maxDistance = maxDistance_;
		probeSettings.maxDelta = gridResolution_;

		probeVisualization.clear();
		// fill probe database (and visualize the probes)
		for( int i = 0 ; i < objectInstances_.items.size() ; i++ ) {
			const AlignedBox3f bbox = objectInstances_.items[i].getBBox();
			ProbeGrid probeGrid( OrientedGrid::from( floor( bbox.sizes() / gridResolution_ ) + Vector3i::Constant(1), bbox.min(), gridResolution_ ) );

			renderContext.disableObjectIndex = i;			
			if( probeMask == MASK_ALL_SIBLINGS ) {
				renderContext.disableTemplateId = objectInstances_.items[i].templateId;
			}

			sampleProbes( probeGrid, std::bind( &Application::drawScene, this ), maxDistance_ );
			probeDatabase_.addObjectInstanceToDatabase( probeGrid, objectInstances_.items[i].templateId );

			// visualize this probe grid
			probeVisualization.append();
			const float visSize = 0.05;
			for( Iterator3 iterator = probeGrid.getIterator() ; iterator.hasMore() ; ++iterator ) {
				const Vector3f &probePosition = probeGrid.getGrid().getPosition( iterator.getIndex3() );
				const Probe &probe = probeGrid[ *iterator ];

				probeVisualization.setPosition( probePosition );

				for( int i = 0 ; i < Probe::DistanceContext::numSamples ; ++i ) {
					const Vector3f &direction = probe.distanceContext.directions[i];
					const float distance = probe.distanceContext.distances[i];

					const Vector3f &vector = probeGrid.getGrid().getIndexDirection( direction ) * (1.0 - distance / maxDistance_);

					probeVisualization.setColor( Vector3f::Unit(2) * vector.norm() );
					probeVisualization.drawVector( vector * visSize );				
				}
			}
			probeVisualization.end();
		}

		//probeDatabase_.dumpMinDistances();
	}

	void drawScene() {
		glUseProgram( viewerPPLProgram );
		objScene.Draw();

		RenderContext renderContext;
		if( !renderContext.disableObjects ) {
			if( !renderContext.solidObjects ) {
				glUseProgram( 0 );
			}
			for( int i = 0 ; i < objectInstances_.items.size() ; i++ ) {
				if( renderContext.disableObjectIndex == i ) {
					continue;
				}

				objectInstances_.items[i].draw();
			}
		}

		glUseProgram( 0 );
	}

	void drawEverything() {
		{
			RenderContext context;
			context.solidObjects = solidObjects;
			drawScene();
		}

		DebugRender::ImmediateCalls targetVolume;
		targetVolume.begin();
		targetVolume.setColor( Vector3f::Unit(1) );
		targetVolume.drawAABB( targetCube_.min(), targetCube_.max() );
		targetVolume.end();

		if( showProbes ) {
			probeVisualization.render();
		}

		// draw the prototype
		if( showPrototype ) {
			glEnable( GL_LINE_STIPPLE );
			glLineStipple( 1, 0x003f );
			objectInstances_.prototype.draw();
			glDisable( GL_LINE_STIPPLE );
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

		AntTWBarGroupTypes::TypeMapping<Vector3i>::Type = 
			AntTWBarGroupTypes::Struct<Eigen::Vector3i, AntTWBarGroupTypes::Vector3i, EigenSummarizer<Eigen::Vector3i> >( "Vector3i" ).
			add( "x", &AntTWBarGroupTypes::Vector3i::x ).
			add( "y", &AntTWBarGroupTypes::Vector3i::y ).
			add( "z", &AntTWBarGroupTypes::Vector3i::z ).
			define();

		AntTWBarGroupTypes::TypeMapping<Vector3f>::Type = 
			AntTWBarGroupTypes::Struct<Eigen::Vector3f, AntTWBarGroupTypes::Vector3f, EigenSummarizer<Eigen::Vector3f> >( "Vector3f" ).
			add( "x", &AntTWBarGroupTypes::Vector3f::x, "step=0.1" ).
			add( "y", &AntTWBarGroupTypes::Vector3f::y, "step=0.1" ).
			add( "z", &AntTWBarGroupTypes::Vector3f::z, "step=0.1" ).
			define();
		
		AntTWBarGroupTypes::TypeMapping<ObjectInstance>::Type =
			AntTWBarGroupTypes::Struct<ObjectInstance>( "ObjectInstance" ).
			add( "templateId", &ObjectInstance::templateId, AntTWBarGroup::format( "min=0 max=%i", objectTemplates_.size() - 1 ).c_str() ).
			add( "position", &ObjectInstance::position ).
			define();

		AntTWBarGroupTypes::TypeMapping<ProbeMask>::Type =
			AntTWBarGroupTypes::Enum<ProbeMask>( "ProbeMask" ).
			add( "none" ).
			add( "all instances" ).
			add( "all objects" ).
			define();

		eventDispatcher.eventHandlers.push_back( make_nonallocated_shared( antTweakBarEventHandler ) );
	}

	void initAntTweakBarUI() {
		ui_ = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup("UI") );

		minCallback_.getCallback = [&](Vector3f &v) { v = targetCube_.min(); };
		minCallback_.setCallback = [&](const Vector3f &v) { targetCube_ = AlignedBox3f_fromMinSize( v, targetCube_.sizes() ); };
		ui_->addVarCB("Min target", minCallback_ );

		sizeCallback_.getCallback = [&](Vector3f &v) { v = targetCube_.sizes(); };
		sizeCallback_.setCallback = [&](const Vector3f &v) { targetCube_ = AlignedBox3f_fromMinSize( targetCube_.min(), v ); };
		ui_->addVarCB("Size target", sizeCallback_ );

		ui_->addVarRW( "Grid resolution", gridResolution_, "min=0 step = 0.1" );
		ui_->addVarRW( "Max probe distance", maxDistance_, "min=0 step=0.1" );
		ui_->addVarRW( "Show probes", showProbes );
		ui_->addVarRW( "Show matched probes", showMatchedProbes );		
		ui_->addVarRW( "Fix selection", dontUnfocus );
		ui_->addVarRW( "Show prototype", showPrototype );
		ui_->addVarRW( "Solid objects", solidObjects );
		ui_->addVarRW( "Refill probe mask", probeMask );
		ui_->addVarRW( "Mask all objects on find", maskAllObjectsOnFind );

		writeStateCallback_.callback = std::bind(&Application::writeState, this);
		ui_->addButton("Write state", writeStateCallback_ );

		writeObjectsCallback.callback = std::bind( &Application::writeObjects, this );
		ui_->addButton( "Save objects", writeObjectsCallback );

		findCandidatesCallback_.callback = std::bind(&Application::Do_findCandidates, this);
		ui_->addButton("Find candidates", findCandidatesCallback_ );

		refillProbeDatabaseCallback.callback = std::bind( &Application::refillProbeDatabase, this );
		ui_->addButton( "Refill probe database", refillProbeDatabaseCallback );
				
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

		targetVolumes_.onAction = [=] ( const AlignedBox3f &tV ) {
			targetCube_ = tV;
		};
		targetVolumes_.getSummary = [=] ( const AlignedBox3f &tV, int ) -> std::string {
			Vector3f size = tV.sizes();
			return AntTWBarGroup::format( "(%f,%f,%f) %fx%fx%f", tV.min()[0], tV.min()[1], tV.min()[2], size[0], size[1], size[2] );
		};
		targetVolumes_.getNewItem = [=] () {
			return targetCube_;
		};
		targetVolumes_.init( "Target cubes", nullptr );

		objectInstances_.onItemSelected = [=] (const ObjectInstance &value, int i) {
			objectInstances_.prototype = value;
		};

		objectInstances_.init( "Object Instances", nullptr );
	}

	void visualizeMatchedProbes(int index) {
		const ProbeDatabase::CandidateInfo &info = results_[index].second;

		matchedProbes_[index].begin();
		int begin = 0;
		for( int i = 0 ; i < info.matchesPositionEndOffsets.size() ; ++i ) {
			const Vector3f &position = info.matchesPositionEndOffsets[i].first;

			const int end = info.matchesPositionEndOffsets[i].second;
			const int count = end - begin;
			matchedProbes_[index].setPosition( position );
			matchedProbes_[index].setColor( Vector3f( 1.0 - float(count) / info.maxSingleMatchCount, 1.0, 0.0 ) );
			matchedProbes_[index].drawAbstractSphere( gridResolution_ / 4 );

			begin = end;
		}
		matchedProbes_[index].end();
	}

	void readState() {
		using namespace Serializer;

		TextReader reader( "state.json" );

		get( reader, "gridResolution", gridResolution_ );
		maxDistance_ = gridResolution_ * 8;

		get( reader, "solidObjects", solidObjects );
		get( reader, "probeMask", probeMask );
		get( reader, "maskAllObjectsOnFind", maskAllObjectsOnFind );

		get( reader, "targetCube", targetCube_ );
		get( reader, "showProbes", showProbes );
		get( reader, "showMatchedProbes", showMatchedProbes );
		get( reader, "dontUnfocus", dontUnfocus );
		get( reader, "showPrototype", showPrototype );

		Vector3f cameraPosition, cameraDirection;
		get( reader, "cameraPosition", cameraPosition );
		get( reader, "cameraDirection", cameraDirection );

		camera.setPosition( cameraPosition );
		camera.lookAt( cameraDirection, Vector3f::UnitY() );

		get( reader, "cameraPositions", cameraPositions_.collection );
		get( reader, "cameraPositionLabels", cameraPositions_.collectionLabels );

		get( reader, "targetVolumes", targetVolumes_.collection );
		get( reader, "targetVolumeLabels", targetVolumes_.collectionLabels );
	}

	void writeState() {
		using namespace Serializer;

		TextEmitter emitter( "state.json" );

		put( emitter, "gridResolution", gridResolution_ );
		put( emitter, "solidObjects", solidObjects );
		
		put( emitter, "probeMask", probeMask );
		put( emitter, "maskAllObjectsOnFind", maskAllObjectsOnFind );

		put( emitter, "targetCube", targetCube_ );
		put( emitter, "showProbes", showProbes );
		put( emitter, "showMatchedProbes", showMatchedProbes );
		put( emitter, "dontUnfocus", dontUnfocus );
		put( emitter, "showPrototype", showPrototype );
		
		put( emitter, "cameraPosition", camera.getPosition() );
		put( emitter, "cameraDirection", camera.getDirection() );

		put( emitter, "cameraPositions", cameraPositions_.collection );
		put( emitter, "cameraPositionLabels", cameraPositions_.collectionLabels );

		put( emitter, "targetVolumes", targetVolumes_.collection );
		put( emitter, "targetVolumeLabels", targetVolumes_.collectionLabels );
	}

	void readObjects() {
		Serializer::TextReader reader( "objects.json" );

		Serializer::get( reader, "objectTemplates", objectTemplates_ );
		objectTemplateBase = &objectTemplates_.front();
		Serializer::get( reader, "objectInstances", objectInstances_.items );
		Serializer::get( reader, "objectPrototype", objectInstances_.prototype );
	}

	void writeObjects() {
		Serializer::TextEmitter emitter( "objects.json" );

		Serializer::put( emitter, "objectTemplates", objectTemplates_ );
		Serializer::put( emitter, "objectInstances", objectInstances_.items );
		Serializer::put( emitter, "objectPrototype", objectInstances_.prototype );
	}

	void Do_findCandidates() {
		ProbeGrid probeGrid( OrientedGrid::from( Vector3i::Constant(1) + ceil( targetCube_.sizes() / gridResolution_ ), targetCube_.min(), gridResolution_ ) );

		ProbeSettings settings;
		settings.maxDelta = gridResolution_;
		settings.maxDistance = maxDistance_;

		{
			RenderContext renderContext;
			if( maskAllObjectsOnFind ) {
				renderContext.disableObjects = true;
			}
			sampleProbes( probeGrid, std::bind( &Application::drawScene, this ), maxDistance_ );
		}		

		results_ = probeDatabase_.findCandidates( probeGrid );

		activeProbe_ = -1;

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

		RenderContext renderContext;
		renderContext.solidObjects = true;

		glUseProgram( viewerPPLProgram );

		for( int i = 0 ; i < results_.size() ; ++i, topLeft.y() += previewSize + 10 ) {
			ObjectTemplate &objectTemplate = objectTemplates_[ results_[i].first ];

			int flippedBottom = window.getSize().y - (topLeft.y() + size.y());
			glViewport( topLeft.x(), flippedBottom, size.x(), size.y() );
			glScissor( topLeft.x(), flippedBottom, size.x(), size.y() );
			
			glClear( GL_DEPTH_BUFFER_BIT );

			glPushMatrix();
			glScale( Vector3f::Constant( 8.0 / objectTemplate.bbSize.norm() ) );
			objectTemplate.Draw();
			glPopMatrix();

			UIButton &button = uiButtons_[i];
			button.area.min() = topLeft;
			button.area.max() = topLeft + size;
			button.SetVisible( true );
		}
		for( int i = results_.size() ; i < objectTemplates_.size() ; ++i ) {
			uiButtons_[i].SetVisible( false );
		}

		glUseProgram( 0 );
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