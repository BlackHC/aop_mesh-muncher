
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

rtDeclareVariable(uint2, launchIndex, rtLaunchIndex, );
rtDeclareVariable(uint2, launchDim, rtLaunchDim, );
rtBuffer<uchar4, 2>	 outputBuffer;

// Camera Params:
rtDeclareVariable(float3, eyePosition, , );
rtDeclareVariable(float3, U, , );
rtDeclareVariable(float3, V, , );
rtDeclareVariable(float3, W, , );

RT_PROGRAM void pinholeCamera_rayGeneration()
{
	float2 d = (make_float2(launchIndex) + make_float2(0.5f, 0.5f)) / make_float2(launchDim) * 2.0f - 1.0f;;

	float3 ray_origin = eyePosition;
	float3 ray_direction = normalize(d.x*U + d.y*V + W);
	
	optix::Ray ray( ray_origin, ray_direction, RT_EYE, sceneEpsilon );
	
	Ray_Eye ray_eye;
	ray_eye.color = make_float3( 0.0f );

	rtTrace( rootObject, ray, ray_eye );
		
	outputBuffer[launchIndex] = make_color( ray_eye.color );
}

RT_PROGRAM void exception() {
	unsigned int const error_code = rtGetExceptionCode();
	if(RT_EXCEPTION_STACK_OVERFLOW == error_code) {
		outputBuffer[launchIndex] = make_uchar4(255, 0, 0, 255);
	} else {
		rtPrintExceptionDetails();
	}
}

RT_PROGRAM void miss() {
	currentRay_eye.color = make_float3( 0 );
	currentRay_eye.distance = maxDistance;
}