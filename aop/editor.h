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
#include "boost/range/algorithm/find.hpp"

struct Editor : EventDispatcher {
	struct Volumes {
		virtual int getCount() = 0;
		virtual Obb *get( int index ) = 0;
	};

	struct VectorVolumes : Volumes {
		std::vector<Obb> obbs;

		int getCount() {
			return (int) obbs.size();
		}

		Obb *get( int index ) {
			return &obbs[ index ];
		}
	};

	struct SelectionVisitor;

	struct ISelection {
		virtual Eigen::Vector3f getSize() = 0;
		virtual void setSize( const Eigen::Vector3f &size ) = 0;

		virtual Obb::Transformation getTransformation() = 0;
		virtual void setTransformation( const Obb::Transformation &transformation ) = 0;

		virtual Obb getObb() = 0;
		virtual void setObb( const Obb &obb ) = 0;

		virtual bool canResize() = 0;
		virtual bool canTransform() = 0;

		// the selection renders whatever border it wants itself
		virtual void render() = 0;

		virtual ~ISelection() {}

		virtual void acceptVisitor( SelectionVisitor &visitor ) = 0;
	};

	struct ObbSelection : ISelection {
		int index;
		Editor *editor;

		ObbSelection( Editor *editor, int index ) : editor( editor ), index( index ) {}

		Eigen::Vector3f getSize() {
			return editor->volumes->get( index )->size;
		}

		void setSize( const Eigen::Vector3f &size ) {
			editor->volumes->get( index )->size = size;
		}

		Obb::Transformation getTransformation() {
			return editor->volumes->get( index )->transformation;
		}

		void setTransformation( const Obb::Transformation &transformation ) {
			editor->volumes->get( index )->transformation = transformation;
		}

		Obb getObb() {
			return *editor->volumes->get( index );
		}

		void setObb( const Obb &newObb ) {
			*editor->volumes->get( index ) = newObb;
		}

		bool canResize() {
			return true;
		}

		bool canTransform() {
			return true;
		}

		void render() {
			Editor::renderHighlitOBB( getObb() );
		}

		void acceptVisitor( SelectionVisitor &visitor );
	};

	struct SGSInstanceSelection : ISelection {
		int instanceIndex;
		Editor *editor;

		SGSInstanceSelection( Editor *editor, int instanceIndex )
			:
			instanceIndex( instanceIndex ),
			editor( editor )
		{}

		Eigen::Vector3f getSize() {
			return editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).sizes();
		}

		void setSize( const Eigen::Vector3f &size ) {
			// do nothing
		}

		Obb::Transformation getTransformation() {
			return
				editor->world->sceneRenderer.getInstanceTransformation( instanceIndex ) *
				Eigen::Translation3f( editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).center() )
				;
		}

		void setTransformation( const Obb::Transformation &transformation ) {
			if( editor->world->sceneRenderer.isDynamicInstance( instanceIndex ) ) {
				editor->world->sceneRenderer.setInstanceTransformation( 
					instanceIndex,
					transformation *
					Eigen::Translation3f( -editor->world->sceneRenderer.getUntransformedInstanceBoundingBox( instanceIndex ).center() )
					);
			}
		}

		Obb getObb() {
			return Obb( getTransformation(), getSize() );
		}

		void setObb( const Obb &obb ) {
			setTransformation( obb.transformation );
		}

		bool canResize() {
			return false;
		}

		bool canTransform() {
			return true;
		}

		void render() {
			Editor::renderHighlitOBB( getObb() );
		}

		void acceptVisitor( SelectionVisitor &visitor );
	};
		
	struct SGSMultiModelSelection : ISelection {
		Editor *editor;
		std::vector<int> modelIndices;

		SGSMultiModelSelection( Editor *editor, int modelIndex ) :
			editor( editor ) 
		{
			modelIndices.push_back( modelIndex );
		}

		SGSMultiModelSelection( Editor *editor, std::vector<int> modelIndices ) :
			editor( editor ),
			modelIndices( modelIndices )
		{
		}

		// TODO: this is design fail: we should use multiple interfaces for all this and then use visitors [10/2/2012 kirschan2]
		virtual Eigen::Vector3f getSize() {
			throw std::exception( "not implemented!" );
		}
		virtual void setSize( const Eigen::Vector3f &size ) {
			throw std::exception( "not implemented!" );
		}

		virtual Obb::Transformation getTransformation() {
			throw std::exception( "not implemented!" );
		}

		virtual void setTransformation( const Obb::Transformation &transformation ) {
			throw std::exception( "not implemented!" );
		}

		virtual Obb getObb() {
			throw std::exception( "not implemented!" );
		}
		virtual void setObb( const Obb &obb ) {
			throw std::exception( "not implemented!" );
		}

		bool canResize() {
			return false;
		}

		bool canTransform() {
			return false;
		}

		void render() {
			// TODO: we could use a display list to speed things up [10/2/2012 kirschan2]
			// we walk the scene manually and render obbs for all instances that match
			const int numInstances = editor->world->sceneRenderer.getNumInstances();
			for( int instanceIndex = 0 ; instanceIndex < numInstances ; instanceIndex++ ) {
				const int modelIndex = editor->world->sceneRenderer.getModelIndex( instanceIndex );

				if( boost::range::find( modelIndices, modelIndex ) != modelIndices.end() ) {
					Editor::renderHighlitOBB( makeOBB( editor->world->sceneRenderer.getInstanceTransformation( instanceIndex ), editor->world->sceneRenderer.getModelBoundingBox( modelIndex ) ) );
				}
			}
		}

		void acceptVisitor( SelectionVisitor &visitor );
	};

	// NOTE: modified visitor pattern to handle the no object case as well [10/2/2012 kirschan2]
	struct SelectionVisitor {
		virtual void visit() {}

		void dispatch( ISelection *selection ) {
			if( selection ) {
				selection->acceptVisitor( *this );
			}
			else {
				visit();
			}
		}

		virtual void visit( ISelection *selection ) {}

		virtual void visit( ObbSelection *obbSelection ) {
			visit( (ISelection *) obbSelection );
		}

		virtual void visit( SGSInstanceSelection *instanceSelection ) {
			visit( (ISelection *) instanceSelection );
		}

		virtual void visit( SGSMultiModelSelection *modelSelection ) {
			visit( (ISelection *) modelSelection );
		}

		virtual ~SelectionVisitor() {}
	};

	std::shared_ptr< ISelection > selection;

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
		Obb::Transformation storedTransformation;

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

		void onKeyboard(  EventState &eventState );

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
		Obb storedOBB;

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

	static void renderHighlitOBB( const Obb &obb );
};


//////////////////////////////////////////////////////////////////////////
// visitor

inline void Editor::SGSInstanceSelection::acceptVisitor( SelectionVisitor &visitor ) {
	visitor.visit( this );
}

inline void Editor::ObbSelection::acceptVisitor( SelectionVisitor &visitor ) {
	visitor.visit( this );
}

inline void Editor::SGSMultiModelSelection::acceptVisitor( SelectionVisitor &visitor ) {
	visitor.visit( this );
}

