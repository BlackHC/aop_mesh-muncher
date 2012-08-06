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
	scene.drawBox( Vector3f::Constant(8), false, true );
	scene.end();

	OrientedGrid grid = OrientedGrid::from( Vector3i::Constant(7), Vector3f::Constant(-3), 1.0 );
	grid.transformation = Eigen::AngleAxisf( M_PI / 4, Vector3f::UnitY() ) * grid.transformation;

	DepthSampler sampler;

	sampler.directions[0].push_back( Vector3f::UnitX() + Vector3f::UnitZ() );
	sampler.directions[0].push_back( -Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( Vector3f::UnitX() - Vector3f::UnitZ() );
	sampler.directions[1].push_back( -Vector3f::UnitX() + Vector3f::UnitZ() );
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
		const Vector3i position = grid.getIndex3( i );
		
		EXPECT_EQ( sampler.depthSamples.getSample( i, 0 ), 7 - position.z() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 1 ), position.z() + 1 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 2 ), 7 - position.x() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 3 ), position.x() + 1 );

		EXPECT_EQ( sampler.depthSamples.getSample( i, 4 ), 7 - position.y() );
		EXPECT_EQ( sampler.depthSamples.getSample( i, 5 ), position.y() + 1 );
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