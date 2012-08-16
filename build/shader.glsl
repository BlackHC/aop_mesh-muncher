#ifdef VERTEX_SHADER
out vec3 viewPos; 
void main() {
	gl_FrontColor = gl_Color;
	viewPos = vec3( gl_ModelViewMatrix * gl_Vertex );
	gl_Position = ftransform(); 
}
#endif
#ifdef FRAGMENT_SHADER
in vec3 viewPos; 

void main() {
	vec3 normal = cross( dFdx( viewPos ), dFdy( viewPos ) );
	gl_FragColor = vec4( gl_Color.rgb * max( 0.0, 0.4 + 5.0 / length( viewPos ) * abs( dot( normalize( normal ), normalize( viewPos ) ) ) ), 1.0 );
}
#endif

