#pragma once

#include "optixProgramInterface.h"
#include "sgsSceneRenderer.h"
#include "camera.h"
#include "optixRenderer.h"
#include "grid.h"

#include "make_nonallocated_shared.h"

namespace SGSInterface {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::SelectionResult SelectionResult;

	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes );
	
	struct View {
		ViewerContext viewerContext;
		RenderContext renderContext;
		// world to view rotation (axes as rows)
		Eigen::Matrix3f viewAxes;

		void updateFromCamera( const Camera &camera ) {
			viewerContext.projectionView = camera.getProjectionMatrix() * camera.getViewTransformation().matrix();
			viewerContext.worldViewerPosition = camera.getPosition();
			viewAxes = camera.getViewRotation();
		}
	};



	// allows for queries of object centers in a certain distance
	struct SceneGrid {
		const SGSSceneRenderer &renderer;

		typedef std::vector< int > InstanceIndices;
		// modelIndex, distance
		typedef std::vector< std::pair< int, float > > QueryResults;

		/*void load();
		void store();*/

		SceneGrid( const SGSSceneRenderer &renderer ) : renderer( renderer ) {}

		void build( float resolution );

		QueryResults query(
			int disableModelIndex,
			int disabledInstanceIndex,
			const Eigen::Vector3f &position,
			float radius
		);

	private:
		SimpleIndexMapping3 mapping;
		std::vector< InstanceIndices > instanceGrid;
	};

	struct World {
		SGSScene scene;
		SGSSceneRenderer sceneRenderer;
		OptixRenderer optixRenderer;
		SceneGrid sceneGrid;

		World() : sceneGrid( sceneRenderer ) {}

		void init( const char *scenePath);
		void renderViewFrame( const View &view );
		void renderOptixViewFrame( const View &view );
		// TODO: remove this function again [10/14/2012 kirschan2]
		void generateProbes( int instanceIndex, float resolution, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes );
		bool selectFromView( const View &view, float xh, float yh, SelectionResult *result, int forceDisabledInstanceIndex = -1 );

		int addInstance( int modelIndex, const Eigen::Vector3f &center );
		void removeInstance( int instanceIndex );
	};
}