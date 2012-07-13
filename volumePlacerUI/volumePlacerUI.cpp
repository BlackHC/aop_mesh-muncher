#include <niven.Core.Core.h>
#include <niven.Core.Exception.h>
#include <niven.Core.Log.h>
#include <niven.Core.String.h>

#include <niven.Core.IO.Path.h>

#include <niven.Engine.Event.EventHelper.h>
#include <niven.Engine.Event.EventForwarder.h>

#include <niven.Engine.Event.MouseClickEvent.h>
#include <niven.Engine.Event.MouseMoveEvent.h>
#include <niven.Engine.Event.KeyboardEvent.h>
#include <niven.Engine.Event.MouseWheelEvent.h>
#include <niven.Engine.Event.TextInputEvent.h>
#include <niven.Engine.Event.RenderWindowResizedEvent.h>

#include <niven.Volume.FileBlockStorage.h>
#include <niven.Volume.MarchingCubes.h>
#include <niven.Volume.Volume.h>

#include <niven.Engine.BaseApplication3D.h>
#include <niven.Engine.Event.KeyboardEvent.h>

#include <niven.Render.EffectLoader.h>
#include <niven.Render.EffectManager.h>

#include <niven.Core.Math.VectorToString.h>

#include "objModel.h"
#include "anttwbargroup.h"

#include <memory>

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

int TW_TYPE_VECTOR3I = 0;

template<typename S>
struct NivenSummarizer {
	static void summarize( char *summary, size_t maxLength, const S* object) {
		std::ostringstream out;
		out << niven::StringConverter::ToString<S>( *object ) << '\0';
		out.str().copy( summary, maxLength );
	}
};

class AntTweakBarEventHandler : public niven::IEventHandler {
public:
	void Init(Render::IRenderWindow *renderWindow) {
		renderWindow_ = renderWindow;
		uiActive_ = false;
		keyModifiers_ = TW_KMOD_NONE;
		mouseWheelPos_ = 0;
	}

private:
	bool OnKeyboardImpl(const KeyboardEvent& event) {
		KeyCodes::Enum keyCode = event.GetCode();

		if (keyCode == KeyCodes::Key_Alt) {
			if (event.IsPressed())
				keyModifiers_ |= TW_KMOD_ALT;
			else
				keyModifiers_ &= ~TW_KMOD_ALT;
		} else if (keyCode == KeyCodes::Key_Control_Left ||
			keyCode == KeyCodes::Key_Control_Right) {
				if (event.IsPressed())
					keyModifiers_ |= TW_KMOD_CTRL;
				else 
					keyModifiers_ &= ~TW_KMOD_CTRL;
		} else if (keyCode == KeyCodes::Key_Shift_Left ||
			keyCode == KeyCodes::Key_Shift_Right) {
				if (event.IsPressed())
					keyModifiers_ |= TW_KMOD_SHIFT;
				else
					keyModifiers_ &= ~TW_KMOD_SHIFT;
		}

		if (!renderWindow_->IsCursorCaptured() && uiActive_) {
			if (!event.IsPressed())
				return false;
			int key = -1;

			switch (keyCode) {
			case KeyCodes::Key_Backspace:    key = TW_KEY_BACKSPACE; break;
			case KeyCodes::Key_Tab:          key = TW_KEY_TAB; break;
			case KeyCodes::Key_Return:       key = TW_KEY_RETURN; break;
			case KeyCodes::Key_Pause:        key = TW_KEY_PAUSE; break;
			case KeyCodes::Key_Escape:       key = TW_KEY_ESCAPE; break;
				//case KeyCodes::Key_Space:        key = TW_KEY_SPACE; break;
			case KeyCodes::Key_Delete:       key = TW_KEY_DELETE; break;
			case KeyCodes::Key_Cursor_Up:    key = TW_KEY_UP; break;
			case KeyCodes::Key_Cursor_Down:  key = TW_KEY_DOWN; break;
			case KeyCodes::Key_Cursor_Right: key = TW_KEY_RIGHT; break;
			case KeyCodes::Key_Cursor_Left:  key = TW_KEY_LEFT; break;
			case KeyCodes::Key_Insert:       key = TW_KEY_INSERT; break;
			case KeyCodes::Key_Home:         key = TW_KEY_HOME; break;
			case KeyCodes::Key_End:          key = TW_KEY_END; break;
			case KeyCodes::Key_Page_Up:      key = TW_KEY_PAGE_UP; break;
			case KeyCodes::Key_Page_Down:    key = TW_KEY_PAGE_DOWN; break;
			case KeyCodes::Key_F1:           key = TW_KEY_F1; break;
			case KeyCodes::Key_F2:           key = TW_KEY_F2; break;
			case KeyCodes::Key_F3:           key = TW_KEY_F3; break;
			case KeyCodes::Key_F4:           key = TW_KEY_F4; break;
			case KeyCodes::Key_F5:           key = TW_KEY_F5; break;
			case KeyCodes::Key_F6:           key = TW_KEY_F6; break;
			case KeyCodes::Key_F7:           key = TW_KEY_F7; break;
			case KeyCodes::Key_F8:           key = TW_KEY_F8; break;
			case KeyCodes::Key_F9:           key = TW_KEY_F9; break;
			case KeyCodes::Key_F10:          key = TW_KEY_F10; break;
			case KeyCodes::Key_F11:          key = TW_KEY_F11; break;
			case KeyCodes::Key_F12:          key = TW_KEY_F12; break;
			case KeyCodes::Key_F13:          key = TW_KEY_F13; break;
			case KeyCodes::Key_F14:          key = TW_KEY_F14; break;
			case KeyCodes::Key_F15:          key = TW_KEY_F15; break;
			default: return false;
			}

			TwKeyPressed(key, keyModifiers_);
		}

		return uiActive_;
	}

