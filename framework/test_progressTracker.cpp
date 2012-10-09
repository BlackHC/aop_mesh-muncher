#include "progressTracker.h"

#include <gtest.h>

TEST( ProgressTracker, staticInterface ) {
	ProgressTracker::init();

	const auto rootTask = ProgressTracker::addTask( 0, 2 );

	const auto firstSubTask = ProgressTracker::addTask( rootTask, 1 );

	ASSERT_FLOAT_EQ( 0.0, ProgressTracker::getProgress() );

	ProgressTracker::markFinished( firstSubTask );

	ProgressTracker::markFinished( rootTask );

	ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );

	const auto secondSubTask = ProgressTracker::addTask( rootTask, 2 );

	ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );

	const auto taskA = ProgressTracker::addTask( secondSubTask, 2 );
	const auto taskB = ProgressTracker::addTask( secondSubTask, 1 );

	ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );

	ProgressTracker::markAllSubTasksFinished( taskA );

	ASSERT_FLOAT_EQ( 0.75, ProgressTracker::getProgress() );

	ProgressTracker::markFinished( secondSubTask );

	ASSERT_FLOAT_EQ( 0.75, ProgressTracker::getProgress() );

	ProgressTracker::markFinished( taskB );
		
	ASSERT_FLOAT_EQ( 1.0, ProgressTracker::getProgress() );

	ProgressTracker::markFinished( secondSubTask );

	ASSERT_FLOAT_EQ( 1.0, ProgressTracker::getProgress() );

	ProgressTracker::markFinished( rootTask );

	ASSERT_FLOAT_EQ( 1.0, ProgressTracker::getProgress() );
}

TEST( ProgressTracker, contextInterface ) {
	ProgressTracker::Context context1( 2 );

	ASSERT_FLOAT_EQ( 0.0, ProgressTracker::getProgress() );

	{
		ProgressTracker::Context context2( 2 );

		ASSERT_FLOAT_EQ( 0.0, ProgressTracker::getProgress() );

		context2.markFinished();

		ASSERT_FLOAT_EQ( 0.25, ProgressTracker::getProgress() );

		context2.markFinished();

		ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );
	}

	context1.markFinished();

	ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );

	{
		ProgressTracker::Context context2( 2 );

		ASSERT_FLOAT_EQ( 0.5, ProgressTracker::getProgress() );

		context2.markFinished();

		ASSERT_FLOAT_EQ( 0.75, ProgressTracker::getProgress() );

		context2.markFinished();

		ASSERT_FLOAT_EQ( 1.0, ProgressTracker::getProgress() );
	}

	context1.markFinished();

	ASSERT_FLOAT_EQ( 1.0, ProgressTracker::getProgress() );
}