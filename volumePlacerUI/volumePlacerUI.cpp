#include <niven.Core.Core.h>
#include <niven.Core.Exception.h>
#include <niven.Core.Log.h>
#include <niven.Core.String.h>

#include <niven.Core.IO.Path.h>

#include <niven.Volume.FileBlockStorage.h>
#include <niven.Volume.MarchingCubes.h>
#include <niven.Volume.Volume.h>

#include <niven.Engine.BaseApplication3D.h>
#include <niven.Engine.Event.KeyboardEvent.h>
#include <niven.Engine.DebugRenderUtility.h>

#include <niven.Render.EffectLoader.h>
#include <niven.Render.EffectManager.h>

#include <niven.Core.Math.VectorToString.h>

#include "objModel.h"
#include "anttwbargroup.h"
#include "antTweakBarEventHandler.h"

#include "volumeCalibration.h"
#include "mipVolume.h"
#include "cache.h"

#include <memory>

#include <iostream>

#include "volumePlacer.h"

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

		//FixRDPMouseInput();
		InitAntTweakBar();

		effectManager_.Initialize (renderSystem_.get (), &effectLoader_);	
		objModel_.Init( renderSystem_, effectManager_, IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );

		camera_->GetFrustum ().SetPerspectiveProjection (
			Degree (75.0f),
			renderWindow_->GetAspectRatio (),
			0.1f, 10000.0f);
		

		InitUI();		

		InitVolume();

		UnorderedDistanceContext::setDirections();

		const Vector3i min(850,120,120);
		const Vector3i size(280, 280, 280);
		probes = new Probes(min, size, 16);
		CreateProbeCrosses();

		sampleProbes( *denseCache, *probes );

		targetCube = probes->getVolumeFromIndexCube( Cubei::fromMinSize( Vector3i::CreateZero(), Vector3i::Constant(4) ) );

		probeVolume = createCube( layerCalibration.getPosition( min ), layerCalibration.getPosition( min + size ), Color3f( 1.0, 0.0, 0.0 ) );

		Cubei cube[2] = { Cubei::fromMinSize( Vector3i(0,0,0), Vector3i(4,1,1) ), Cubei::fromMinSize( Vector3i(5,5,5), Vector3i(4,1,1) ) };
		for( int i = 0 ; i < 2 ; i++ ) {
			Cubei volumeCoords = probes->getVolumeFromIndexCube( cube[i] );
			volume[i] = createCube( layerCalibration.getPosition( volumeCoords.minCorner ), layerCalibration.getPosition( volumeCoords.maxCorner ), Color3f( 1.0, 1.0, 1.0 ) );

			addObjectInstanceToDatabase( *probes, database, volumeCoords, i );
		}
	}

	void ShutdownImpl() {
		effectManager_.Shutdown ();
	}

	void DrawImpl ()
	{
		objModel_.Draw( renderContext_ );

		Vector3f minCorner = layerCalibration.getPosition( targetCube.minCorner );
		Vector3f maxCorner = layerCalibration.getPosition( targetCube.maxCorner );
		DebugRenderObject targetVolume = createCube( minCorner, maxCorner, Color3f( 0.0, 1.0, 0.0 ) );
		Draw( targetVolume );
		
		Draw( probeVolume );
		if( showProbePositions ) {
			Draw( probeCrosses );
		}

		for( int i = 0 ; i < 2 ; i++ ) {
			Draw( volume[i] );
		}

		TwDraw();
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

		antTweakBarEventHandler.Init( renderWindow_ );
		eventForwarder_.Prepend( &antTweakBarEventHandler );
	}

	void InitUI() {
		ui = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup("UI") );

		minCallback.getCallback = [&](Vector3i &v) { v = targetCube.minCorner; };
		minCallback.setCallback = [&](const Vector3i &v) { targetCube = Cubei::fromMinSize( v, targetCube.getSize() ); };
		ui->addVarCB("Min target", TW_TYPE_VECTOR3I, minCallback );

		sizeCallback.getCallback = [&](Vector3i &v) { v = targetCube.getSize(); };
		sizeCallback.setCallback = [&](const Vector3i &v) { targetCube = Cubei::fromMinSize( targetCube.minCorner, v ); };
		ui->addVarCB("Size target", TW_TYPE_VECTOR3I, sizeCallback );

		showProbePositions = false;
		ui->addVarRW( "Show probe positions", showProbePositions );

		findCandidatesCallback.callback = std::bind(&SampleApplication::Do_findCandidates, this);
		ui->addButton("Find candidates", findCandidatesCallback );

		candidateResultsUI = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup( "Candidates", ui.get() ) );
	}

	void InitVolume() {
		fbs = new Volume::FileBlockStorage;
		if( !fbs->Open( "P:\\BlenderScenes\\two_boxes_4.nvf", true ) ) {
			Log::Error( "VolumePlacerUI", "couldn't open the volume file!" );
		}

		mipVolume = new MipVolume(*fbs);
		denseCache = new DenseCache(*mipVolume);

		volumeCalibration.readFrom( *fbs );
		layerCalibration.readFrom( *fbs, volumeCalibration, "Density" );
	}

	void CreateProbeCrosses() {
		Render::DebugRenderUtility dru;
		for( Iterator3D iter(probes->probeDims) ; !iter.IsAtEnd() ; ++iter ) {
			dru.AddCross( layerCalibration.getPosition( probes->getPosition( iter ) ), Color3f(0.0, 1.0, 0.0), 0.05 );
		}
		probeCrosses = dru.GetLineSegments();
	}

	void Do_findCandidates() {
		results = findCandidates( *probes, database, targetCube );

		candidateResultsUI->clear();
		for( auto it = results.cbegin() ; it != results.cend() ; ++it ) {
			candidateResultsUI->addVarRO( AntTWBarGroup::format( "Candidate %i", it->first ), it->second );
		}
	}

private:
	// niven stuff
	Render::EffectManager effectManager_;
	Render::EffectLoader effectLoader_;

	// AntTweakBar members
	AntTweakBarEventHandler antTweakBarEventHandler;
	std::unique_ptr<AntTWBarGroup> ui, candidateResultsUI;
	
	ObjModel objModel_;
	
	Cubei targetCube;

	// volume members
	Volume::FileBlockStorage *fbs;
	MipVolume *mipVolume;
	DenseCache *denseCache;
	VolumeCalibration volumeCalibration;
	LayerCalibration layerCalibration;
	Probes *probes;

	ProbeDatabase database;

	ProbeDatabase::WeightedCandidateIdVector results;

	// render objects
	DebugRenderObject probeVolume;
	DebugRenderObject volume[2];

	DebugRenderObject probeCrosses;

	// ui callbacks
	AntTWBarGroup::ButtonCallback findCandidatesCallback;
	AntTWBarGroup::VariableCallback<Vector3i> minCallback, sizeCallback;

	// ui fields
	bool showProbePositions;

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