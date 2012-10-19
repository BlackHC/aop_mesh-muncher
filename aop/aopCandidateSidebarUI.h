#pragma once

#include "aopApplication.h"

#include <memory>

namespace aop {
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

	struct LocalCandidateBarUI {
		typedef std::pair< float, int > ScoreModelIndexPair;

		Application *application;

		ClippedContainer clipper;
		ScrollableContainer scroller;

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