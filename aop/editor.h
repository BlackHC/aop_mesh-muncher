#pragma once

#include "sgsInterface.h"
#include "mathUtility.h"
#include "optixEigenInterop.h"
#include <verboseEventHandlers.h>
#include "make_nonallocated_shared.h"
#include <Eigen/Eigen>
#include <GL/glew.h>
#include <unsupported/Eigen/OpenGLSupport>

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

struct Editor : EventDispatcher {
	struct Volumes {
		virtual int getCount() = 0;
		virtual OBB *get( int index ) = 0;
	};

	struct VectorVolumes : Volumes {
		std::vector<OBB> obbs;

		int getCount() {
			return (int) obbs.size();
		}

		OBB *get( int index ) {
			return &obbs[ index ];
		}
	};

	struct ITransformer {
		enum Type {
			T_OBB,
			T_SGS
		} type;
		
		ITransformer( Type type ) : type( type ) {}

		virtual Eigen::Vector3f getSize() = 0;
		virtual void setSize( const Eigen::Vector3f &size ) = 0;

		virtual OBB::Transformation getTransformation() = 0;
		virtual void setTransformation( const OBB::Transformation &transformation ) = 0;

		virtual OBB getOBB() = 0;
		virtual void setOBB( const OBB &obb ) = 0;

		virtual bool canResize() = 0;

		virtual ~ITransformer() {}
	};

	struct OBBTransformer : ITransformer {
		int index;
		Editor *editor;

		OBBTransformer( Editor *editor, int index ) : ITransformer( T_OBB ), editor( editor ), index( index ) {}

		Eigen::Vector3f getSize() {
			return editor->volumes->get( index )->size;
		}

		void setSize( const Eigen::Vector3f &size ) {
			editor->volumes->get( index )->size = size;
		}

		OBB::Transformation getTransformation() {
			return editor->volumes->get( index )->transformation;
		}

		void setTransformation( const OBB::Transformation &transformation ) {
			editor->volumes->get( index )->transformation = transformation;
		}

		OBB getOBB() {
			return *editor->volumes->get( index );
		}

		void setOBB( const OBB &newObb ) {
			*editor->volumes->get( index ) = newObb;
		}

		bool canResize() {
			return true;
		}
	};

	struct SGSInstanceTransformer : ITransformer {
		int instanceIndex;
		Editor *editor;

		SGSInstanceTransformer( Editor *editor, int instanceIndex )
			:
			ITransformer( T_SGS ),
			instanceIndex( instanceIndex ),
			editor( editor )
		{}

		Eigen::Vector3f getSize() {
			return editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).sizes();
		}

		void setSize( const Eigen::Vector3f &size ) {
			// do nothing
		}

