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
		Eigen::Matrix3f viewAxes;

		void updateFromCamera( const Camera &camera ) {
			viewerContext.projectionView = camera.getProjectionMatrix() * camera.getViewTransformation().matrix();
			viewerContext.worldViewerPosition = camera.getPosition();
			viewAxes = camera.getViewRotation();
		}
	};

	struct World {
		SGSScene scene;
		SGSSceneRenderer sceneRenderer;
		OptixRenderer optixRenderer;

		void init( const char *scenePath);
		void renderViewFrame( const View &view );
		void renderOptixViewFrame( const View &view );
		void generateProbes( int instanceIndex, float resolution, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes );
		bool selectFromView( const View &view, float xh, float yh, SelectionResult *result );
	};
}