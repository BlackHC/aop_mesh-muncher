#if VERTEX_SHADER
	void main() {
		gl_FrontColor = gl_Color;	
		gl_Position = gl_ModelViewMatrix * gl_Vertex; 
	}
#endif

#if GEOMETRY_SHADER
	uniform mat4 mainAxisProjection[3];

	// these lines enable the geometry shader support.
	layout( triangles ) in;
	layout( triangle_strip, max_vertices=3 ) out;

	out vec3 worldPosition;
	out {
		vec2 min, max;
	} boundingBox;

	int determineMainAxis( const vec3 faceNormal ) {
		const vec3 axisDot = abs( faceNormal );

		int mainAxis = 0;
		if( axisDot.x > axisDot.z ) {
			if( axisDot.y > axisDot.x ) {
				mainAxis = 1;
			}
		}
		else { // z > x
			if( axisDot.y > axisDot.z ) {
				mainAxis = 1;
			}
			else {
				mainAxis = 2;
			}
		}

		return mainAxis;
	}

	vec4[3] conservativeExpand( const vec4 corners[3], const vec2 halfPixelSize ) {
		const vec3 edges[3] = { vec3( corners[1] - corners[0] ), vec3( corners[2] - corners[1] ), vec3( corners[0] - corners[2] ) };
		vec3 edgePlanes[3];
		
		for( int i = 0 ; i < 3 ; ++i ) {
			edgePlanes[i] = cross( edges[i], corners[i].xyz );
			edgePlanes[i].z -= dot( halfPixelSize, abs( edgePlanes[i].xy ) );
		}
 
		vec4 intersections[3];
 
		// use w to impliclity divide by 'z'
		intersections[0].xyw = cross( edgePlanes[0], edgePlanes[2] );
		intersections[1].xyw = cross( edgePlanes[1], edgePlanes[0] );
		intersections[2].xyw = cross( edgePlanes[1], edgePlanes[2] );
 
		// calculate the new z
		const vec3 normal = cross( edges[0], edges[1] );
		const float face_d = dot( normal, corners[0].xyz );
		for( int i = 0 ; i < 3 ; ++i ) {
			intersections[i].z = (face_d - dot( normal.xy, intersections[i].xy )) / normal.z * intersections[i].w;
		}
 
		return intersections;
	}

	void main( void )
	{
		// the incoming position is the one using output grid coordinates
		const vec3 edgeA = vec3( gl_in[1].gl_Position - gl_in[0].gl_Position );
		const vec3 edgeB = vec3( gl_in[2].gl_Position - gl_in[0].gl_Position );

		// calculate the face normal
		const vec3 faceNormal = cross( edgeA, edgeB );

		const int mainAxis = determineMainAxis( faceNormal );

		// we use different viewports depending on the main axis, because the grid volume doesn't have to be a unit cube
		gl_ViewportIndex = mainAxis;

		const mat4 projectionMatrix = mainAxisProjection[ mainAxis ];
		// mat2( mainAxisProjection[ mainAxis ] ) * vec2( size.permuted_xy ) : full size
		const vec2 halfPixelSize = mat2( projectionMatrix ) * vec2( 0.5 );

		// transform the corners
		vec4 corners[3];
		for( int i = 0 ; i < 3 ; i++ ) {
			corners[i] = projectionMatrix * gl_in[i].gl_Position;
		}

		// set the bounding box
		boundingBox.min = min( corners[0], min( corners[1], corners[2] ) ) - halfPixelSize;
		boundingBox.max = max( corners[0], max( corners[1], corners[2] ) ) + halfPixelSize;

		corners = conservativeExpand( corners, halfPixelSize );	
		
		for( int i = 0 ; i < gl_in.length() ; i++ )
		{
			gl_FrontColor = gl_in[i].gl_FrontColor;
	
			// store the original position for easy value accumulation
			worldPosition = vec3( gl_in[i].gl_Position );

			// the grid volume doesn't have to be a unit cube, so we need a different projection matrix for each axis
			gl_Position = corners[i];

			EmitVertex();
		}
	}
#endif

#if FRAGMENT_SHADER
	uniform volatile layout(r32ui) uimage3D volumeChannels[4]; // rgb hit

	in vec3 worldPosition;

	void main() {
		// for now only store only one voxel
		uvec3 ucolor = uvec3( gl_Color * 255.0 );

		ivec3 voxelPosition = ivec3( worldPosition );

		imageAtomicAdd( volumeChannels[0], voxelPosition, ucolor.r );
		imageAtomicAdd( volumeChannels[1], voxelPosition, ucolor.g );
		imageAtomicAdd( volumeChannels[2], voxelPosition, ucolor.b );
		imageAtomicAdd( volumeChannels[3], voxelPosition, 1u );

		//imageAtomicAdd( volumeChannels[3], ivec3(0), 1u );

		//gl_FragColor = vec4( vec3(worldPosition) / 32.0, 1.0 );
	}
#endif

