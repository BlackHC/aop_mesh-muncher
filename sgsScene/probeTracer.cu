#include <optix_world.h>

using namespace optix;

#include "optixProgramInterface.h"

__device__ __inline__ uchar4 make_color(const float3& c)
{
	return make_uchar4( static_cast<unsigned char>(__saturatef(c.x)*255.99f),  /* B */
						static_cast<unsigned char>(__saturatef(c.y)*255.99f),  /* G */
						static_cast<unsigned char>(__saturatef(c.z)*255.99f),  /* R */
						255u);                                                 /* A */
}

__device__ __inline__ uchar3 make_rgb(const float3& c)
{
	return make_uchar3( static_cast<unsigned char>(__saturatef(c.x)*255.99f),  /* B */
						static_cast<unsigned char>(__saturatef(c.y)*255.99f),  /* G */
						static_cast<unsigned char>(__saturatef(c.z)*255.99f)); /* R */
}


rtDeclareVariable(uint, probeIndex, rtLaunchIndex, );
rtDeclareVariable(uint, numProbes, rtLaunchDim, );

#define numHemisphereSamples 39939
rtBuffer<float3> hemisphereSamples;

rtBuffer<Probe> probes;
rtBuffer<ProbeContext> probeContexts;

RT_PROGRAM void sampleProbes() {
	Probe probe = probes[ probeIndex ];

	Onb onb( probe.direction );

	//rtPrintf( "%f", dot( onb.m_normal, cross( onb.m_tangent, onb.m_binormal ) ) );

	int sampleStartIndex = numProbeSamples * probeIndex + numProbes * 521;

	float distance = 0.0f;
	float3 color = make_float3( 0.0f );
	int numHits = 0;
	for( int rayIndex = 0 ; rayIndex < numProbeSamples ; ++rayIndex ) {
		const float3 sample = hemisphereSamples[ (sampleStartIndex + rayIndex) % numHemisphereSamples ];
		const float3 rayDirection = onb.m_normal * sample.z + onb.m_tangent * sample.x + onb.m_binormal * sample.y;
		
		Ray ray( probe.position, rayDirection, RT_EYE, sceneEpsilon, maxDistance );

		Ray_Eye ray_eye;
		rtTrace( rootObject, ray, ray_eye );

		// TODO: could weight the probes by sample.z
		if( ray_eye.distance < maxDistance ) {
			++numHits;
			distance += ray_eye.distance;
			color += ray_eye.color;
		}
	}

	ProbeContext &context = probeContexts[ probeIndex ];
	if( numHits ) {
		context.color = make_rgb( color / numHits);
		context.distance = distance / numHits;
	}
	context.hitCounter = numHits;
}

RT_PROGRAM void sampleProbes_exception() {
	unsigned int const error_code = rtGetExceptionCode();
	if(RT_EXCEPTION_STACK_OVERFLOW == error_code) {
		probeContexts[ probeIndex ].color = make_rgb( make_float3( 1.0f, 1.0f, 1.0f ) );
		probeContexts[ probeIndex ].hitCounter = 0;
		probeContexts[ probeIndex ].distance = 1.0f;
	} else {
		rtPrintExceptionDetails();
	}
}