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

#include <niven.Render.EffectLoader.h>
#include <niven.Render.EffectManager.h>

#include <objModel.h>

#include <iostream>

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

/////////////////////////////////////////////////////////////////////////////
class SampleApplication : public BaseApplication3D
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

		effectManager_.Initialize (renderSystem_.get (), &effectLoader_);	

		objModel_.Init( renderSystem_, effectManager_, IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );

		camera_->GetFrustum ().SetPerspectiveProjection (
			Degree (75.0f),
			renderWindow_->GetAspectRatio (),
			0.1f, 10000.0f);
	}

	void ShutdownImpl() {
		effectManager_.Shutdown ();
	}

	void DrawImpl ()
	{
		objModel_.Draw( renderContext_ );
	}

private:
	ObjModel objModel_;

	Render::EffectManager effectManager_;
	Render::EffectLoader effectLoader_;
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