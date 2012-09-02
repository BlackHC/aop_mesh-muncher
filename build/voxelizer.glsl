splat:
	vertex: |
		void main() {
			gl_FrontColor = gl_Color;	
			gl_Position = gl_ModelViewMatrix * gl_Vertex; 
		}

	geometry: |
			uniform mat4 mainAxisProjection[3];
			uniform mat3 mainAxisPermutation[3];

			// these lines enable the geometry shader support.
			layout( triangles ) in;
			layout( triangle_strip, max_vertices=3 ) out;

			out vec3 viewPosition;
			flat out int mainAxis;

		#ifdef CONSERVATIVE
			out BoundingBox {
				vec2 min, max;
			} boundingBox;
		#endif

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

			// outer normal for a CCW edge orientation with y up
			vec2 getOuterNormal2( const vec2 edge, const vec3 faceNormal ) {
				return cross( vec3( edge, 0.0 ), vec3( 0.0, 0.0, sign( faceNormal.z ) ) ).xy;
			}
			// return lamda * a, such that lambda * a perp projected onto b is b
			vec2 backProjection( const vec2 a, const vec2 b ) {
				if( dot( b, b ) == 0 ) {
					return vec2( 0 );
				}
				return a * dot( b, b ) / dot( a, b );
			}
 
			vec3 expandCorner( const vec3 corner, const vec3 faceNormal, const vec2 edgeNext, const vec2 edgePrev, const vec2 halfPixelSize ) {	
				const vec2 edgeNormalNext = normalize( getOuterNormal2( edgeNext, faceNormal ) );
				const vec2 edgeNormalPrev = normalize( getOuterNormal2( edgePrev, faceNormal ) );
 
				const vec2 shiftedNext = edgeNormalNext * dot( abs( edgeNormalNext ), halfPixelSize ); 
				const vec2 shiftedPrev = edgeNormalPrev * dot( abs( edgeNormalPrev ), halfPixelSize );
 
				const vec2 deltaXY = backProjection( edgePrev, shiftedNext ) + backProjection( edgeNext, shiftedPrev );
 
				// solve for z
				const float triangle_d = dot( corner, faceNormal );
				// we have: faceNormal . newCorner - d = 0 <=> newCorner.z = (d - faceNormal.xy . newCorner.xy) / faceNormal.z
				const float deltaZ = -dot( faceNormal.xy, deltaXY ) / faceNormal.z;
 
				return corner + vec3( deltaXY, deltaZ );
			}

			void main( void )
			{
				// the incoming position is the one using output grid coordinates
				const vec3 edgeA = vec3( gl_in[1].gl_Position - gl_in[0].gl_Position );
				const vec3 edgeB = vec3( gl_in[2].gl_Position - gl_in[0].gl_Position );

				// calculate the face normal
				const vec3 faceNormal = cross( edgeA, edgeB );

				/*const int*/ mainAxis = determineMainAxis( faceNormal );

				// we use different viewports depending on the main axis, because the grid volume doesn't have to be a unit cube
				gl_ViewportIndex = mainAxis;

				// grab the projection and permutation matrix for this axis
				const mat4 projectionMatrix = mainAxisProjection[ mainAxis ];
				const mat3 permutationMatrix = mainAxisPermutation[ mainAxis ];

				// mat2( mainAxisProjection[ mainAxis ] ) * vec2( size.permuted_xy ) : full size
				const vec2 halfPixelSize = vec2( 0.5 );

				// permute the corners
				vec3 corners[3];
				for( int i = 0 ; i < 3 ; i++ ) {
					corners[i] = permutationMatrix * gl_in[i].gl_Position.xyz;		
				}

		#ifdef CONSERVATIVE
				// permute the face normal
				const vec3 permutedFaceNormal = permutationMatrix * faceNormal;

				// set the bounding box
				boundingBox.min = min( corners[0].xy, min( corners[1].xy, corners[2].xy ) ) - halfPixelSize;
				boundingBox.max = max( corners[0].xy, max( corners[1].xy, corners[2].xy ) ) + halfPixelSize;

				// calculate the planeEdges
				const vec2 planeEdges[3] = { corners[1].xy - corners[0].xy, corners[2].xy - corners[1].xy, corners[0].xy - corners[2].xy };

				// expand the corners

				corners[0] = expandCorner( corners[0], permutedFaceNormal, planeEdges[0], planeEdges[2], halfPixelSize );
				corners[1] = expandCorner( corners[1], permutedFaceNormal, planeEdges[1], planeEdges[0], halfPixelSize );
				corners[2] = expandCorner( corners[2], permutedFaceNormal, planeEdges[2], planeEdges[1], halfPixelSize );
		#endif 
				for( int i = 0 ; i < gl_in.length() ; i++ )
				{
					gl_FrontColor = gl_in[i].gl_FrontColor;
	
					// store the expanded corners in the original world coords (multiply with the transposed permutation matrix)
					viewPosition = corners[i];

					// transform the view position into the unit cube
					gl_Position = projectionMatrix * vec4( corners[i], 1.0 );

					EmitVertex();
				}
			}

	fragment: |
			uniform mat3 mainAxisPermutation[3];

			uniform volatile layout(r32ui) uimage3D volumeChannels[4]; // rgb hit

			flat in int mainAxis;
			in vec3 viewPosition;

		#ifdef CONSERVATIVE
			in BoundingBox {
				vec2 min, max;
			} boundingBox;
		#endif

			struct VoxelInfo {
				uvec3 color;
			};

			VoxelInfo processFragment() {
				VoxelInfo result;
				result.color = uvec3( gl_Color * 255.0 );
				return result;
			}

			void splatVoxel( const ivec3 voxelPosition, const VoxelInfo voxelInfo ) {
				imageAtomicAdd( volumeChannels[0], voxelPosition, voxelInfo.color.r );
				imageAtomicAdd( volumeChannels[1], voxelPosition, voxelInfo.color.g );
				imageAtomicAdd( volumeChannels[2], voxelPosition, voxelInfo.color.b );
				imageAtomicAdd( volumeChannels[3], voxelPosition, 1u );
			}

			void main() {
		#ifdef CONSERVATIVE
				if( any( lessThan( viewPosition.xy, boundingBox.min ) ) || any( greaterThan( viewPosition.xy, boundingBox.max ) ) ) {
					discard;
				}
		#endif

				VoxelInfo voxelInfo = processFragment();
		
				float deltaZ = 0.5 * fwidth( viewPosition.z );
				int minZ = int( viewPosition.z - deltaZ );
				int maxZ = int( ceil( viewPosition.z + deltaZ ) );

				for( int z = minZ ; z <= maxZ ; ++z ) {
					ivec3 voxelPosition = ivec3( vec3( viewPosition.xy, z ) * mainAxisPermutation[ mainAxis ] );

					splatVoxel( voxelPosition, voxelInfo );
				}

				//gl_FragColor = vec4( vec3(worldPosition) / 32.0, 1.0 );
			}
	
muxer: 	
	vertex: |
		// TODO: using a geometry shader to create full screen quads and the fragment shader for the actual operation might be better throughput wise?
		uniform restrict readonly layout(r32ui) uimage3D volumeChannels[4]; // rgb hit
		uniform restrict writeonly layout(rgba8) image3D volume;

		uniform ivec3 sizeHelper; // size.x size.y size.x*size.y;

		void main() {
			ivec3 index3;
			index3.x = gl_InstanceID % sizeHelper.x;
			index3.y = (gl_InstanceID / sizeHelper.x) % sizeHelper.y;
			index3.z = (gl_InstanceID / sizeHelper.z);
		
			uvec4 rgb_hit;
			for( int i = 0 ; i < 4 ; ++i ) {
				rgb_hit[i] = imageLoad( volumeChannels[i], index3 )[0];
			}

			vec4 color;
			color.rgb = vec3( rgb_hit.rgb ) / 255.0 / rgb_hit[3];
			color.a = rgb_hit[3] > 0 ? 1.0 : 0.0;
			imageStore( volume, index3, color );
		}
	
