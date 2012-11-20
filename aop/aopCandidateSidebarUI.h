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
		Application *application;

		ClippedContainer clipper;
		ScrollableContainer scroller;

		QueryResults queryResults;
		Obb queryObb;
		
		LocalCandidateBarUI( Application *application, QueryResults queryResults, Obb queryObb ) 
			: application( application )
			, queryResults( std::move( queryResults ) )
			, queryObb( queryObb )
		{
			init();
		}

		~LocalCandidateBarUI() {
			application->widgetRoot.removeEventHandler( &clipper );
		}

		void init();
		void refresh();
	};

	struct CandidateSidebarUI {
		Application *application;

		CandidateContainer sidebar;

		QueryResults queryResults;

		CandidateSidebarUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			application->widgetRoot.addEventHandler( make_nonallocated_shared( sidebar ) );
		}

		void clear() {
			sidebar.clear();
		}

		void setModels( const QueryResults &queryResults );
	};

	inline std::shared_ptr<CandidateSidebarUI> createCandidateSidebarUI( Application *application ) {
		return std::make_shared<CandidateSidebarUI>( application );
	}
}