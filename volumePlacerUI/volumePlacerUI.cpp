#include <niven.Core.Core.h>
#include <niven.Core.Exception.h>
#include <niven.Core.Log.h>
#include <niven.Core.String.h>

#include <niven.Core.IO.Path.h>

#include <niven.Volume.FileBlockStorage.h>
#include <niven.Volume.MarchingCubes.h>
#include <niven.Volume.Volume.h>

#include <niven.Engine.BaseApplication3D.h>
#include <niven.Engine.DebugRenderUtility.h>

#include <niven.Render.EffectLoader.h>
#include <niven.Render.EffectManager.h>

#include <niven.Core.Math.VectorToString.h>

#include <niven.Engine.Draw2D.h>
#include <niven.Engine.Draw2DRectangle.h>

#include "objModel.h"
#include "anttwbargroup.h"
#include "antTweakBarEventHandler.h"

#include "volumeCalibration.h"
#include "mipVolume.h"
#include "cache.h"

#include <vector>
#include <memory>

#include <iostream>

#include "ptree_serializer.h"
#include "anttwbarcollection.h"
#include "volumePlacer.h"

#include "ui.h"

//#include <boost/property_tree/ptree.hpp>

#if defined(NIV_DEBUG) && defined(NIV_OS_WINDOWS)
#define StartMemoryDebugging() \
	_CrtMemState __initialState; \
	_CrtMemCheckpoint(&__initialState); \
	_CrtMemDumpStatistics	(&__initialState); \
	_ASSERTE( _CrtCheckMemory( ) )
#define StopMemoryDebugging() \
	_ASSERTE( _CrtCheckMemory( ) );	\
	_CrtMemDumpAllObjectsSince(&__initialState)
#else
#define StartMemoryDebugging()
#define StopMemoryDebugging()
#endif

using namespace niven;

namespace AntTWBarGroupTypes {
	template<>
	struct TypeMapping< niven::Vector3i > {
		static int Type;
	};

	int TypeMapping< niven::Vector3i >::Type;
}

template<typename S>
struct NivenSummarizer {
	static void summarize( char *summary, size_t maxLength, const S* object) {
		std::ostringstream out;
		out << niven::StringConverter::ToString<S>( *object ) << '\0';
		out.str().copy( summary, maxLength );
	}
};

typedef std::vector<Render::DebugVertex> DebugRenderObject;

DebugRenderObject createCube (const Vector3f &minCorner, const Vector3f &maxCorner, const Color3f &color) {
	Render::DebugRenderUtility dru;
	dru.AddAABB (minCorner, maxCorner, color);
	return dru.GetLineSegments();
}

DebugRenderObject createCross (const Vector3f &position, const Color3f &color, float size = 1.0) {
	Render::DebugRenderUtility dru;
	dru.AddCross (position, color, size);
	return dru.GetLineSegments();
}

struct ObjectTemplate {
	int id;

	// centered around 0.0
	Vector3f bbSize;
	Color3f color;

	void Draw( Render::IRenderSystem *renderSystem, Render::IRenderContext *renderContext ) {
		DebugRenderObject bbox = createCube( -bbSize * 0.5, bbSize * 0.5, color );
		renderSystem->DebugDrawLines( bbox );
	}
};

template<ptree_serializer_mode mode, typename S, int N>
void ptree_serializer_exchange( ptree_serializer &tree, niven::Vector<S, N> &data ) {
	ptree_serialize<mode>( tree, "x", data[0] );
	if( N >= 2 )
		ptree_serialize<mode>( tree, "y", data[1] );
	if( N >= 3 )
		ptree_serialize<mode>( tree, "z", data[2] );
	if( N >= 4 )
		ptree_serialize<mode>( tree, "w", data[2] );
}

