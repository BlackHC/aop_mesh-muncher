#include "sgsInterface.h"
#include "mathUtility.h"
#include "probeGenerator.h"

#include "autoTimer.h"

using namespace Eigen;

namespace SGSInterface {
	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes ) {
		const auto &sgsBoundingBox = renderer.getUntransformedInstanceBoundingBox( instanceIndex );
		const auto &sgsTransformation = renderer.getInstanceTransformation( instanceIndex );

		const Obb obb = makeOBB( sgsTransformation, sgsBoundingBox );
		ProbeGenerator::generateInstanceProbes( obb.size, resolution, probes );
		ProbeGenerator::transformProbes( probes, obb.transformation, transformedProbes );
	}

	void World::init( const char *scenePath ) {
		AUTO_TIMER_FOR_FUNCTION();

		sceneRenderer.reloadShaders();

		{
			AUTO_TIMER_FOR_FUNCTION( "load scene");
			Serializer::BinaryReader reader( scenePath );
			Serializer::read( reader, scene );
		}
		{
			AUTO_TIMER_FOR_FUNCTION( "process scene");
			const char *cachePath = "scene.sgsRendererCache";
			sceneRenderer.processScene( make_nonallocated_shared( scene ), cachePath );
		}
		{
			AUTO_TIMER_FOR_FUNCTION( "init optix");
			optixRenderer.init( make_nonallocated_shared( sceneRenderer ) );
		}

		// TODO: magic number.. add a constant? [10/9/2012 kirschan2]
		{
			AUTO_TIMER_FOR_FUNCTION( "build scene grid");
			sceneGrid.build( 40.0f );
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

	bool World::selectFromView( const View &view, float xh, float yh, SelectionResult *result, int forceDisabledInstanceIndex ) {
		OptixRenderer::SelectionRays selectionRays;
		selectionRays.push_back( optix::make_float2( xh, yh ) );

		OptixRenderer::SelectionResults selectionResults;
		RenderContext renderContext( view.renderContext );
		if( forceDisabledInstanceIndex >= 0 ) {
			renderContext.disabledInstanceIndex = forceDisabledInstanceIndex;
		}
		optixRenderer.selectFromPinholeCamera( selectionRays, selectionResults, view.viewerContext, renderContext );

		if( result ) {
			*result = selectionResults.front();
		}
		return selectionResults.front().hasHit();
	}

	int World::addInstance( int modelIndex, const Vector3f &center ) {
		Instance instance;
		instance.modelId = modelIndex;
		instance.transformation = Translation3f( center );
		return sceneRenderer.addInstance( instance );
	}

	void World::removeInstance( int instanceIndex ) {
		sceneRenderer.removeInstance( instanceIndex );
	}

	void SceneGrid::build( float resolution ) {
		const auto &sceneBoundingBox = renderer.sceneBoundingBox;
		mapping = createCenteredIndexMapping( resolution, sceneBoundingBox.sizes(), sceneBoundingBox.center() );
		instanceGrid.clear();
		instanceGrid.resize( mapping.count );

#if 0
		const Vector3f diagonal = Vector3f::Constant( 1.0 );
#endif
		const int numInstances = renderer.getNumInstances();
		for( int instanceIndex = 0 ; instanceIndex < numInstances ; ++instanceIndex ) {
#if 0
			// this adds the instanceIndex to all bounding boxes that might contain its bbox
			auto boundingSphere = renderer.getUntransformedInstanceBoundingSphere( instanceIndex );
			boundingSphere.center = renderer.getInstanceTransformation( instanceIndex ) * boundingSphere.center;
			VolumeIterator3 iter(
				floor( mapping.getIndex3( boundingSphere.center - diagonal * boundingSphere.radius ) ),
				ceil( mapping.getIndex3( boundingSphere.center - diagonal * boundingSphere.radius ) + Vector3f::Constant( 1.0 ) )
			);
			for( ; iter.hasMore() ; ++iter ) {
				const auto &index3 = iter.getIndex3();
				if( mapping.isValid( index3 ) ) { 
					const int index = mapping.getIndex( index3 );
					instanceGrid[ index ].push_back( instanceIndex );
				}
			}
#else
			// this just adds the origin of the instance to the cell it belongs into
			const auto index3 = floor( Vector3f::Constant( 0.5f ) + mapping.getIndex3( renderer.getInstanceTransformation( instanceIndex ).translation() ) );
			if( mapping.isValid( index3 ) ) {
				instanceGrid[ mapping.getIndex( index3 ) ].push_back( instanceIndex );
			}
			else {
				__debugbreak();
			}
#endif
		}
	}

	// TODO: createCenteredIndexMapping creates a mess because I have to add an offset of 0.5 everywhere [10/9/2012 kirschan2]
	SGSInterface::SceneGrid::QueryResults SceneGrid::query( int disableModelIndex, int disabledInstanceIndex, const Vector3f &position, float radius ) {
		QueryResults results;

		const Vector3f diagonal = Vector3f::Constant( 1.0 );
		const Vector3f minCorner = position - radius * diagonal;
		const Vector3f maxCorner = position + radius * diagonal;

		const Vector3i beginIndex3 = mapping.clampIndex3( floor( Vector3f::Constant( 0.5f ) + mapping.getIndex3( minCorner ) ) );
		const Vector3i endIndex3 = mapping.clampIndex3( ceil( Vector3f::Constant( 0.5f ) + mapping.getIndex3( maxCorner ) + diagonal ) );
		for( auto iter = mapping.getSubIterator( beginIndex3, endIndex3 ) ; iter.hasMore() ; ++iter ) {
			const InstanceIndices &instanceIndices = instanceGrid[ iter.getIndex() ];

			for( auto instanceIndex = instanceIndices.begin() ; instanceIndex != instanceIndices.end() ; ++instanceIndex ) {
				const int modelIndex = renderer.getModelIndex( *instanceIndex );
				if( 
					*instanceIndex == disabledInstanceIndex ||
					modelIndex == disableModelIndex
				) {
						continue;
				}

				const float distance = (renderer.getInstanceTransformation( *instanceIndex ).translation() - position).norm();
				if( distance <= radius ) {
					results.emplace_back( std::make_pair( modelIndex, distance ) );
				}
			}
		}

		return results;
	}
}