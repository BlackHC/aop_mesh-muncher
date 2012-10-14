#ifndef __OPTIXPROGRAMINTERFACE_H__
#define __OPTIXPROGRAMINTERFACE_H__

#include <optix_world.h>

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
	// in CIELAB space
	// http://robotics.stanford.edu/~ruzon/software/rgblab.html
	// CIELAB values range as follows: L lies between 0 and 100, and a and b lie between -110 and 110
	optix::char3 Lab;
	unsigned char hitCounter;
	float distance;
};

// terrain is -1, -1
// miss is -2, -2
struct SelectionResult {
	enum {
		SELECTION_INDEX_TERRAIN = -1,
		SELECTION_INDEX_MISS = -2,
		SELECTION_INDEX_STACK_OVERFLOW = -3
	};

	int modelIndex;
	int objectIndex;

	// occlusion measure
	optix::float3 hitPosition;

	float hitDistance;

	bool hasHit() {
		return objectIndex >= SELECTION_INDEX_TERRAIN;
	}
};

#define _numProbeSamples 31

//////////////////////////////////////////////////////////////////////////
// CUDA specific declarations/definitions
#if defined(__CUDACC__)

struct MergedTextureInfo {
	int2 offset;
	int2 size;
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

#define numProbeSamples _numProbeSamples

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

// this should go into its own file
namespace CIELAB {
	__device__ const float3 D65white = { 0.95047f,1.0f,1.08883f };
	__device__ const float rgb2XYZ[] = {
		0.4124f, 0.3576f, 0.1805f,
		0.2126f, 0.7152f, 0.0722f,
		0.0193f, 0.1192f, 0.9505f
	};

	__device__ const float XYZ2rgb[] = {
		3.24062514f, -1.53720784f, -0.498628557f,
		-0.968930602f,  1.87575603f, 0.0415175036f,
		0.0557101034f, -0.204021037f, 1.05699599f
	};

	__device__ float3 fromRGB( const float3 &rgb ) {
		const float3 XYZ = *reinterpret_cast<const Matrix3x3*>(rgb2XYZ) * rgb;

		// I'm leaving out the linear small value correction
		const float3 transformedXYZ = {
			powf( XYZ.x / D65white.x, 1.f/3.f),
			powf( XYZ.y / D65white.y, 1.f/3.f),
			powf( XYZ.z / D65white.z, 1.f/3.f)
		};

		const float3 Lab = {
			116.0 * transformedXYZ.y - 16.0,
			500.0 * (transformedXYZ.x - transformedXYZ.y),
			200.0 * (transformedXYZ.y - transformedXYZ.z),
		};

		return Lab;
	}

	__device__ float3 toRGB( const float3 &Lab ) {
		float3 transformedXYZ;
		transformedXYZ.y = (Lab.x + 16.0) / 116.0;

		transformedXYZ.x = transformedXYZ.y + Lab.y / 500.0;
		transformedXYZ.z = transformedXYZ.y - Lab.z / 200.0;

		// again I'm leaving out the linear small value correction
		const float3 XYZ = {
			powf( transformedXYZ.x, 3.0f ) * D65white.x,
			powf( transformedXYZ.y, 3.0f ) * D65white.y,
			powf( transformedXYZ.z, 3.0f ) * D65white.z
		};

		const float3 rgb = *reinterpret_cast<const Matrix3x3*>(XYZ2rgb) * XYZ;

		return rgb;
	}
}

#else
//////////////////////////////////////////////////////////////////////////
// host specific declarations/definitions
struct MergedTextureInfo {
	int offset[2];
	int size[2];
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

const int numProbeSamples = _numProbeSamples;
#undef _numProbeSamples

#endif

#if !defined(__CUDACC__)
}
#endif

#endif /*__OPTIXPROGRAMINTERFACE_H__*/