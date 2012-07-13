float4x4 WorldViewProjection;
float4x4 WorldView;

// Position
// 	Id: Pf3__
// 	Hash: DA4F9D04937219E721C08CE7634D477062C326F1
#if NIV_VFORMAT_DA4F9D04937219E721C08CE7634D477062C326F1

//////////////////////////////////////////////////////////////////////////////
struct ApplicationToVertex
{
	float3 position : POSITION;
};

//////////////////////////////////////////////////////////////////////////////
struct VertexToFragment
{
	float4 position : SV_POSITION;
	float3 worldPos : WORLDPOS;
};

//////////////////////////////////////////////////////////////////////////////
VertexToFragment VS_main (ApplicationToVertex app2vs)
{
	VertexToFragment result = (VertexToFragment)0;

	result.position = mul(float4(app2vs.position.xyz, 1.0), WorldViewProjection);
	result.worldPos = app2vs.position;

	return result;
}

float4 PS_main (VertexToFragment vtf) : SV_Target 
{
	float3 normal = normalize( cross( ddx( vtf.worldPos ), ddy( vtf.worldPos ) ) );

	float4 viewPos = mul(float4(vtf.worldPos, 1.0), WorldView);
	float attenuation = abs( dot( normalize( viewPos ), mul( float4( normal, 0.0 ), WorldView ) ) );

	float fade = max( 0, -2.0 / viewPos.z ) * attenuation + 0.1;
	return float4( fade * (normal * 0.5 + 0.5), 1.0 );
	
	//return float4( fade, fade, fade, 1.0 );
}

#endif