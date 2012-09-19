
#include <optix_world.h>

using namespace optix;

struct Ray_Payload 
{
	float4 result;
};

rtDeclareVariable( Ray_Payload, pr_payload, rtPayload, );
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );
rtDeclareVariable( float2, texcoord, attribute texcoord, );
rtDeclareVariable(float3, surface_color, attribute surface_color, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(float,      t_hit, rtIntersectionDistance, );

RT_PROGRAM void closest_hit()
{
	float3 phit    = ray.origin + t_hit * ray.direction;

	float3 world_shading_normal   = normalize(shading_normal);
	float3 world_geometric_normal = normalize(geometric_normal);
	float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);
		
	pr_payload.result = make_float4( make_float3( abs( dot( ffnormal, make_float3( 0.0, 1.0, 1.0 ) ) ) ), 1.0 );
}

RT_PROGRAM void any_hit()
{		
	rtTerminateRay();
}

struct VFormat
{
	float3 position;
	float3 normal;
	float2 uv[2];
};

rtBuffer<VFormat> vertex_buffer;
rtBuffer<int3> index_buffer; // position indices

RT_PROGRAM void intersect( int primIdx )
{
	int3 v_idx = index_buffer[primIdx];

	float3 p0 = vertex_buffer[ v_idx.x ].position;
	float3 p1 = vertex_buffer[ v_idx.y ].position;
	float3 p2 = vertex_buffer[ v_idx.z ].position;

	// Intersect ray with triangle
	float3 n;
	float t, beta, gamma;
	if( intersect_triangle( ray, p0, p1, p2, n, t, beta, gamma ) ) {
		if( rtPotentialIntersection( t ) ) {
			int3 n_idx = index_buffer[ primIdx ];

			float3 n0 = vertex_buffer[ n_idx.x ].normal;
			float3 n1 = vertex_buffer[ n_idx.y ].normal;
			float3 n2 = vertex_buffer[ n_idx.z ].normal;
			shading_normal = normalize( n1*beta + n2*gamma + n0*(1.0f-beta-gamma) );
			geometric_normal = -n;

			int3 t_idx = index_buffer[ primIdx ];
			
			float2 t0 = vertex_buffer[ t_idx.x ].uv[0];
			float2 t1 = vertex_buffer[ t_idx.y ].uv[0];
			float2 t2 = vertex_buffer[ t_idx.z ].uv[0];
			texcoord = ( t1*beta + t2*gamma + t0*(1.0f-beta-gamma) );
				
			/*float3 c0 = vertex_buffer[ t_idx.x ].color;
			float3 c1 = vertex_buffer[ t_idx.y ].color;
			float3 c2 = vertex_buffer[ t_idx.z ].color;*/
			surface_color = make_float3( 1.0 ); //c1*beta + c2*gamma + c0*(1.0f - beta-gamma);

			rtReportIntersection(0);
		}
	}
}

RT_PROGRAM void bounding_box (int primIdx, float result[6])
{
	int3 v_idx = index_buffer[primIdx];

	float3 v0 = vertex_buffer[ v_idx.x ].position;
	float3 v1 = vertex_buffer[ v_idx.y ].position;
	float3 v2 = vertex_buffer[ v_idx.z ].position;

	optix::Aabb* aabb = (optix::Aabb*)result;
	aabb->m_min = fminf( fminf( v0, v1), v2 );
	aabb->m_max = fmaxf( fmaxf( v0, v1), v2 );
}
