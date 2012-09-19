
#include <optix_world.h>

using namespace optix;

#include "optixProgramInterface.h"

rtBuffer<MergedTextureInfo> textureInfos;
rtTextureSampler<float4, 2> objectTexture;

// one per primitive
rtBuffer<int> textureIndices;

struct Vertex
{
	float3 position;
	float3 normal;
	float2 uv[2];
};

rtBuffer<Vertex> vertexBuffer;
rtBuffer<int3> indexBuffer; // position indices

__device__ float4 performTextureLookup() {
	MergedTextureInfo &textureInfo = textureInfos[ textureIndex ];

	const float2 wrappedTexCoords = texCoord - floor( texCoord ); 
	const float2 mergedTexCoords = make_float2( textureInfo.offset ) + wrappedTexCoords * make_float2( textureInfo.size );

	return tex2D( objectTexture, mergedTexCoords.x, mergedTexCoords.y );
}

RT_PROGRAM void closestHit() {
	float3 hitPosition = currentRay.origin + t_hit * currentRay.direction;

	float3 worldShadingNormal   = normalize(shadingNormal);
	float3 worldGeometricNormal = normalize(geometricNormal);
	float3 ffnormal = faceforward(worldShadingNormal, -currentRay.direction, worldGeometricNormal);
	
	// actually -sunDirection but we don't need to care because of the abs
	float diffuseAttenuation = abs( dot( ffnormal, sunDirection ) );

	const float4 textureLookup = performTextureLookup();
	
	const float3 litSurfaceColor = diffuseAttenuation * make_float3( textureLookup ) * getDirectionalLightTransmittance( hitPosition, sunDirection );

	if( textureLookup.w < 1.0 ) {
		// cast another ray
		optix::Ray subRay( hitPosition, currentRay.direction, RT_EYE, sceneEpsilon );
		
		Ray_Eye subRay_eye;
		subRay_eye.color = make_float3( 0.0f );

		rtTrace(rootObject, subRay, subRay_eye);

		currentRay_eye.color = litSurfaceColor * textureLookup.w + subRay_eye.color * (1.0f - textureLookup.w);
	}
	else {
		currentRay_eye.color = litSurfaceColor;
	}
}

RT_PROGRAM void closestHitAdditive() {
	float3 hitPosition = currentRay.origin + t_hit * currentRay.direction;

	const float4 textureLookup = performTextureLookup();
	
	// cast another ray
	optix::Ray subRay( hitPosition, currentRay.direction, RT_EYE, sceneEpsilon );
		
	Ray_Eye subRay_eye;
	subRay_eye.color = make_float3( 0.0f );

	rtTrace(rootObject, subRay, subRay_eye);

	currentRay_eye.color = make_float3( textureLookup ) + subRay_eye.color;
}

RT_PROGRAM void anyHit() {
	const float4 textureLookup = performTextureLookup();
	
	currentRay_shadow.transmittance *= 1.0 - textureLookup.w;
	if( currentRay_shadow.transmittance < 0.01 ) {
		rtTerminateRay();
	}
	else {
		// NOTE: this is important, otherwise it wont take into account other possible hit locations
		rtIgnoreIntersection();
	}
}

// additive objects are fully transparent
RT_PROGRAM void anyHitAdditive() {
	rtIgnoreIntersection();
}

RT_PROGRAM void intersect( int primIdx )
{
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

			textureIndex = textureIndices[ primIdx ];
				
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
