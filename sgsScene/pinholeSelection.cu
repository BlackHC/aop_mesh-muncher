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

rtDeclareVariable(uint, selectionRayIndex, rtLaunchIndex, );
rtDeclareVariable(uint, numSelectionRays, rtLaunchDim, );
rtBuffer<SelectionResult> selectionResults;
rtBuffer<float2> selectionRays;

// Camera Params:
rtDeclareVariable(float3, eyePosition, , );
rtDeclareVariable(float3, U, , );
rtDeclareVariable(float3, V, , );
rtDeclareVariable(float3, W, , );



RT_PROGRAM void selectFromPinholeCamera()
{
	float2 selectionRayDirection = selectionRays[ selectionRayIndex ];

	float3 ray_origin = eyePosition;
	float3 ray_direction = normalize(selectionRayDirection.x*U + selectionRayDirection.y*V + W);
	
	optix::Ray ray( ray_origin, ray_direction, RT_SELECTION, sceneEpsilon );
	
	Ray_Selection ray_selection;

	rtTrace( rootObject, ray, ray_selection );

	selectionResults[ selectionRayIndex ] = ray_selection;
}

RT_PROGRAM void selectFromPinholeCamera_exception() {
	unsigned int const error_code = rtGetExceptionCode();
	if(RT_EXCEPTION_STACK_OVERFLOW == error_code) {
		selectionResults[ selectionRayIndex ].modelIndex = selectionResults[ selectionRayIndex ].objectIndex = SelectionResult::SELECTION_INDEX_STACK_OVERFLOW;
	} else {
		rtPrintExceptionDetails();
	}
}

RT_PROGRAM void selection_miss() {
	currentRay_selection.modelIndex = currentRay_selection.objectIndex = SelectionResult::SELECTION_INDEX_MISS;
}