template<ptree_serializer_mode mode, typename S, int N>
void ptree_serializer_exchange( ptree_serializer &tree, niven::Color<S, N> &data ) {
	ptree_serialize<mode>( tree, "r", data[0] );
	if( N >= 2 )
		ptree_serialize<mode>( tree, "g", data[1] );
	if( N >= 3 )
		ptree_serialize<mode>( tree, "b", data[2] );
	if( N >= 4 )
		ptree_serialize<mode>( tree, "a", data[2] );
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

	virtual void Draw( Render::IRenderSystem *renderSystem, Render::IRenderContext *renderContext ) {
		Matrix4f worldBase = renderContext->GetTransformation( Render::RenderTransformation::World );

		renderContext->SetTransformation( Render::RenderTransformation::World, CreateTranslation4( position ) * worldBase );
		objectTemplate->Draw( renderSystem, renderContext );
		renderContext->SetTransformation( Render::RenderTransformation::World, worldBase );
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

/////////////////////////////////////////////////////////////////////////////
class SampleApplication : public BaseApplication3D, IEventHandler
{
	NIV_DEFINE_CLASS(SampleApplication, BaseApplication3D)

	struct CameraPosition {
		Vector3f position;
		Vector3f direction;

		CameraPosition( const Vector3f &position, const Vector3f &direction ) : position( position ), direction( direction ) {}
		CameraPosition() {}
	};

public:
	SampleApplication ()
	{
	}

private:
	void InitImpl ()
	{
		Super::InitImpl ();

		maxDistance_ = 128.0;

		//FixRDPMouseInput();
		InitAntTweakBar();

		effectManager_.Initialize (renderSystem_.get (), &effectLoader_);	
		objScene_.Init( renderSystem_, effectManager_, IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );

		camera_->GetFrustum ().SetPerspectiveProjection (
			Degree (75.0f),
			renderWindow_->GetAspectRatio (),
			0.1f, 10000.0f);

		camera_->SetMoveSpeedMultiplier( 0.5 );

		// init preview transformation
		previewTransformation_.projection = CreatePerspectiveProjectionFovRH( 
			Degree (75.0f),
			1.0f /*renderWindow_->GetAspectRatio ()*/,
			0.1f, 10.0f);
		previewTransformation_.view = CreateViewLookAtRH( Vector3f( 0.0, 0.0, -2.0 ), Vector3f::CreateZero(), Vector3f::CreateUnit(1) );

		InitVolume();

		UnorderedDistanceContext::setDirections();

		const Vector3i min = layerCalibration_.getGlobalFloorIndex( objScene_.boundingBox.GetMinimum() );
		const Vector3i size = layerCalibration_.getGlobalCeilIndex( objScene_.boundingBox.GetMaximum() ) - min;

		probes_ = std::unique_ptr<Probes>( new Probes( min, size, 16) );
		
		if( !probes_->readFromFile( "probes.data" ) ) {
			sampleProbes( *denseCache_, *probes_ );
			probes_->writeToFile( "probes.data" );
		}

		CreateProbeVisualization();
		probeVolume_ = createCube( layerCalibration_.getPosition( min ), layerCalibration_.getPosition( min + size ), Color3f( 1.0, 0.0, 0.0 ) );

		targetCube_ = probes_->getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i::CreateZero(), Vector3i::Constant(4) ) );

#if 0
		// init object templates
		{
			ObjectTemplate t;
			t.bbSize = layerCalibration_.getSize( probes_->getSize( Vector3i( 4, 1, 1) ) );
			t.color = Color3f( 1.0, 1.0, 1.0 );
			t.id = 0;
			objectTemplates_.push_back( t );

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
			const Cubei volumeCoords( layerCalibration_.getGlobalCeilIndex( bbox.minCorner ), layerCalibration_.getGlobalFloorIndex( bbox.maxCorner ) );

			probeDatabase_.addObjectInstanceToDatabase( *probes_, volumeCoords, objectInstances_[i].objectTemplate->id );
		}

		readState();

		InitUI();
		InitPreviewUI();
	}

	void InitPreviewUI() 
	{
		uiManager_.Init( renderSystem_.get(), renderContext_, renderWindow_ );

		uiButtons_.resize( objectTemplates_.size() );
		matchedProbes_.resize( objectTemplates_.size() );

		for( int i = 0 ; i < uiButtons_.size() ; i++ ) {
			uiManager_.elements.push_back( &uiButtons_[i] );
			uiButtons_[i].SetVisible( false );
			uiButtons_[i].onFocus = [=] () { activeProbe_ = i; };
			uiButtons_[i].onUnfocus = [=] () { if( !dontUnfocus ) activeProbe_ = -1; };
		}

		eventForwarder_.Prepend( &uiManager_ );

		activeProbe_ = -1;
	}

	void ShutdownImpl() {
		effectManager_.Shutdown ();

		Super::ShutdownImpl();
	}

	void UpdateImpl (const float deltaTime, const double elapsedTime) {
		Super::UpdateImpl( deltaTime, elapsedTime );

		float angle = elapsedTime * 360.0 / 10.0;
		previewTransformation_.world = CreateRotationY4( Degree( angle ) );		
	}

	void DrawPreview() {
		renderContext_->SetTransformation( Render::RenderTransformation::Projection, previewTransformation_.projection );
		renderContext_->SetTransformation( Render::RenderTransformation::View, previewTransformation_.view );
		
		const int maxPreviewWidth = renderWindow_->GetWidth() / 10;
		const int maxTotalPreviewHeight = (maxPreviewWidth + 10) * results_.size();
		const int previewSize = (maxTotalPreviewHeight < renderWindow_->GetHeight()) ? maxPreviewWidth : (renderWindow_->GetHeight() - 20) / results_.size() - 10;
				
		Vector2i topLeft( renderWindow_->GetWidth() - previewSize - 10, 10 );
		Vector2i size( previewSize, previewSize );
		
		for( int i = 0 ; i < results_.size() ; ++i, topLeft.Y() += previewSize + 10 ) {
			ObjectTemplate &objectTemplate = objectTemplates_[ results_[i].first ];

			renderContext_->SetViewport( Rectangle<int>( topLeft, size ) );
			renderContext_->ClearRenderTarget( Render::RenderTargetClearFlags::Depth_Buffer );

			renderContext_->SetTransformation( Render::RenderTransformation::World, previewTransformation_.world * CreateScale4( 2.0 / MaxElement( objectTemplate.bbSize ) ) );	

			objectTemplate.Draw( renderSystem_.get(), renderContext_ );

			UIButton &button = uiButtons_[i];
			button.area = Rectangle<int>(topLeft, size);
			button.SetVisible( true );
		}
				
		renderContext_->SetViewport( renderWindow_->GetViewport() );
	}

	void DrawImpl ()
	{
		renderContext_->SetTransformation( Render::RenderTransformation::World, Matrix4f::CreateIdentity() );

		objScene_.Draw( renderContext_ );

		Vector3f minCorner = layerCalibration_.getPosition( targetCube_.minCorner );
		Vector3f maxCorner = layerCalibration_.getPosition( targetCube_.maxCorner );
		DebugRenderObject targetVolume = createCube( minCorner, maxCorner, Color3f( 0.0, 1.0, 0.0 ) );
		Draw( targetVolume );
		
		Draw( probeVolume_ );
		if( showProbes ) {
			Draw( probeCrosses_ );
		}

		for( int i = 0 ; i < objectInstances_.size() ; i++ ) {
			objectInstances_[i].Draw( renderSystem_.get(), renderContext_ );
		}

		if( showMatchedProbes && activeProbe_ != -1 ) {
			renderSystem_->DebugDrawLines( matchedProbes_[activeProbe_].GetLineSegments() );
		}

		DrawPreview();

		TwDraw();

		uiManager_.Draw();
	}

private:
	void Draw( const DebugRenderObject &object ) {
		renderSystem_->DebugDrawLines( object );
	}

	void FixRDPMouseInput() {
		eventListener_.UnregisterWindow( renderWindow_ );
		eventListener_.RegisterWindow( renderWindow_, WindowEventHandlingFlags::ProcessSystemEvents | WindowEventHandlingFlags::ProcessInputEvents );
	}

	void InitAntTweakBar() {
		void *device;
		renderSystem_->GenericQuery( niven::Render::GenericQueryIds::DX11_GetDevice, &device );
		TwInit(  TW_DIRECT3D11, device );
		TwWindowSize( renderWindow_->GetWidth(), renderWindow_->GetHeight() );

		AntTWBarGroupTypes::TypeMapping<Vector3i>::Type = AntTWBarGroupTypes::Struct<Vector3i,AntTWBarGroupTypes::Vector3i, NivenSummarizer<Vector3i> >( "Vector3i" ).
			add( "x", &AntTWBarGroupTypes::Vector3i::x ).
			add( "y", &AntTWBarGroupTypes::Vector3i::y ).
			add( "z", &AntTWBarGroupTypes::Vector3i::z ).
			define();

		antTweakBarEventHandler_.Init( renderWindow_ );
		eventForwarder_.Prepend( &antTweakBarEventHandler_ );
	}

	void InitUI() {
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

		findCandidatesCallback_.callback = std::bind(&SampleApplication::Do_findCandidates, this);
		ui_->addButton("Find candidates", findCandidatesCallback_ );

		writeStateCallback_.callback = std::bind(&SampleApplication::writeState, this);
		ui_->addButton("Write state", writeStateCallback_ );

		candidateResultsUI_ = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup( "Candidates", ui_.get() ) );

		cameraPositions_.onAction = [=] ( const CameraPosition &camPos ) {
			camera_->SetViewLookAt( camPos.position, camPos.position + camPos.direction, Vector3f::CreateUnit(1) );
		};
		cameraPositions_.getSummary = [=] ( const CameraPosition &camPos, int ) {
			return AntTWBarGroup::format( "View %f %f %f", camPos.position[0], camPos.position[1], camPos.position[2] );
		};
		cameraPositions_.getNewItem = [=] () {
			return SampleApplication::CameraPosition( camera_->GetPosition(), camera_->GetViewDirection() );
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
			
	void InitVolume() {
		fbs_ = std::unique_ptr<Volume::FileBlockStorage>( new Volume::FileBlockStorage );
		if( !fbs_->Open( "P:\\BlenderScenes\\two_boxes_4.nvf", true ) ) {
			Log::Error( "VolumePlacerUI", "couldn't open the volume file!" );
		}

		mipVolume_ = std::unique_ptr<MipVolume>( new MipVolume(*fbs_) );
		denseCache_ = std::unique_ptr<DenseCache>( new DenseCache(*mipVolume_) );

		volumeCalibration_.readFrom( *fbs_ );
		layerCalibration_.readFrom( *fbs_, volumeCalibration_, "Density" );
	}

	void CreateProbeVisualization() {
		Render::DebugRenderUtility dru;

		const float visSize = 0.05;
		for( Iterator3D iter(probes_->probeDims) ; !iter.IsAtEnd() ; ++iter ) {
			const Vector3f probePosition = layerCalibration_.getPosition( probes_->getPosition( iter ) );
			const Probe &probe = probes_->get(iter);

			for( int i = 0 ; i < Probe::DistanceContext::numSamples ; ++i ) {
				const Vector3f &direction = probe.distanceContext.directions[i];
				const float distance = probe.distanceContext.distances[i];

				const Vector3f &vector = direction * (1.0 - distance / maxDistance_);
				dru.AddLine( probePosition, probePosition + vector * visSize, Color3f( 0.0, 0.0, Length(vector) ) );
			}			
		}
		probeCrosses_ = dru.GetLineSegments();
	}

	void CreatedMatchedProbes(int index) {
		matchedProbes_[index].Clear();

		const ProbeDatabase::CandidateInfo &info = results_[index].second;

		int begin = 0;
		for( int i = 0 ; i < info.matchesPositionEndOffsets.size() ; ++i ) {
			const Vector3f position = layerCalibration_.getPosition( probes_->getPosition( info.matchesPositionEndOffsets[i].first ) );

			const int end = info.matchesPositionEndOffsets[i].second;
			const int count = end - begin;
			matchedProbes_[index].AddSphere( position, 0.05, Color3f( 1.0 - float(count) / info.maxSingleMatchCount, 1.0, 0.0 ));

			begin = end;
		}
	}

	void Do_findCandidates() {
		ProbeMatchSettings settings;
		settings.maxDelta = probes_->step;
		settings.maxDistance = maxDistance_;

		results_ = probeDatabase_.findCandidates( *probes_, targetCube_ );

		candidateResultsUI_->clear();
		for( int i = 0 ; i < results_.size() ; ++i ) {
			const ProbeDatabase::CandidateInfo &info = results_[i].second;
			candidateResultsUI_->addVarRO( AntTWBarGroup::format( "Candidate %i", results_[i].first ), info.score );

			CreatedMatchedProbes(i);
		}
	}

	void writeState() {
		using namespace Serialize;

		FILE *file = fopen( "state", "wb" );
		writeTyped( file, targetCube_ );
		writeTyped( file, showProbes );
		writeTyped( file, showMatchedProbes );
		writeTyped( file, dontUnfocus );

		writeTyped( file, camera_->GetPosition() );
		writeTyped( file, camera_->GetViewDirection() );

		writeTyped( file, cameraPositions_.collection );
		writeTyped( file, cameraPositions_.collectionLabels );

		writeTyped( file, targetVolumes_.collection );
		writeTyped( file, targetVolumes_.collectionLabels );

		fclose( file );
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

		readTyped( file, cameraPositions_.collection );
		readTyped( file, cameraPositions_.collectionLabels );

		readTyped( file, targetVolumes_.collection );
		readTyped( file, targetVolumes_.collectionLabels );
				
		camera_->SetViewLookAt( position, position + viewDirection, Vector3f::CreateUnit(1) );
	}

private:
	// niven stuff
	Render::EffectManager effectManager_;
	Render::EffectLoader effectLoader_;

	// AntTweakBar members
	AntTweakBarEventHandler antTweakBarEventHandler_;
	std::unique_ptr<AntTWBarGroup> ui_, candidateResultsUI_;
	
	ObjScene objScene_;
	
	Cubei targetCube_;

	// volume members
	std::unique_ptr<Volume::FileBlockStorage> fbs_;
	std::unique_ptr<MipVolume> mipVolume_;
	std::unique_ptr<DenseCache> denseCache_;
	VolumeCalibration volumeCalibration_;
	LayerCalibration layerCalibration_;
	std::unique_ptr<Probes> probes_;

	ProbeDatabase probeDatabase_;

	ProbeDatabase::SparseCandidateInfos results_;

	// render objects
	DebugRenderObject probeVolume_;
	DebugRenderObject probeCrosses_;

	std::vector<Render::DebugRenderUtility> matchedProbes_;

	std::vector<ObjectTemplate> objectTemplates_;
	std::vector<ObjectInstance> objectInstances_;

	// ui callbacks
	AntTWBarGroup::ButtonCallback findCandidatesCallback_, writeStateCallback_;
	AntTWBarGroup::VariableCallback<Vector3i> minCallback_, sizeCallback_;

	// ui fields
	bool showProbes;
	bool showMatchedProbes;
	bool dontUnfocus;

	float maxDistance_;
	
	int activeProbe_;
	std::vector<UIButton> uiButtons_;
	UIManager uiManager_;

	struct Transformation {
		Matrix4f world;
		Matrix4f view;
		Matrix4f projection;
	} previewTransformation_;

	AntTWBarCollection<CameraPosition> cameraPositions_;
	AntTWBarCollection<Cubei> targetVolumes_;

private:
	SampleApplication( const SampleApplication & ) {}
};

NIV_IMPLEMENT_CLASS(SampleApplication, TypeInfo::Default, AppTest)

int main (int /* argc */, char* /* argv */ [])
{
	StartMemoryDebugging();
	{
#define EXC
		//#undef EXC
#ifdef EXC
		try
#endif
		{
			Core::Initialize ();

			Log::Info ("VolumePlacerUI","Log started");

			{
				SampleApplication app;
				app.Init ();
				app.Run ();
				app.Shutdown ();
			}

			Log::Info ("VolumePlacerUI","Log closed");

			Core::Shutdown ();
		}
#ifdef EXC
		catch (Exception& e)
		{
			std::cout << e.what () << std::endl;
			std::cerr << e.GetDetailMessage() << std::endl;
			std::cout << e.where () << std::endl;
		}
		catch (std::exception& e)
		{
			std::cout << e.what () << std::endl;
		}
#endif
	}
	StopMemoryDebugging();

	return 0;
}