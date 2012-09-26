#pragma once
#include <optix_world.h>
#include <boost/random.hpp>

#include "rendering.h"

struct SGSSceneRenderer;

struct OptixRenderer {
	typedef OptixProgramInterface::Probe Probe;
	typedef OptixProgramInterface::ProbeContext ProbeContext;

	typedef std::vector< optix::float2 > SelectionRays;

	typedef OptixProgramInterface::SelectionResult SelectionResult;
	typedef std::vector<SelectionResult > SelectionResults;

	static const int numHemisphereSamples = 39989;
	static const int maxNumProbes = 8192;
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

	void createHemisphereSamples( optix::float3 *hemisphereSamples ) {
		// produces randomness out of thin air
		boost::random::mt19937 rng;
		// see pseudo-random number generators
		boost::random::uniform_01<> distribution;

		for( int i = 0 ; i < numHemisphereSamples ; ++i ) {
			const float u1 = distribution(rng) * 0.25f;
			const float u2 = distribution(rng);
			optix::cosine_sample_hemisphere( u1, u2, hemisphereSamples[i] );
		}
	}

	void compileContext();

	void setRenderContext( const RenderContext &renderContext );
	void setPinholeCameraViewerContext( const ViewerContext &viewerContext );

	void renderPinholeCamera( const ViewerContext &viewerContext, const RenderContext &renderContext );
	void selectFromPinholeCamera( const SelectionRays &selectionRays, SelectionResults &selectionResults, const ViewerContext &viewerContext, const RenderContext &renderContext );
	void sampleProbes( const std::vector< Probe > &probes, std::vector< ProbeContext > &probeContexts, const RenderContext &renderContext, float maxDistance = RT_DEFAULT_MAX );

	void addSceneChild( const optix::GeometryGroup &child ) {
		int index = scene->getChildCount();
		scene->setChildCount( index + 1 );
		scene->setChild( index, child );
	}
};