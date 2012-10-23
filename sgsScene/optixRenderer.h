#pragma once
#include <optix_world.h>

#include "rendering.h"
#include <vector>

struct SGSSceneRenderer;

struct OptixRenderer {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::ProbeContext ProbeContext;

	typedef OptixProgramInterface::Probes Probes;
	typedef OptixProgramInterface::ProbeContexts ProbeContexts;

	typedef std::vector< optix::float2 > SelectionRays;

	typedef OptixProgramInterface::SelectionResult SelectionResult;
	typedef std::vector< SelectionResult > SelectionResults;

	static const int numHemisphereSamples = 39989;
	static const int maxNumProbes = 1<<18;
	static const int maxNumSelectionRays = 32;

	optix::Context context;
	optix::Group scene;
	optix::Acceleration acceleration;

	optix::Buffer outputBuffer;
	int width, height;
	ScopedTexture2D debugTexture;

	optix::Buffer probes, probeContexts, hemisphereSamples;

	optix::Buffer selectionRays, selectionResults;

	std::shared_ptr< SGSSceneRenderer > sgsSceneRenderer;

	void init( const std::shared_ptr< SGSSceneRenderer > &sgsSceneRenderer );

	void createHemisphereSamples( optix::float3 *hemisphereSamples );

	void prepareLaunch();

	void compileContext();

	void setRenderContext( const RenderContext &renderContext );
	void setPinholeCameraViewerContext( const ViewerContext &viewerContext );

	void renderPinholeCamera( const ViewerContext &viewerContext, const RenderContext &renderContext );
	void selectFromPinholeCamera(
		const SelectionRays &selectionRays,
		SelectionResults &selectionResults,
		const ViewerContext &viewerContext,
		const RenderContext &renderContext
	);
	void sampleProbes(
		const Probes &probes,
		ProbeContexts &probeContexts,
		const RenderContext &renderContext,
		float maxDistance = RT_DEFAULT_MAX,
		int sampleOffset = 0
	);

	void addSceneChild( const optix::GeometryGroup &child ) {
		int index = scene->getChildCount();
		scene->setChildCount( index + 1 );
		scene->setChild( index, child );
	}
};