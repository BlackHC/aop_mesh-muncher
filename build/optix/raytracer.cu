
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

rtDeclareVariable( Ray_Payload, pr_payload, rtPayload, );

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

RT_PROGRAM void miss()
{
	pr_payload.result = make_float4 (0 );
}