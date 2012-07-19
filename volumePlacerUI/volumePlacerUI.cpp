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

#include "volumePlacer.h"

#include "ui.h"

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

int TW_TYPE_VECTOR3I = 0;

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

struct CandidateObject {
	// centered around 0.0
	Vector3f bbSize;
};



/////////////////////////////////////////////////////////////////////////////
class SampleApplication : public BaseApplication3D, IEventHandler
{
	NIV_DEFINE_CLASS(SampleApplication, BaseApplication3D)

public:
	SampleApplication ()
	{
	}

private:
	void InitImpl ()
	{
		Super::InitImpl ();

		maxDistance = 128.0;

		//FixRDPMouseInput();
		InitAntTweakBar();

		manager.Init( renderSystem_.get(), renderContext_, renderWindow_ );

		for( int i = 0 ; i < 2 ; i++ ) {
			manager.elements.push_back( &uiButtons[i] );
			uiButtons[i].SetVisible( false );
			uiButtons[i].onFocus = [=] () { activeProbe = i; };
			uiButtons[i].onUnfocus = [=] () { if( !dontUnfocus ) activeProbe = -1; };
		}

		eventForwarder_.Prepend( &manager );
		
		effectManager_.Initialize (renderSystem_.get (), &effectLoader_);	
		objModel_.Init( renderSystem_, effectManager_, IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );

		camera_->GetFrustum ().SetPerspectiveProjection (
			Degree (75.0f),
			renderWindow_->GetAspectRatio (),
			0.1f, 10000.0f);

		previewTransformation_.projection = CreatePerspectiveProjectionFovRH( 
			Degree (75.0f),
			1.0f /*renderWindow_->GetAspectRatio ()*/,
			0.1f, 10.0f);

		previewTransformation_.view = CreateViewLookAtRH( Vector3f( 0.0, 0.0, -2.0 ), Vector3f::CreateZero(), Vector3f::CreateUnit(1) );

		InitUI();	

		activeProbe = -1;

		InitVolume();

		UnorderedDistanceContext::setDirections();

		const Vector3i min(850,120,120);
		const Vector3i size(280, 280, 280);
		probes_ = std::unique_ptr<Probes>( new Probes(min, size, 16) );
		
		if( !probes_->readFromFile( "probes.data" ) ) {
			sampleProbes( *denseCache_, *probes_ );
			probes_->writeToFile( "probes.data" );
		}

		CreateProbeVisualization();

		targetCube_ = probes_->getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i::CreateZero(), Vector3i::Constant(4) ) );

		probeVolume = createCube( layerCalibration_.getPosition( min ), layerCalibration_.getPosition( min + size ), Color3f( 1.0, 0.0, 0.0 ) );

		Cubei cube[2] = { Cubei::fromMinSize( Vector3i(0,0,0), Vector3i(4,1,1) ), Cubei::fromMinSize( Vector3i(5,5,5), Vector3i(3,1,1) ) };

		for( int i = 0 ; i < 2 ; ++i ) {
			objects[i].bbSize = cube[i].getSize();
		}

		for( int i = 0 ; i < 2 ; i++ ) {
			Cubei volumeCoords = probes_->getVolumeFromIndexCube( cube[i] );
			volume[i] = createCube( layerCalibration_.getPosition( volumeCoords.minCorner ), layerCalibration_.getPosition( volumeCoords.maxCorner ), Color3f( 1.0, 1.0, 1.0 ) );

			addObjectInstanceToDatabase( *probes_, database, volumeCoords, i );
		}

		camera_->SetMoveSpeedMultiplier( 0.5 );

		readState();
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
		const int maxTotalPreviewHeight = (maxPreviewWidth + 10) * results.size();
		const int previewSize = (maxTotalPreviewHeight < renderWindow_->GetHeight()) ? maxPreviewWidth : (renderWindow_->GetHeight() - 20) / results.size() - 10;
				
		Vector2i topLeft( renderWindow_->GetWidth() - previewSize - 10, 10 );
		Vector2i size( previewSize, previewSize );
		
		for( int i = 0 ; i < results.size() ; ++i, topLeft.Y() += previewSize + 10 ) {
			CandidateObject &object = objects[ results[i].first ];

			renderContext_->SetViewport( Rectangle<int>( topLeft, size ) );
			renderContext_->ClearRenderTarget( Render::RenderTargetClearFlags::Depth_Buffer );

			renderContext_->SetTransformation( Render::RenderTransformation::World, previewTransformation_.world * CreateScale4( 2.0 / MaxElement( object.bbSize ) ) );	

			DebugRenderObject bbox = createCube( -object.bbSize * 0.5, object.bbSize * 0.5, Color3f( 1.0, 1.0, 1.0 ) );
			renderSystem_->DebugDrawLines( bbox );

			UIButton &button = uiButtons[i];
			button.area = Rectangle<int>(topLeft, size);
			button.SetVisible( true );
		}
				
		renderContext_->SetViewport( renderWindow_->GetViewport() );
	}

	void DrawImpl ()
	{
		renderContext_->SetTransformation( Render::RenderTransformation::World, Matrix4f::CreateIdentity() );

		objModel_.Draw( renderContext_ );

		Vector3f minCorner = layerCalibration_.getPosition( targetCube_.minCorner );
		Vector3f maxCorner = layerCalibration_.getPosition( targetCube_.maxCorner );
		DebugRenderObject targetVolume = createCube( minCorner, maxCorner, Color3f( 0.0, 1.0, 0.0 ) );
		Draw( targetVolume );
		
		Draw( probeVolume );
		if( showProbes ) {
			Draw( probeCrosses_ );
		}

		for( int i = 0 ; i < 2 ; i++ ) {
			Draw( volume[i] );
		}

		if( showMatchedProbes && activeProbe != -1 ) {
			renderSystem_->DebugDrawLines( matchedProbes_[activeProbe].GetLineSegments() );
		}

		DrawPreview();

		TwDraw();

		manager.Draw();
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

		TW_TYPE_VECTOR3I = AntTWBarGroupTypes::Struct<Vector3i,AntTWBarGroupTypes::Vector3i, NivenSummarizer<Vector3i> >( "Vector3i" ).
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
		ui_->addVarCB("Min target", TW_TYPE_VECTOR3I, minCallback_ );

		sizeCallback_.getCallback = [&](Vector3i &v) { v = targetCube_.getSize(); };
		sizeCallback_.setCallback = [&](const Vector3i &v) { targetCube_ = Cubei::fromMinSize( targetCube_.minCorner, v ); };
		ui_->addVarCB("Size target", TW_TYPE_VECTOR3I, sizeCallback_ );

		ui_->addVarRW( "Max probe distance", maxDistance, "min=0" );

		showProbes = false;
		ui_->addVarRW( "Show probes ", showProbes );
		ui_->addVarRW( "Show matched probes ", showMatchedProbes );
		ui_->addVarRW( "Fix selection", dontUnfocus );

		findCandidatesCallback_.callback = std::bind(&SampleApplication::Do_findCandidates, this);
		ui_->addButton("Find candidates", findCandidatesCallback_ );

		writeStateCallback_.callback = std::bind(&SampleApplication::writeState, this);
		ui_->addButton("Write state", writeStateCallback_ );

		candidateResultsUI_ = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup( "Candidates", ui_.get() ) );
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

			for( int i = 0 ; i < Probe::numSamples ; ++i ) {
				const Vector3f &direction = probe.directions[i];
				const float distance = probe.distances[i];

				const Vector3f &vector = direction * (1.0 - distance / MAX_DISTANCE);
				dru.AddLine( probePosition, probePosition + vector * visSize, Color3f( 0.0, 0.0, Length(vector) ) );
			}			
		}
		probeCrosses_ = dru.GetLineSegments();
	}

	void CreatedMatchedProbes(int index) {
		matchedProbes_[index].Clear();

		const ProbeDatabase::CandidateInfo &info = results[index].second;
		for( int i = 0 ; i < info.matches.size() ; ++i ) {
			const Vector3f position = layerCalibration_.getPosition( probes_->getPosition( info.matches[i].first ) );
			matchedProbes_[index].AddSphere( position, 0.05, Color3f( 1.0 - float(info.matches[i].second) / info.maxSingleMatchCount, 1.0, 0.0 ));
		}
	}

	void Do_findCandidates() {
		results = database.findCandidates( *probes_, targetCube_, maxDistance );

		candidateResultsUI_->clear();
		for( int i = 0 ; i < results.size() ; ++i ) {
			const ProbeDatabase::CandidateInfo &info = results[i].second;
			candidateResultsUI_->addVarRO( AntTWBarGroup::format( "Candidate %i", results[i].first ), info.score );

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

		camera_->SetViewLookAt( position, position + viewDirection, Vector3f::CreateUnit(1) );
	}

private:
	// niven stuff
	Render::EffectManager effectManager_;
	Render::EffectLoader effectLoader_;

	// AntTweakBar members
	AntTweakBarEventHandler antTweakBarEventHandler_;
	std::unique_ptr<AntTWBarGroup> ui_, candidateResultsUI_;
	
	ObjModel objModel_;
	
	Cubei targetCube_;

	// volume members
	std::unique_ptr<Volume::FileBlockStorage> fbs_;
	std::unique_ptr<MipVolume> mipVolume_;
	std::unique_ptr<DenseCache> denseCache_;
	VolumeCalibration volumeCalibration_;
	LayerCalibration layerCalibration_;
	std::unique_ptr<Probes> probes_;

	ProbeDatabase database;

	ProbeDatabase::SparseCandidateInfos results;

	// render objects
	DebugRenderObject probeVolume;
	DebugRenderObject volume[2];

	DebugRenderObject probeCrosses_;
	Render::DebugRenderUtility matchedProbes_[2];

	// ui callbacks
	AntTWBarGroup::ButtonCallback findCandidatesCallback_, writeStateCallback_;
	AntTWBarGroup::VariableCallback<Vector3i> minCallback_, sizeCallback_;

	// ui fields
	bool showProbes;
	bool showMatchedProbes;
	bool dontUnfocus;

	float maxDistance;

	CandidateObject objects[2];

	int activeProbe;
	UIButton uiButtons[2];
	UIManager manager;

	struct Transformation {
		Matrix4f world;
		Matrix4f view;
		Matrix4f projection;
	} previewTransformation_;
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