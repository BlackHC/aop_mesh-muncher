#ifndef __OPTIXPROGRAMINTERFACE_H__
#define __OPTIXPROGRAMINTERFACE_H__

#include <optix_world.h>

#if !defined(__CUDACC__)
#	include <vector>

namespace OptixProgramInterface {
#	define HOSTORDEVICE
#else
#	define HOSTORDEVICE __device__
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

	optix::float3 diffuse;
};

struct TransformedProbe {
	optix::float3 position;
	optix::float3 direction;
};

struct ProbeSample {
	// in CIELAB space
	// http://robotics.stanford.edu/~ruzon/software/rgblab.html
	// CIELAB values range as follows: L lies between 0 and 100, and a and b lie between -110 and 110
	optix::char3 colorLab;
	unsigned char occlusion;
	float distance;
};

// terrain is -1, -1
// miss is -2, -2
struct SelectionResult {
	enum : signed int {
		SELECTION_INDEX_TERRAIN = -1,
		SELECTION_INDEX_MISS = -2,
		SELECTION_INDEX_STACK_OVERFLOW = -3
	};

	int modelIndex;
	int objectIndex;

	optix::float3 hitPosition;
	optix::float3 hitNormal;

	float hitDistance;

	bool hasHit() {
		return objectIndex >= SELECTION_INDEX_TERRAIN;
	}
};

#define _numProbeSamples 127

// this should go into its own file
namespace CIELAB {
	using namespace optix;

	HOSTORDEVICE const float3 D65white = { 0.95047f,1.0f,1.08883f };
	HOSTORDEVICE const float rgb2XYZ[] = {
		0.4124f, 0.3576f, 0.1805f,
		0.2126f, 0.7152f, 0.0722f,
		0.0193f, 0.1192f, 0.9505f
	};

	HOSTORDEVICE const float XYZ2rgb[] = {
		3.24062514f, -1.53720784f, -0.498628557f,
		-0.968930602f,  1.87575603f, 0.0415175036f,
		0.0557101034f, -0.204021037f, 1.05699599f
	};

	inline HOSTORDEVICE const float f( const float v ) {
		const float threshold = 6.0f / 29.0f;
		if( v > threshold*threshold*threshold ) {
			return powf( v, 1.0f/3.0f );
		}
		else {
			return v / (3.0f * threshold * threshold) + 4.0f / 29.0f;
		}
	}

	inline HOSTORDEVICE const float inv_f( const float v ) {
		const float threshold = 6.0f / 29.0f;
		if( v > threshold ) {
			return v*v*v;
		}
		else {
			return 3.0f * threshold * threshold * (v - 4.0 / 29.0);
		}
	}

	inline HOSTORDEVICE float3 fromRGB( const float3 &rgb ) {
		const float3 XYZ = *reinterpret_cast<const Matrix3x3*>(rgb2XYZ) * rgb;

		// I'm leaving out the linear small value correction
		const float3 transformedXYZ = {
			f( XYZ.x / D65white.x ),
			f( XYZ.y / D65white.y ),
			f( XYZ.z / D65white.z )
		};

		const float3 Lab = {
			116.0f * transformedXYZ.y - 16.0f,
			500.0f * (transformedXYZ.x - transformedXYZ.y),
			200.0f * (transformedXYZ.y - transformedXYZ.z),
		};

		return Lab;
	}

	inline HOSTORDEVICE float3 toRGB( const float3 &Lab ) {
		float3 transformedXYZ;
		transformedXYZ.y = (Lab.x + 16.0f) / 116.0f;

		transformedXYZ.x = transformedXYZ.y + Lab.y / 500.0f;
		transformedXYZ.z = transformedXYZ.y - Lab.z / 200.0f;

		// again I'm leaving out the linear small value correction
		const float3 XYZ = {
			inv_f( transformedXYZ.x ) * D65white.x,
			inv_f( transformedXYZ.y ) * D65white.y,
			inv_f( transformedXYZ.z ) * D65white.z
		};

		const float3 rgb = *reinterpret_cast<const Matrix3x3*>(XYZ2rgb) * XYZ;

		return rgb;
	}
}

//////////////////////////////////////////////////////////////////////////
// CUDA specific declarations/definitions
#if defined(__CUDACC__)

// TODO: merge this somehow back into mathUtility.h [10/28/2012 Andreas]
const optix::char3 neighborOffsets[] = {
	// first z, then x, then y
	// z
	{ 0, 0, 1 }, { 0, 0, -1 }, { -1, 0, -1 },
	{ 1, 0, -1 }, { -1, 0, 1 }, { 1, 0, 1 },
	{ 0, 1, -1 }, { 0, -1, -1 }, { -1, 1, -1 },
	// x
	{ 1, 0, 0 }, { -1, 0, 0 }, { 1, 1, -1 },
	{ 1, -1, 1 }, { 1, 1, 1 }, { 1, -1, -1 },
	{ -1, 1, 0 }, { 1, 1, 0 }, { -1, -1, -1 },
	// y
	{ 0, 1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { 1, -1, 0 },
	{ -1, -1, 1 }, { 0, -1, 1 }, { -1, 1, 1 }, { 0, 1, 1 }
};

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

rtDeclareVariable( float3, sunDirection, , ) = { -1.0, -1.0, -1.0 };
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

typedef std::vector< TransformedProbe > TransformedProbes;
typedef std::vector< ProbeSample > ProbeSamples;

#endif

#if !defined(__CUDACC__)
}
#endif

#endif /*__OPTIXPROGRAMINTERFACE_H__*/