
#include <optix_world.h>

using namespace optix;

#include "optixProgramInterface.h"

rtBuffer<MergedTextureInfo> textureInfos;
rtTextureSampler<float4, 2> objectTexture;

// one per primitive
rtBuffer<MaterialInfo> materialInfos;
rtBuffer<int> materialIndices;

struct Vertex
{
	float3 position;
	float3 normal;
	float2 uv[1];
};

rtBuffer<Vertex> vertexBuffer;
rtBuffer<int3> indexBuffer; // position indices

__device__ float4 getTexel() {
	MergedTextureInfo &textureInfo = textureInfos[ materialInfo.textureIndex ];

	const float2 wrappedTexCoords = texCoord - floor( texCoord ); 
	const float2 mergedTexCoords = make_float2( textureInfo.offset ) + wrappedTexCoords * make_float2( textureInfo.size );

	return tex2D( objectTexture, mergedTexCoords.x, mergedTexCoords.y );
}

__device__ float3 subTrace( const float3 &position, const float3 &direction, bool earlyOut ) {
	if( earlyOut ) {
		return make_float3( 0.0 );
	}

	optix::Ray subRay( position, direction, RT_EYE, sceneEpsilon );
		
	Ray_Eye subRay_eye;
	subRay_eye.color = make_float3( 0.0f );

	rtTrace(rootObject, subRay, subRay_eye);
	
	return subRay_eye.color;
}

RT_PROGRAM void eye_closestHit() {
	float3 hitPosition = currentRay.origin + t_hit * currentRay.direction;

	currentRay_eye.distance = t_hit;

	float3 worldShadingNormal   = normalize(shadingNormal);
	float3 worldGeometricNormal = normalize(geometricNormal);
	float3 ffnormal = faceforward(worldShadingNormal, -currentRay.direction, worldGeometricNormal);
	
	// actually -sunDirection but we don't need to care because of the abs
	float diffuseAttenuation = abs( dot( ffnormal, sunDirection ) );

	const float4 diffuseColor = getTexel();
	
	const float3 litSurfaceColor = make_float3( diffuseColor ) * 
		(0.2 + 0.8 * diffuseAttenuation * getDirectionalLightTransmittance( hitPosition, sunDirection ));

	switch( materialInfo.alphaType ) {
	default:
	case MaterialInfo::AT_NONE:
		currentRay_eye.color = litSurfaceColor;
		break;
	case MaterialInfo::AT_ADDITIVE:
		currentRay_eye.color = make_float3( diffuseColor ) + subTrace( hitPosition, currentRay.direction, false );
		break;
	case MaterialInfo::AT_MATERIAL: {
		const float alpha = materialInfo.alpha;
		currentRay_eye.color = litSurfaceColor * alpha + subTrace( hitPosition, currentRay.direction, alpha > 0.99f ) * (1.0f - alpha);
		break;
	}
	case MaterialInfo::AT_TEXTURE:
	case MaterialInfo::AT_ALPHATEST:
		currentRay_eye.color = litSurfaceColor * diffuseColor.w + subTrace( hitPosition, currentRay.direction, diffuseColor.w > 0.99f ) * (1.0f - diffuseColor.w);
		break;
	case MaterialInfo::AT_MULTIPLY:
		currentRay_eye.color = make_float3( diffuseColor ) * subTrace( hitPosition, currentRay.direction, false );
		break;
	case MaterialInfo::AT_MULTIPLY_2:
		currentRay_eye.color = make_float3( diffuseColor ) * subTrace( hitPosition, currentRay.direction, false ) * 2;
		break;
	}
}

#if 0
// could be used to avoid subtraces
RT_PROGRAM void eye_anyHit() {
	if( getTexel().w < 0.05 ) {
		rtIgnoreIntersection();
	}
}
#endif

