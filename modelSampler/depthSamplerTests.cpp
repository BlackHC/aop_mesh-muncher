#include "depthSampler.h"

#include "gtest.h"

#include "debugRender.h"

#include <SFML/Window.hpp>

using namespace Eigen;

TEST( DepthSampler, rotatedCubeTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	glMatrixMode( GL_MODELVIEW );
	glRotatef( 45.0, 0.0, 1.0, 0.0 );
	const double scaleFactor = sqrt(2.0);
	scene.drawBox( Vector3f( 8 * scaleFactor, 8, 8 * scaleFactor ), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 8 * scaleFactor;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	float abs_eps = 0.1;
	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3i position = grid.getIndex3( i );
		
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 0 ) / scaleFactor, 7 - (position.x() + position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 1 ) / scaleFactor, (position.x() + position.z()) / 2.0 + 1, abs_eps );

		EXPECT_NEAR( sampler.depthSamples.getSample( i, 2 ) / scaleFactor, 4 - (position.x() - position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 3 ) / scaleFactor, (position.x() - position.z()) / 2.0 + 4, abs_eps );

		EXPECT_NEAR( sampler.depthSamples.getSample( i, 4 ), 7 - position.y(), abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 5 ), position.y() + 1, abs_eps );
	}
}

TEST( DepthSampler, rotatedBoxTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	glMatrixMode( GL_MODELVIEW );
	glRotatef( 45.0, 0.0, 1.0, 0.0 );
	const double scaleFactor = sqrt(2.0);
	scene.drawBox( Vector3f( 8 * scaleFactor, 12, 16 * scaleFactor ), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 16 * scaleFactor;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	float abs_eps = 0.1;
	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3i position = grid.getIndex3( i );

		EXPECT_NEAR( sampler.depthSamples.getSample( i, 0 ) / scaleFactor, 11 - (position.x() + position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 1 ) / scaleFactor, (position.x() + position.z()) / 2.0 + 5, abs_eps );

		EXPECT_NEAR( sampler.depthSamples.getSample( i, 2 ) / scaleFactor, 4 - (position.x() - position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 3 ) / scaleFactor, (position.x() - position.z()) / 2.0 + 4, abs_eps );

		EXPECT_NEAR( sampler.depthSamples.getSample( i, 4 ), 9 - position.y(), abs_eps );
		EXPECT_NEAR( sampler.depthSamples.getSample( i, 5 ), position.y() + 3, abs_eps );
	}
}

TEST( DepthSampler, cubeTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f::Constant(8), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 0 ), 4 - position.z() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 1 ), position.z() + 4 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 2 ), 4 - position.x() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 3 ), position.x() + 4 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 4 ), 4 - position.y() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 5 ), position.y() + 4 );
	}
}

TEST( DepthSampler, boxTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f(8, 12, 16), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i(7, 11, 15), Vector3f(-3, -5, -7), 1.0 );

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 0 ), 8 - position.z() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 1 ), position.z() + 8 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 2 ), 4 - position.x() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 3 ), position.x() + 4 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 4 ), 6 - position.y() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 5 ), position.y() + 6 );
	}
}

// to test for a bug with wrong main axes
TEST( DepthSampler, boxTest90DegreeRotated ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f(8, 12, 16), false, true );
	scene.end();

	const int permutation[3] = {2,0,1};
	OrientedGrid grid = OrientedGrid::from( OrientedGrid::from( Vector3i(7, 11, 15), Vector3f(-3, -5, -7), 1.0 ), permutation );
	
	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 0 ), 6 - position.y() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 1 ), position.y() + 6 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 2 ), 8 - position.z() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 3 ), position.z() + 8 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 4 ), 4 - position.x() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 5 ), position.x() + 4 );
	}
}

TEST( DepthSampler, nonUnitGridCubeTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f::Constant(16), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(7), Vector3f::Constant(-6), 2.0 );

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 0 ), 8 - position.z() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 1 ), position.z() + 8 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 2 ), 8 - position.x() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 3 ), position.x() + 8 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 4 ), 8 - position.y() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 5 ), position.y() + 8 );
	}
}