	bool OnMouseClickImpl(const MouseClickEvent& event) {
		if (!renderWindow_->IsCursorCaptured() && uiActive_) {
			TwMouseAction action;

			if (event.IsPressed()) {
				action = TW_MOUSE_PRESSED;
			} else {
				action = TW_MOUSE_RELEASED;
			}

			TwMouseButtonID button;
			switch (event.GetButton()) {
			case MouseButton::Left:
				button = TW_MOUSE_LEFT;
				break;
			case MouseButton::Middle:
				button = TW_MOUSE_MIDDLE;
				break;
			case MouseButton::Right:
				button = TW_MOUSE_RIGHT;
				break;
			default:
				return false;
			}

			bool handled = (TwMouseButton(action, button) == 1);
			return handled;
		}

		return false;
	}

	bool OnMouseMoveImpl(const MouseMoveEvent& event) {
		if (!renderWindow_->IsCursorCaptured()) {
			const Vector2i& mousePosition = event.GetPosition();

			bool handled = (TwMouseMotion(mousePosition[0], mousePosition[1]) == 1);
			SetActive( handled );

			return handled;
		}

		return false;
	}

	bool OnMouseWheelImpl(const MouseWheelEvent& event) {
		bool handled = false;
		if (!renderWindow_->IsCursorCaptured() && uiActive_) {
			if (event.GetDirection() == MouseWheelDirection::Vertical) {
				mouseWheelPos_ += event.GetAmount();
			}
			if( TwMouseWheel(mouseWheelPos_) == 1 )
				return true;
		}

		return true;
	}

	bool OnTextInputImpl(const niven::TextInputEvent& event) {
		if( !renderWindow_->IsCursorCaptured() && uiActive_ ) {
			char32 ch = event.GetCharacter();
			bool handled = false;
			if (31 < ch && ch < 128) {
				handled = (TwKeyPressed(ch, keyModifiers_) == 1);
			}
		}

		return uiActive_;
	}

	bool OnRenderWindowResizedImpl(const RenderWindowResizedEvent& event) {
		TwWindowSize(event.GetWidth(), event.GetHeight());

		return false;
	}

	void SetActive (bool active) {
		uiActive_ = active;
	}

	Render::IRenderWindow *renderWindow_;
	bool uiActive_;
	int keyModifiers_;
	int mouseWheelPos_;
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

		void *device;
		renderSystem_->GenericQuery( niven::Render::GenericQueryIds::DX11_GetDevice, &device );
		TwInit(  TW_DIRECT3D11, device );

		TW_TYPE_VECTOR3I = AntTWBarGroupTypes::Struct<Vector3i,AntTWBarGroupTypes::Vector3i, NivenSummarizer<Vector3i> >( "Vector3i" ).
			add( "x", &AntTWBarGroupTypes::Vector3i::x ).
			add( "y", &AntTWBarGroupTypes::Vector3i::y ).
			add( "z", &AntTWBarGroupTypes::Vector3i::z ).
			define();

		TwWindowSize( renderWindow_->GetWidth(), renderWindow_->GetHeight() );

		effectManager_.Initialize (renderSystem_.get (), &effectLoader_);	

		objModel_.Init( renderSystem_, effectManager_, IO::Path( "P:\\BlenderScenes\\two_boxes.obj" ) );

		camera_->GetFrustum ().SetPerspectiveProjection (
			Degree (75.0f),
			renderWindow_->GetAspectRatio (),
			0.1f, 10000.0f);

		ui = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup("UI") );

		ui->_addVarRW("min target", (TwType) TW_TYPE_VECTOR3I, (void*) &min );
		ui->_addVarRW("size target", (TwType) TW_TYPE_VECTOR3I, (void*) &size );

		antTweakBarEventHandler.Init( renderWindow_ );
		eventForwarder_.Prepend( &antTweakBarEventHandler );	
	}

	void ShutdownImpl() {
		effectManager_.Shutdown ();
	}

	void DrawImpl ()
	{
		objModel_.Draw( renderContext_ );

		TwDraw();
	}

private:
	ObjModel objModel_;
	AntTweakBarEventHandler antTweakBarEventHandler;


	Render::EffectManager effectManager_;
	Render::EffectLoader effectLoader_;

	std::unique_ptr<AntTWBarGroup> ui;
	Vector3i min, size;
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