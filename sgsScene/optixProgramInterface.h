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
	RT_COUNT
};

struct MaterialInfo {
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

rtDeclareVariable( Ray_Eye, currentRay_eye, rtPayload, );
rtDeclareVariable( Ray_Shadow, currentRay_shadow, rtPayload, );
rtDeclareVariable( float3, geometricNormal, attribute geometricNormal, );
rtDeclareVariable( float3, shadingNormal, attribute shadingNormal, );
rtDeclareVariable( float2, texCoord, attribute texCoord, );

rtDeclareVariable(MaterialInfo, materialInfo, attribute materialInfo, );

rtDeclareVariable(optix::Ray, currentRay, rtCurrentRay, );
rtDeclareVariable(float, t_hit, rtIntersectionDistance, );

rtDeclareVariable(rtObject, rootObject, , );

#define sunDirection make_float3( 0.0, -1.0, -1.0 )
#define sceneEpsilon 0.005f
#define maxDistance RT_DEFAULT_MAX

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
#endif

#if !defined(__CUDACC__)
}
#endif

#endif /*__OPTIXPROGRAMINTERFACE_H__*/