RT_PROGRAM void shadow_anyHit() {
	switch( materialInfo.alphaType ) {
	default:
	case MaterialInfo::AT_NONE:
		currentRay_shadow.transmittance = 0.0;
		rtTerminateRay();
		return;
	case MaterialInfo::AT_ADDITIVE:
		rtIgnoreIntersection();
		return;
	case MaterialInfo::AT_MATERIAL:
		currentRay_shadow.transmittance *= 1.0 - materialInfo.alpha;
		break;
	case MaterialInfo::AT_ALPHATEST:
	case MaterialInfo::AT_TEXTURE:
		currentRay_shadow.transmittance *= 1.0 - getTexel().w;
		break;
	}

	if( currentRay_shadow.transmittance < 0.01 ) {
		rtTerminateRay();
	}
	else {
		// NOTE: this is important, otherwise it wont take into account other possible hit locations
		rtIgnoreIntersection();
	}
}

RT_PROGRAM void selection_anyHit() {
	float alpha = 1.0;
	switch( materialInfo.alphaType ) {
	default:
	case MaterialInfo::AT_NONE:
		return;
	case MaterialInfo::AT_ADDITIVE:
		return;
	case MaterialInfo::AT_MATERIAL:
		alpha = materialInfo.alpha;
		break;
	case MaterialInfo::AT_ALPHATEST:
	case MaterialInfo::AT_TEXTURE:
		alpha = getTexel().w;
		break;
	}

	if( alpha < 0.1 ) {
		rtIgnoreIntersection();
	}
}

RT_PROGRAM void selection_closestHit() {
	currentRay_selection.objectIndex = materialInfo.objectIndex;
	currentRay_selection.modelIndex = materialInfo.modelIndex;

	currentRay_selection.hitPosition = currentRay.origin + t_hit * currentRay.direction;
	currentRay_selection.hitDistance = t_hit;
}

RT_PROGRAM void intersect( int primIdx )
{
	MaterialInfo &minfo = materialInfos[ materialIndices[ primIdx ] ];
	if( minfo.objectIndex == disabledObjectIndex || minfo.modelIndex == disabledModelIndex ) {
		return;
	}

	int3 v_idx = indexBuffer[primIdx];

	float3 p0 = vertexBuffer[ v_idx.x ].position;
	float3 p1 = vertexBuffer[ v_idx.y ].position;
	float3 p2 = vertexBuffer[ v_idx.z ].position;

	// Intersect ray with triangle
	float3 n;
	float t, beta, gamma;
	if( intersect_triangle( currentRay, p0, p1, p2, n, t, beta, gamma ) ) {
		if( rtPotentialIntersection( t ) ) {
			int3 n_idx = indexBuffer[ primIdx ];

			float3 n0 = vertexBuffer[ n_idx.x ].normal;
			float3 n1 = vertexBuffer[ n_idx.y ].normal;
			float3 n2 = vertexBuffer[ n_idx.z ].normal;
			shadingNormal = normalize( n1*beta + n2*gamma + n0*(1.0f-beta-gamma) );
			geometricNormal = -n;

			int3 t_idx = indexBuffer[ primIdx ];
			
			float2 t0 = vertexBuffer[ t_idx.x ].uv[0];
			float2 t1 = vertexBuffer[ t_idx.y ].uv[0];
			float2 t2 = vertexBuffer[ t_idx.z ].uv[0];
			texCoord = ( t1*beta + t2*gamma + t0*(1.0f-beta-gamma) );

			materialInfo = minfo;

			rtReportIntersection(0);
		}
	}
}

RT_PROGRAM void calculateBoundingBox (int primIdx, float result[6]) {
	int3 v_idx = indexBuffer[primIdx];

	float3 v0 = vertexBuffer[ v_idx.x ].position;
	float3 v1 = vertexBuffer[ v_idx.y ].position;
	float3 v2 = vertexBuffer[ v_idx.z ].position;

	optix::Aabb* aabb = (optix::Aabb*)result;
	aabb->m_min = fminf( fminf( v0, v1), v2 );
	aabb->m_max = fmaxf( fmaxf( v0, v1), v2 );
}
