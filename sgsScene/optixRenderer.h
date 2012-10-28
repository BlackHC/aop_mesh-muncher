#pragma once
#include <optix_world.h>

#include "rendering.h"
#include <vector>

struct SGSSceneRenderer;

struct OptixRenderer {
	typedef OptixProgramInterface::TransformedProbe TransformedProbe;
	typedef OptixProgramInterface::ProbeSample ProbeSample;

	typedef OptixProgramInterface::TransformedProbes TransformedProbes;
	typedef OptixProgramInterface::ProbeSamples ProbeSamples;

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

	optix::Buffer transformedProbesBuffer, probeSamplesBuffer, hemisphereSamplesBuffer;

	optix::Buffer selectionRaysBuffer, selectionResultsBuffer;

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
		const TransformedProbes &probes,
		ProbeSamples &probeSamples,
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