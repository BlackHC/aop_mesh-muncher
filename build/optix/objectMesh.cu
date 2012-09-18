
#include <optix_world.h>

using namespace optix;

__device__ __inline__ uchar4 make_color(const float3& c)
{
    return make_uchar4( static_cast<unsigned char>(__saturatef(c.x)*255.99f),  /* B */
                        static_cast<unsigned char>(__saturatef(c.y)*255.99f),  /* G */
                        static_cast<unsigned char>(__saturatef(c.z)*255.99f),  /* R */
                        255u);                                                 /* A */
}

struct Ray_Payload 
{
	float4 result;
};

rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, launch_dim, rtLaunchDim, );
rtBuffer<uchar4, 2>	 result_buffer;
rtDeclareVariable(rtObject, top_object, , );

// lights
// rtBuffer<float3> LightPosition;
// rtBuffer<float4> LightColor;

// Camera Params:
rtDeclareVariable(float3, eye, , );
rtDeclareVariable(float3, U, , );
rtDeclareVariable(float3, V, , );
rtDeclareVariable(float3, W, , );


RT_PROGRAM void ray_gen()
{
	float2 d = (make_float2(launch_index) + make_float2(0.5f, 0.5f)) / make_float2(launch_dim) * 2.0f - 1.0f;;
	result_buffer[launch_index] = make_uchar4 (0, 0, 0, 255);

	float3 ray_origin = eye;
	float3 ray_direction = normalize(d.x*U + d.y*V + W);
    
	optix::Ray ray = optix::make_Ray(ray_origin, ray_direction, 0, 0.05f, RT_DEFAULT_MAX);
    
	Ray_Payload payload;
	payload.result = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	rtTrace(top_object, ray, payload);
	    
	result_buffer[launch_index] = make_color( make_float3( payload.result ) );
}

RT_PROGRAM void exception()
{
	unsigned int const error_code = rtGetExceptionCode();
	if(RT_EXCEPTION_STACK_OVERFLOW == error_code) {
		result_buffer[launch_index] = make_uchar4(255, 0, 0, 255);
	} else {
		rtPrintExceptionDetails();
	}
}

rtDeclareVariable( Ray_Payload, pr_payload, rtPayload, );
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );
rtDeclareVariable( float2, texcoord, attribute texcoord, );
rtDeclareVariable(float3, surface_color, attribute surface_color, );

struct PerRayData_occlusion
{
  float occlusion;
};

rtDeclareVariable(PerRayData_occlusion, prd_occlusion, rtPayload, );

rtDeclareVariable(optix::Ray, ray,          rtCurrentRay, );
rtDeclareVariable(float,      t_hit,        rtIntersectionDistance, );
RT_PROGRAM void closest_hit()
{
	float3 phit    = ray.origin + t_hit * ray.direction;

	float3 world_shading_normal   = normalize(shading_normal);
	float3 world_geometric_normal = normalize(geometric_normal);
	float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);
		
	pr_payload.result = make_float4( dot( ffnormal, make_float3( 0.0, -1.0, 1.0 ) ) );
}

RT_PROGRAM void any_hit()
{		
	prd_occlusion.occlusion = 1.0f;

	rtTerminateRay();
}

RT_PROGRAM void miss()
{
	pr_payload.result = make_float4 (0 );
}

struct VFormat
{
	float3 position;
	float3 normal;
	float2 uv[1];
};

rtBuffer<VFormat> vertex_buffer;
rtBuffer<int3> index_buffer;				// position indices

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