		OBB::Transformation getTransformation() {
			return
				editor->world->sceneRenderer.getInstanceTransformation( instanceIndex ) *
				Eigen::Translation3f( editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).center() )
				;
		}

		void setTransformation( const OBB::Transformation &transformation ) {
			if( editor->world->sceneRenderer.isDynamicInstance( instanceIndex ) ) {
				editor->world->sceneRenderer.setInstanceTransformation( 
					instanceIndex,
					transformation *
					Eigen::Translation3f( -editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).center() )
					);
			}
		}

		OBB getOBB() {
			OBB obb;
			obb.transformation = getTransformation();
			obb.size = getSize();
			return obb;
		}

		void setOBB( const OBB &obb ) {
			setTransformation( obb.transformation );
		}

		bool canResize() {
			return false;
		}
	};

	std::shared_ptr< ITransformer > transformer;

	SGSInterface::View *view;
	SGSInterface::World *world;
	Volumes *volumes;

	struct Mode : NullEventHandler {
		const char *name;
		Editor *editor;

		bool dragging;
		bool selected;
		//MouseDelta dragDelta;

		Mode( Editor *editor, const char *name ) : editor( editor ), name( name ), dragging( false ), selected( false ) {}
		virtual ~Mode() {}

		sf::Vector2i popMouseDelta() {
			return eventSystem->exclusiveMode.popMouseDelta();
		}

		virtual void render() {}
		virtual void storeState() {}
		virtual void restoreState() {}

		void startDragging() {
			if( dragging ) {
				return;
			}

			storeState();
			dragging = true;
			//dragDelta.reset();
			eventSystem->setCapture( this, FT_EXCLUSIVE );
		}

		void stopDragging( bool accept ) {
			if( !dragging ) {
				return;
			}

			if( accept ) {
				storeState();
			}
			else {
				restoreState();
			}

			dragging = false;
			eventSystem->setCapture( nullptr, FT_EXCLUSIVE );
		}

		void onSelected() {
			selected = true;
		}

		void onUnselected() {
			selected = false;
			stopDragging( false );
		}

		bool acceptFocus( FocusType focusType ) {
			return true;
		}
	};

	struct TransformMode : Mode {
		float transformSpeed;
		OBB::Transformation storedTransformation;

		virtual void transform( const Eigen::Vector3f &relativeMovement, bool localMode ) = 0;

		TransformMode( Editor *editor, const char *name ) : Mode( editor, name ), transformSpeed( 0.1f ) {}

		void storeState();
		void restoreState();

		virtual void onMouse( EventState &eventState );
		void onKeyboard( EventState &eventState );
		void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime );
		void onNotify( const EventState &eventState );

		std::string getHelp(const std::string &prefix /* = std::string */ ) {
			return prefix + name + ": click+drag with mouse to transform, and WASD, Space and Ctrl for precise transformation; keep shift pressed for faster transformation; use the mouse wheel to change precise transformation granularity; alt for local mode\n";
		}
	};

	void convertScreenToHomogeneous( int x, int y, float &xh, float &yh );

	struct Selecting : Mode {
		Selecting( Editor *editor, const char *name ) : Mode( editor, name ) {}

		void onMouse( EventState &eventState );

		std::string getHelp( const std::string &prefix /* = std::string */ ) {
			return prefix + name;
		}
	};

	struct Placing : Mode {
		Placing( Editor *editor, const char *name ) : Mode( editor, name ) {}

		void onMouse( EventState &eventState );

		std::string getHelp( const std::string &prefix /* = std::string */ ) {
			return prefix + name;
		}
	};

	Eigen::Vector3f getScaledRelativeViewMovement( const Eigen::Vector3f &relativeMovement );

	struct Moving : TransformMode {
		Moving( Editor *editor, const char *name ) : TransformMode( editor, name ) {}

		virtual void transform( const Eigen::Vector3f &relativeMovement, bool localMode );
	};

	struct Rotating : TransformMode {
		Rotating( Editor *editor, const char *name ) : TransformMode( editor, name ) {}

		void transform( const Eigen::Vector3f &relativeMovement, bool localMode );
	};

	struct Resizing : Mode {
		OBB storedOBB;

		Eigen::Vector3f mask, invertedMask;
		Eigen::Vector3f hitPoint;

		float transformSpeed;

		Resizing( Editor *editor, const char *name ) : Mode( editor, name ), transformSpeed( 0.01f ) {}

		void storeState();
		void restoreState();

		void transform( const Eigen::Vector3f &relativeMovement, bool fixCenter, bool invertMask );
		bool setCornerMasks( int x, int y );

		void onMouse( EventState &eventState );
		void onKeyboard( EventState &eventState );
		void onUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime );

		void render();

		std::string getHelp(const std::string &prefix /* = std::string */ ) {
			return prefix + name + ": click+drag with mouse to resize, Ctrl for symmetric resizing, Alt to invert the affected axes, keep shift pressed for faster resizing; use the mouse wheel to change precise transformation granularity\n";
		}
	};

	EventDispatcher dispatcher;
	TemplateEventRouter<Mode> modes;

	Selecting selecting;
	Placing placing;
	Moving moving;
	Rotating rotating;
	Resizing resizing;

	Editor()
		:
		EventDispatcher( "Editor" ),
		modes( "Mode" ),
		dispatcher( "" ),
		selecting( this, "Select" ),
		placing( this, "Place" ),
		moving( this, "Move" ),
		rotating( this, "Rotate" ),
		resizing( this, "Resize" )
	{}

	void selectMode( Mode *mode );

	void init();
	void render();
};