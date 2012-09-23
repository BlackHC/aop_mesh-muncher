#ifndef __OPTIXPROGRAMINTERFACE_H__
#define __OPTIXPROGRAMINTERFACE_H__

#if !defined(__CUDACC__)
namespace OptixProgramInterface {
#endif

//////////////////////////////////////////////////////////////////////////
// shared declarations/definitions
enum RayType {
	RT_EYE,
	RT_SHADOW,
	RT_SELECTION,
	RT_COUNT
};

struct MaterialInfo {
	int objectIndex;
	int modelIndex;

	int textureIndex;
	
	// copied straight from SGSScene::Material
	enum AlphaType {
		AT_NONE,
		AT_MATERIAL, // material only alpha
		AT_TEXTURE, // texture * material 
		AT_ADDITIVE, // additive
		AT_MULTIPLY,
		AT_MULTIPLY_2,
		AT_ALPHATEST // like AT_TEXTURE
	};

	AlphaType alphaType;
	float alpha;
};

struct Probe {
	optix::float3 position;
	optix::float3 direction;
};

struct ProbeContext {
	optix::uchar4 color;
	float distance;
	float hitPercentage;
};

// terrain is -1, -1
// miss is -2, -2
struct SelectionResult {
	enum {
		SELECTION_INDEX_TERRAIN = -2,
		SELECTION_INDEX_MISS = -1,
		SELECTION_INDEX_STACK_OVERFLOW = -3
	};

	int modelIndex;
	int objectIndex;

	optix::float3 hitPosition;
	float hitDistance;
};

//////////////////////////////////////////////////////////////////////////
// CUDA specific declarations/definitions
#if defined(__CUDACC__)

struct MergedTextureInfo {
	int2 offset;
	int2 size;
	int index;
};

struct Ray_Eye {
	float3 color;
	float distance;
};

struct Ray_Shadow {
	float transmittance;
};


typedef SelectionResult Ray_Selection;

rtDeclareVariable( Ray_Eye, currentRay_eye, rtPayload, );
rtDeclareVariable( Ray_Shadow, currentRay_shadow, rtPayload, );
rtDeclareVariable( Ray_Selection, currentRay_selection, rtPayload, );
rtDeclareVariable( float3, geometricNormal, attribute geometricNormal, );
rtDeclareVariable( float3, shadingNormal, attribute shadingNormal, );
rtDeclareVariable( float2, texCoord, attribute texCoord, );

rtDeclareVariable(MaterialInfo, materialInfo, attribute materialInfo, );

rtDeclareVariable(optix::Ray, currentRay, rtCurrentRay, );
rtDeclareVariable(float, t_hit, rtIntersectionDistance, );

rtDeclareVariable(rtObject, rootObject, , );

#define sceneEpsilon 0.005f

rtDeclareVariable( float3, sunDirection, , ) = { 0.0, -1.0, -1.0 };
rtDeclareVariable( float, maxDistance, , ) = RT_DEFAULT_MAX;

// render context
rtDeclareVariable(int, disabledObjectIndex, , );
rtDeclareVariable(int, disabledModelIndex, , );

//////////////////////////////////////////////////////////////////////////
// CUDA helper functions

// lightDirection from light away
__device__ float getDirectionalLightTransmittance( const float3 &position, const float3 &lightDirection ) {
	// cast another ray
	optix::Ray subRay( position, -lightDirection, RT_SHADOW, sceneEpsilon );

	Ray_Shadow subRay_shadow;
	subRay_shadow.transmittance = 1.0f;

	rtTrace(rootObject, subRay, subRay_shadow);

	return subRay_shadow.transmittance;
}


#else
//////////////////////////////////////////////////////////////////////////
// host specific declarations/definitions
struct MergedTextureInfo {
	int offset[2];
	int size[2];
	int index;
	int pad;
};

const char * const rayTypeNamespaces[] = {
	"eye",
	"shadow",
	"selection"
};

enum EntryPoint {
	renderPinholeCameraView,
	sampleProbes,
	selectFromPinholeCamera,
	EP_COUNT
};

const char * const entryPointNamespaces[] = {
	"renderPinholeCameraView",
	"sampleProbes",
	"selectFromPinholeCamera"
};

#endif

#if !defined(__CUDACC__)
}
#endif

#endif /*__OPTIXPROGRAMINTERFACE_H__*/