#include "colorAndDepthSampler.h"

#include "gtest.h"

#include "debugRender.h"

#include <SFML/Window.hpp>

using namespace Eigen;

TEST( Sampler, rotatedCubeTest ) {
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

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 8 * scaleFactor;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	float abs_eps = 0.1;
	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3i position = grid.getIndex3( i );
		
		EXPECT_NEAR( samples.getSample( i, 0 ).depth / scaleFactor, 7 - (position.x() + position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( samples.getSample( i, 1 ).depth / scaleFactor, (position.x() + position.z()) / 2.0 + 1, abs_eps );

		EXPECT_NEAR( samples.getSample( i, 2 ).depth / scaleFactor, 4 - (position.x() - position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( samples.getSample( i, 3 ).depth / scaleFactor, (position.x() - position.z()) / 2.0 + 4, abs_eps );

		EXPECT_NEAR( samples.getSample( i, 4 ).depth, 7 - position.y(), abs_eps );
		EXPECT_NEAR( samples.getSample( i, 5 ).depth, position.y() + 1, abs_eps );
	}
}

TEST( Sampler, rotatedBoxTest ) {
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

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 16 * scaleFactor;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	float abs_eps = 0.1;
	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3i position = grid.getIndex3( i );

		EXPECT_NEAR( samples.getSample( i, 0 ).depth / scaleFactor, 11 - (position.x() + position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( samples.getSample( i, 1 ).depth / scaleFactor, (position.x() + position.z()) / 2.0 + 5, abs_eps );

		EXPECT_NEAR( samples.getSample( i, 2 ).depth / scaleFactor, 4 - (position.x() - position.z()) / 2.0, abs_eps );
		EXPECT_NEAR( samples.getSample( i, 3 ).depth / scaleFactor, (position.x() - position.z()) / 2.0 + 4, abs_eps );

		EXPECT_NEAR( samples.getSample( i, 4 ).depth, 9 - position.y(), abs_eps );
		EXPECT_NEAR( samples.getSample( i, 5 ).depth, position.y() + 3, abs_eps );
	}
}

TEST( Sampler, cubeTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f::Constant(8), false, true );
	scene.end();

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( samples.getSample( i, 0 ).depth, 4 - position.z() );
		EXPECT_EQ( samples.getSample( i, 1 ).depth, position.z() + 4 );

		EXPECT_EQ( samples.getSample( i, 2 ).depth, 4 - position.x() );
		EXPECT_EQ( samples.getSample( i, 3 ).depth, position.x() + 4 );

		EXPECT_EQ( samples.getSample( i, 4 ).depth, 4 - position.y() );
		EXPECT_EQ( samples.getSample( i, 5 ).depth, position.y() + 4 );
	}
}

TEST( Sampler, boxTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f(8, 12, 16), false, true );
	scene.end();

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i(7, 11, 15), Vector3f(-3, -5, -7), 1.0 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( samples.getSample( i, 0 ).depth, 8 - position.z() );
		EXPECT_EQ( samples.getSample( i, 1 ).depth, position.z() + 8 );

		EXPECT_EQ( samples.getSample( i, 2 ).depth, 4 - position.x() );
		EXPECT_EQ( samples.getSample( i, 3 ).depth, position.x() + 4 );

		EXPECT_EQ( samples.getSample( i, 4 ).depth, 6 - position.y() );
		EXPECT_EQ( samples.getSample( i, 5 ).depth, position.y() + 6 );
	}
}

// to test for a bug with wrong main axes
TEST( Sampler, boxTest90DegreeRotated ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f(8, 12, 16), false, true );
	scene.end();

	const int permutation[3] = {2,0,1};
	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i(7, 11, 15), Vector3f(-3, -5, -7), 1.0 ).permuted( permutation );
	
	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( samples.getSample( i, 0 ).depth, 6 - position.y() );
		EXPECT_EQ( samples.getSample( i, 1 ).depth, position.y() + 6 );

		EXPECT_EQ( samples.getSample( i, 2 ).depth, 8 - position.z() );
		EXPECT_EQ( samples.getSample( i, 3 ).depth, position.z() + 8 );

		EXPECT_EQ( samples.getSample( i, 4 ).depth, 4 - position.x() );
		EXPECT_EQ( samples.getSample( i, 5 ).depth, position.x() + 4 );
	}
}

TEST( Sampler, nonUnitGridCubeTest ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f::Constant(16), false, true );
	scene.end();

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i::Constant(7), Vector3f::Constant(-6), 2.0 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( samples.getSample( i, 0 ).depth, 8 - position.z() );
		EXPECT_EQ( samples.getSample( i, 1 ).depth, position.z() + 8 );

		EXPECT_EQ( samples.getSample( i, 2 ).depth, 8 - position.x() );
		EXPECT_EQ( samples.getSample( i, 3 ).depth, position.x() + 8 );

		EXPECT_EQ( samples.getSample( i, 4 ).depth, 8 - position.y() );
		EXPECT_EQ( samples.getSample( i, 5 ).depth, position.y() + 8 );
	}
}

TEST( Sampler, nonUnitGridCubeTest2 ) {
	// dummy context for offscreen rendering
	sf::Context context( sf::ContextSettings(), 1, 1 );

	context.setActive( true );
	glewInit();

	DebugRender::CombinedCalls scene;
	scene.begin();
	scene.drawBox( Vector3f::Constant(16), false, true );
	scene.end();

	SimpleOrientedGrid grid = OrientedGrid_from( Vector3i::Constant(31), Vector3f::Constant(-7.5), 0.5 );

	VolumeSampler<> sampler;
	Samples samples;
	sampler.samplesView.samples = &samples;

	sampler.directions[0].push_back( Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() );
	sampler.directions[1].push_back( -Vector3f::UnitX() );
	sampler.directions[2].push_back( Vector3f::UnitY() );
	sampler.directions[2].push_back( -Vector3f::UnitY() );
	sampler.grid = &grid;

	sampler.maxDepth = 32.0;

	sampler.init();
	samples.init( &grid, sampler.numDirections );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(1.f);

	sampler.sample( [&] () { scene.render(); } );

	for( int i = 0 ; i < grid.count ; ++i ) {
		const Vector3f position = grid.getPosition( grid.getIndex3( i ) );

		EXPECT_EQ( samples.getSample( i, 0 ).depth, 8 - position.z() );
		EXPECT_EQ( samples.getSample( i, 1 ).depth, position.z() + 8 );

		EXPECT_EQ( samples.getSample( i, 2 ).depth, 8 - position.x() );
		EXPECT_EQ( samples.getSample( i, 3 ).depth, position.x() + 8 );

		EXPECT_EQ( samples.getSample( i, 4 ).depth, 8 - position.y() );
		EXPECT_EQ( samples.getSample( i, 5 ).depth, position.y() + 8 );
	}
}