#if VERTEX_SHADER
out vec3 worldPos; 
void main() {
	gl_FrontColor = gl_Color;
	worldPos = vec3( gl_ModelViewMatrix * gl_Vertex );
	gl_Position = ftransform(); 
}
#elif FRAGMENT_SHADER
in vec3 worldPos; 
uniform vec3 viewerPos;

void main() {
	vec3 worldNormal = normalize( cross( dFdx( worldPos ), dFdy( worldPos ) ) );

	/*gl_FragColor = vec4( worldNormal / 2 + 0.5, 1.0 );
	return;*/

	const float ambientTerm = 0.6;
	// directional light
	const vec3 sunDirection = normalize( vec3( 1.0, -1.0, 1.0 ) );
	const vec3 pointLightPosition = vec3( 0.0, 15.0, 0.0 );

	const float sunTerm = max( 0.0, dot( worldNormal, sunDirection ) );

	const vec3 lightVector = pointLightPosition - worldPos;
	const float distanceAttenuation = 5.0 / length( lightVector ); 
	const vec3 lightDirection = normalize( lightVector );
	const float diffuseTerm = max( 0.0, dot( worldNormal, lightDirection ) );
	const float specularTerm = max( 0.0, dot( normalize( normalize(viewerPos - worldPos) + lightDirection ), worldNormal ) );
	
	gl_FragColor = vec4( gl_Color.rgb * ( ambientTerm + sunTerm + (diffuseTerm + pow( specularTerm, 4 )) * distanceAttenuation ), 1.0 );
}
#endif

