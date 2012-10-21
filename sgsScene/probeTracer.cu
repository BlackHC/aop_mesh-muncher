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

__device__ __inline__ char3 make_Lab(const float3& floatLab)
{
	return make_char3( static_cast<signed char>(floatLab.x),  /* L */
						static_cast<signed char>(floatLab.y), /* a */
						static_cast<signed char>(floatLab.z) ); /* b */
}

rtDeclareVariable( uint, probeIndex, rtLaunchIndex, );
rtDeclareVariable( uint, numProbes, rtLaunchDim, );
rtDeclareVariable( uint, sampleOffset, , );

#define numHemisphereSamples 39939
rtBuffer<float3> hemisphereSamples;

rtBuffer<Probe> probes;
rtBuffer<ProbeContext> probeContexts;

RT_PROGRAM void sampleProbes() {
	Probe probe = probes[ probeIndex ];

#if 1
	Onb onb( probe.direction );

	//rtPrintf( "%f", dot( onb.m_normal, cross( onb.m_tangent, onb.m_binormal ) ) );

	uint sampleStartIndex = 
			sampleOffset * numProbes * numProbeSamples 
		+ 
			numProbeSamples * probeIndex 
		+ 
			numProbes * 1979
	;

	float avgDistance = 0.0f;
	float3 avgColor = make_float3( 0.0f );
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
			avgDistance += ray_eye.distance;
			avgColor += ray_eye.color;
		}
	}
#else
	float avgDistance = 0.0f;
	float3 avgColor = make_float3( 0.0f );
	int numHits = 0;

	{
		Ray ray( probe.position, probe.direction, RT_EYE, sceneEpsilon, maxDistance );

		Ray_Eye ray_eye;
		rtTrace( rootObject, ray, ray_eye );

		if( ray_eye.distance < maxDistance ) {
			++numHits;
			avgDistance += ray_eye.distance;
			avgColor += ray_eye.color;
		}
	}
#endif

	ProbeContext &context = probeContexts[ probeIndex ];
	if( numHits ) {
		avgDistance = avgDistance / numHits;
		avgColor = avgColor / numHits;
	}

	// convert to cielab
	const float3 Lab = CIELAB::fromRGB( avgColor );
	context.Lab = make_Lab( Lab );
	context.distance = avgDistance;
	context.hitCounter = numHits;
}

RT_PROGRAM void sampleProbes_exception() {
	unsigned int const error_code = rtGetExceptionCode();
	if(RT_EXCEPTION_STACK_OVERFLOW == error_code) {
		probeContexts[ probeIndex ].Lab = make_char3( -127, -127, -127 );
		probeContexts[ probeIndex ].hitCounter = 0;
		probeContexts[ probeIndex ].distance = 1.0f;
	} else {
		rtPrintExceptionDetails();
	}
}