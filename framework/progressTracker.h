#pragma once

#include "contextHelper.h"
#include <functional>

namespace ProgressTracker {
	typedef int Id;
	
	extern std::function< void() > onMarkFinished;

	Id addTask( Id parent, int numSubTasks );
	// returns false if there are subtasks left
	bool markFinished( Id item, int numSubTasks = 1 );
	void markAllSubTasksFinished( Id item );

	void init();
	void finish();

	float getProgress();

	struct Context : AsContext< int, Context > {
		ProgressTracker::Id item;
		bool itemDone;

		Context( int numSubTasks ) : AsContext( &item ) {
			const int *previous = getPrevious();
			item = ProgressTracker::addTask( previous ? *previous : 0, numSubTasks );
			itemDone = false;
		}

		void onEmptyPush() {
			ProgressTracker::init();
		}

		void onEmptyPop() {
			ProgressTracker::finish();
		}

		void onNonEmptyPop() {
			if( !itemDone ) {
				ProgressTracker::markAllSubTasksFinished( item );
			}
		}

		void markFinished( int numSubTasks = 1 ) {
			itemDone = itemDone || ProgressTracker::markFinished( item, numSubTasks );
		}
	};
}

