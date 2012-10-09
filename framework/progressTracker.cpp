#include "progressTracker.h"

#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>

#include <vector>
#include <exception>
#include <deque>

namespace ProgressTracker {
	typedef int Id;

	namespace {
		sf::Mutex mutex;

		struct Task {
			Id parent;
			int numSubTasks;
			int numFinishedSubTasks;
			int numFinishedSplitTasks;

			Task( Id parent = 0, int numSubTasks = 0 ) : parent( parent ), numSubTasks( numSubTasks ), numFinishedSubTasks(), numFinishedSplitTasks() {}
		};

		std::vector< Task > tasks;
		std::vector< Id > freeTaskIds;
	}

	std::function< void() > onMarkFinished;
	
	// TODO: rename to splitTask [10/8/2012 kirschan2]
	Id addTask( Id parent, int numSubTasks ) {
		sf::Lock lock( mutex );

		if( tasks.empty() ) {
			throw std::logic_error( "ProgressTracker has not been initialized!" );
		}

		if( !freeTaskIds.empty() ) {
			Id item = freeTaskIds.back();
			if( item > parent ) {
				freeTaskIds.pop_back();
				tasks[ item ] = Task( parent, numSubTasks );

				return item;
			}
		}

		Id item = (Id) tasks.size();
		tasks.push_back( Task( parent, numSubTasks ) );
			
		return item;
	}

	bool markFinished( Id item, int numSubTasks ) {
		sf::Lock lock( mutex );

		Task &task = tasks[ item ];
		task.numFinishedSubTasks += numSubTasks;
		task.numFinishedSplitTasks = std::max( 0, task.numFinishedSplitTasks - numSubTasks );

		bool itemDone = false;
		if( item != 0 && task.numFinishedSubTasks == task.numSubTasks ) {
			Task &parentTask = tasks[ task.parent ];
			parentTask.numFinishedSplitTasks++;

			task.parent = -1;
			freeTaskIds.push_back( item );
			itemDone = true;
		}

		if( onMarkFinished ) {
			onMarkFinished();
		}

		return itemDone;
	}

	void markAllSubTasksFinished( Id item ) {
		sf::Lock lock( mutex );

		Task &task = tasks[ item ];
		markFinished( item, task.numSubTasks - task.numFinishedSubTasks );
	}

	void init() {
		sf::Lock lock( mutex );

		tasks.clear();
		tasks.push_back( Task( 0, 1 ) );

		freeTaskIds.clear();
	}

	void finish() {
		sf::Lock lock( mutex );

		tasks.clear();
		freeTaskIds.clear();
	}

	float getProgress() {
		sf::Lock lock( mutex );

		if( tasks.empty() ) {
			return 1.0;
		}

		std::vector< float > localProgress( tasks.size() );
		for( int taskIndex = (Id) tasks.size() - 1 ; taskIndex >= 0 ; taskIndex-- ) {
			Task &task = tasks[ taskIndex ];

			if( task.parent == -1 ) {
				continue;
			}

			const float totalLocalProgress = 
				(localProgress[ taskIndex ] + task.numFinishedSubTasks + task.numFinishedSplitTasks) / task.numSubTasks
			;
					
			// only push the progress if it hasn't been marked complete in the parent yet
			if( taskIndex > 0 ) {
				localProgress[ task.parent ] += totalLocalProgress;
			}
			else {
				return totalLocalProgress;
			}
		}
	}
}