#include "sgsInterface.h"
#include "mathUtility.h"
#include "probeGenerator.h"

namespace SGSInterface {
	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes ) {
		const auto &sgsBoundingBox = renderer.getUntransformedInstanceBoundingBox( instanceIndex );
		const auto &sgsTransformation = renderer.getInstanceTransformation( instanceIndex );

		const OBB obb = makeOBB( sgsTransformation, sgsBoundingBox );
		ProbeGenerator::generateInstanceProbes( obb.size, resolution, probes );
		ProbeGenerator::transformProbes( probes, obb.transformation, transformedProbes );
	}

	void World::init( const char *scenePath ) {
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

	void World::renderViewFrame( const View &view ) {
		sceneRenderer.renderShadowmap( view.renderContext );

		glMatrixMode( GL_PROJECTION );
		glLoadMatrix( view.viewerContext.projectionView );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

		sceneRenderer.render( view.viewerContext.projectionView, view.viewerContext.worldViewerPosition, view.renderContext );
	}

	void World::renderOptixViewFrame( const View &view ) {
		optixRenderer.renderPinholeCamera( view.viewerContext, view.renderContext );
	}

	void World::generateProbes( int instanceIndex, float resolution, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes ) {
		SGSInterface::generateProbes( instanceIndex, resolution, sceneRenderer, probes, transformedProbes );
	}

	bool World::selectFromView( const View &view, float xh, float yh, SelectionResult *result ) {
		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( xh, yh ) );

		OptixRenderer::SelectionResults selectionResults;
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, view.viewerContext, view.renderContext );

		if( result ) {
			*result = selectionResults.front();
		}
		return selectionResults.front().hasHit();
	}

}