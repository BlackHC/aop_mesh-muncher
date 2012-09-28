#pragma once

#include "optixProgramInterface.h"
#include "sgsSceneRenderer.h"
#include "camera.h"
#include "optixRenderer.h"

#include "make_nonallocated_shared.h"
#include <boost/timer/timer.hpp>

namespace SGSInterface {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::SelectionResult SelectionResult;

	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes );
	
	struct View {
		ViewerContext viewerContext;
		RenderContext renderContext;

		void updateFromCamera( const Camera &camera ) {
			viewerContext.projectionView = camera.getProjectionMatrix() * camera.getViewTransformation().matrix();
			viewerContext.worldViewerPosition = camera.getPosition();
		}
	};

	struct World {
		SGSScene scene;
		SGSSceneRenderer sceneRenderer;
		OptixRenderer optixRenderer;

		void init( const char *scenePath) {
			boost::timer::auto_cpu_timer timer( "World::init: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

			sceneRenderer.reloadShaders();

			{
				boost::timer::auto_cpu_timer timer( "World::init, load scene: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );
				Serializer::BinaryReader reader( scenePath );
				Serializer::read( reader, scene );
			}
			{
				boost::timer::auto_cpu_timer timer( "World::init, process scene: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );
				const char *cachePath = "scene.sgsRendererCache";
				sceneRenderer.processScene( make_nonallocated_shared( scene ), cachePath );
			}
			{
				boost::timer::auto_cpu_timer timer( "World::init, init optix: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );
				optixRenderer.init( make_nonallocated_shared( sceneRenderer ) );
			}		
		}

		void renderViewFrame( const View &view ) {
			sceneRenderer.renderShadowmap( view.renderContext );

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glMatrixMode( GL_PROJECTION );
			glLoadMatrix( view.viewerContext.projectionView );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			sceneRenderer.render( view.viewerContext.projectionView, view.viewerContext.worldViewerPosition, view.renderContext );
		}

		void renderOptixViewFrame( const View &view ) {
			optixRenderer.renderPinholeCamera( view.viewerContext, view.renderContext );
		}

		void generateProbes( int instanceIndex, float resolution, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes ) {
			SGSInterface::generateProbes( instanceIndex, resolution, sceneRenderer, probes, transformedProbes );
		}
	};
}