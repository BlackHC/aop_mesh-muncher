#pragma once

#include "aopApplication.h"

#include <memory>
#include "viewportContext.h"

namespace aop {
	struct ActionModelButton : ModelButtonWidget {
		ActionModelButton(
			const Eigen::Vector2f &offset,
			const Eigen::Vector2f &size,
			int modelIndex,
			SGSSceneRenderer &renderer,
			const std::function<void()> &action = nullptr
		) 
			: ModelButtonWidget( offset, size, modelIndex, renderer )
			, action( action )
		{}

		std::function<void()> action;

	private:
		void onAction() {
			if( action ) {
				action();
			}
		}
	};

	struct CandidateContainer : WidgetContainer {
		float minY, maxY;
		float scrollStep;

		CandidateContainer() : minY(), maxY(), scrollStep() {}

		void onMouse( EventState &eventState ) {
			if( eventState.event.type == sf::Event::MouseWheelMoved ) {
				//log( boost::format( "mouse wheel moved %i" ) % eventState.event.mouseWheel.delta );
					
				auto offset = transformChain.getOffset();
				offset.y() = std::min( -minY, std::max( -maxY, offset.y() + eventState.event.mouseWheel.delta * scrollStep ) );
				transformChain.setOffset( offset );

				eventState.accept();
			}
			else {
				WidgetContainer::onMouse( eventState );
			}
		}
	};

	// we set a scissor rectangle here
	struct LocalCandidateContainer : CandidateContainer {
		Eigen::Vector2f size;

		LocalCandidateContainer( const Eigen::Vector2f &size = Eigen::Vector2f::Zero() ) 
			: size( size )
		{
		}

		void onRender();
	};		

	struct LocalCandidateBarUI {
		typedef std::pair< float, int > ScoreModelIndexPair;

		Application *application;

		LocalCandidateContainer candidatesContainer;

		Obb queryObb;

		typedef std::vector< ScoreModelIndexPair > Candidates;
		Candidates candidates;

		LocalCandidateBarUI( Application *application, Candidates candidates, const Obb &queryObb ) 
			: application( application )
			, candidates( std::move( candidates ) )
			, queryObb( queryObb )
		{
			init();
		}

		void init();
		void refresh();
	};

	struct CandidateSidebarUI {
		typedef std::pair< float, int > ScoreModelIndexPair;

		Application *application;

		CandidateContainer sidebar;

		CandidateSidebarUI( Application *application ) : application( application ) {
			init();
		}

		void init() {				
			application->widgetRoot.addEventHandler( make_nonallocated_shared( sidebar ) );
		}

		void clear() {
			sidebar.clear();
		}

		void setModels( std::vector<ScoreModelIndexPair> scoredModelIndices, const Eigen::Vector3f &position );
	};

	inline std::shared_ptr<CandidateSidebarUI> createCandidateSidebarUI( Application *application ) {
		return std::make_shared<CandidateSidebarUI>( application );
	}
}