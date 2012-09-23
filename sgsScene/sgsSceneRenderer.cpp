#include <gl/glew.h>
#include <SOIL.h>

#include <ppl.h>
#include <algorithm>
#include <boost/unordered_map.hpp>
#include <boost/range/algorithm/sort.hpp>
#include "boost/range/algorithm/for_each.hpp"

#include "MaxRectsBinPack.h"
#include <algorithm>
#include "sgsSceneRenderer.h"
#include <boost/timer/timer.hpp>

void SGSSceneRenderer::bakeTerrainTexture( int detailFactor, float textureDetailFactor ) {
	glPushAttrib( GL_ALL_ATTRIB_BITS );

	int numLayers = scene->terrain.layers.size();

	// create and load weight textures
	ScopedTextures2D weightTextures( numLayers );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	for( int layer = 0 ; layer < numLayers ; ++layer ) {
		auto weightTexture = weightTextures.get( layer );
		weightTexture.load( 0, GL_INTENSITY, scene->terrain.layerSize[0], scene->terrain.layerSize[1], 0, GL_RED, GL_UNSIGNED_BYTE, &scene->terrain.layers[ layer ].weights.front() );
		SimpleGL::setRepeatST( weightTexture );
		SimpleGL::setLinearMinMag( weightTexture );
	}

	ScopedFramebufferObject fbo;

	Eigen::Vector2i mapSize( scene->terrain.mapSize[0], scene->terrain.mapSize[1] );
	Eigen::Vector2i bakeSize = mapSize * detailFactor;

	bakedTerrainTexture.load( 0, GL_RGBA8, bakeSize.x(), bakeSize.y(), 0, GL_RGBA, GL_UNSIGNED_BYTE );

	fbo.attach( bakedTerrainTexture, GL_COLOR_ATTACHMENT0 );

	fbo.bind();
	fbo.setDrawBuffers();

	glViewport( 0, 0, bakeSize.x(), bakeSize.y() );

	// no depth buffer
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );

	glClearColor( 0.0, 0.0, 0.0, 0.0 );
	glClear( GL_COLOR_BUFFER_BIT );

	// enable blending
	glEnable( GL_BLEND );
	// additive blending
	glBlendFunc( GL_ONE, GL_ONE );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	// set the combiners
	glActiveTexture( GL_TEXTURE0 );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	glActiveTexture( GL_TEXTURE1 );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	glMatrixMode( GL_TEXTURE );
	glScalef( textureDetailFactor * mapSize.x(), textureDetailFactor * mapSize.y(), 1.0 );

	for( int layerIndex = 0 ; layerIndex < numLayers ; ++layerIndex ) {
		// bind weight first
		glActiveTexture( GL_TEXTURE0 );
		auto weightTexture = weightTextures.get( layerIndex );
		weightTexture.bind();
		weightTexture.enable();

		// bind the terrain texture
		glActiveTexture( GL_TEXTURE1 );
		Texture2D terrainTexture = textures[ scene->terrain.layers[ layerIndex ].textureIndex ];
		terrainTexture.bind();
		terrainTexture.enable();

		glBegin( GL_QUADS );
		glMultiTexCoord2f( GL_TEXTURE0, 0.0, 0.0 );
		glMultiTexCoord2f( GL_TEXTURE1, 0.0, 0.0 );
		glVertex3f( -1.0, -1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 1.0, 0.0 );
		glMultiTexCoord2f( GL_TEXTURE1, 1.0, 0.0 );
		glVertex3f( 1.0, -1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 1.0, 1.0 );
		glMultiTexCoord2f( GL_TEXTURE1, 1.0, 1.0 );
		glVertex3f( 1.0, 1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 0.0, 1.0 );
		glMultiTexCoord2f( GL_TEXTURE1, 0.0, 1.0 );
		glVertex3f( -1.0, 1.0, 0.0 );
		glEnd();
	}

	glActiveTexture( GL_TEXTURE0 );

	fbo.unbind();

	glPopAttrib();

	SimpleGL::setLinearMipmapMinMag( bakedTerrainTexture );
	bakedTerrainTexture.generateMipmap();
}

void SGSSceneRenderer::reloadShaders() {
	while( true ) {
		shaders.shaders.clear();

		loadShaderCollection( shaders, "sgsScene.shaders" );

		terrainProgram.surfaceShader = shaders[ "terrain" ];
		terrainProgram.vertexShader = shaders[ "sgsMesh" ];

		objectProgram.surfaceShader = shaders[ "object" ];
		objectProgram.vertexShader = shaders[ "sgsMesh" ];

		shadowMapProgram.vertexShader = shaders[ "shadowMapMesh" ];
		shadowMapProgram.surfaceShader = shaders[ "shadowMapSurface" ];

		if( terrainProgram.build( shaders ) && objectProgram.build( shaders ) && shadowMapProgram.build( shaders ) ) {
			break;
		}

		__debugbreak();
	}
}

void SGSSceneRenderer::processScene( const std::shared_ptr<SGSScene> &scene, const char *cacheFilename ) {
	this->scene = scene;

	bool cacheChanged = false;
	Cache cache;
	{
		boost::timer::auto_cpu_timer timer( "processScene; load cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );
		Serializer::BinaryReader reader( cacheFilename, sizeof Cache );
		if( reader.valid() ) {
			reader.get( cache );
		}

		// reset the cache if the scene has changed
		if( cache.magicStamp != scene->numSceneVertices ) {
			cache = Cache();
			cache.magicStamp = scene->numSceneVertices;
		}
	}

	// load textures
	int numTextures = scene->textures.size();
	textures.resize( numTextures );

	for( int textureIndex = 0 ; textureIndex < numTextures ; ++textureIndex ) {
		const auto &rawContent = scene->textures[ textureIndex ].rawContent;	

		SOIL_load_OGL_texture_from_memory( &rawContent.front(), rawContent.size(), 0, textures[ textureIndex ].handle, SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS );
	}

	if( cache.mergedObjectTextures.image.empty() ) {
		mergeTextures( textures );

		// store in cache
		cache.mergedObjectTextures.dump( mergedTexture );
		cache.mergedTextureInfos = mergedTextureInfos;
		cacheChanged = true;
	}
	else {
		// load from cache
		mergedTextureInfos = std::move( cache.mergedTextureInfos );
		cache.mergedObjectTextures.load( mergedTexture );

		SimpleGL::setRepeatST( mergedTexture );
		SimpleGL::setLinearMinMag( mergedTexture );
	}

	if( cache.bakedTerrainTexture.image.empty() ) {
		bakeTerrainTexture( 32, 1 / 8.0 );

		// store in cache
		cache.bakedTerrainTexture.dump( bakedTerrainTexture );

		cacheChanged = true;
	}
	else {
		// load from cache
		cache.bakedTerrainTexture.load( bakedTerrainTexture );

		bakedTerrainTexture.generateMipmap();
		SimpleGL::setRepeatST( bakedTerrainTexture );
		SimpleGL::setLinearMipmapMinMag( bakedTerrainTexture );
	}

	// render everything into display lists
	terrainLists.reserve( scene->terrain.tiles.size() );
	solidLists.reserve( scene->subObjects.size() );
	alphaLists.reserve( scene->subObjects.size() );

	loadStaticBuffers();
	setVertexArrayObjects();

	prepareMaterialDisplayLists();

	prerenderDebugInfos();

	// flush the cache if necessary
	if( cacheChanged ) {
		boost::timer::auto_cpu_timer timer( "processScene; store cache: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		Serializer::BinaryWriter writer( cacheFilename, sizeof Cache );
		writer.put( cache );
	}
}

void SGSSceneRenderer::loadStaticBuffers() {
	staticObjectsMesh.vertexBuffer.bufferData( scene->vertices.size() * sizeof( SGSScene::Vertex ), &scene->vertices.front(), GL_STATIC_DRAW );
	staticObjectsMesh.indexBuffer.bufferData( scene->indices.size() * sizeof( unsigned ), &scene->indices.front(), GL_STATIC_DRAW );

	terrainMesh.vertexBuffer.bufferData( scene->terrain.vertices.size() * sizeof( SGSScene::Terrain::Vertex ), &scene->terrain.vertices.front(), GL_STATIC_DRAW );
	terrainMesh.indexBuffer.bufferData( scene->terrain.indices.size() * sizeof( unsigned ), &scene->terrain.indices.front(), GL_STATIC_DRAW );
}

void SGSSceneRenderer::setVertexArrayObjects() {
	staticObjectsMesh.init();
	terrainMesh.init();
	
	// reset the state
	// TODO: verify that this is unnecessary and remove the following block [9/23/2012 kirschan2]
	GL::Buffer::unbind( GL_ARRAY_BUFFER );
	GL::Buffer::unbind( GL_ELEMENT_ARRAY_BUFFER );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

void SGSSceneRenderer::prepareMaterialDisplayLists() {
	materialDisplayLists.resize( scene->subObjects.size() );

	for( int subObjectIndex = 0 ; subObjectIndex < scene->subObjects.size() ; ++subObjectIndex ) {
		const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

		materialDisplayLists[ subObjectIndex ].begin();

		// apply the material
		const auto &material = subObject.material;

		if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
			textures[ material.textureIndex[0] ].bind();
			Texture2D::enable();
		}
		else {
			Texture2D::unbind();
		}

		unsigned char alpha = 255;

		switch( material.alphaType ) {
		default:
		case SGSScene::Material::AT_NONE:
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glDepthMask( GL_TRUE );

			break;
		case SGSScene::Material::AT_ADDITIVE:
			glDepthMask( GL_FALSE );
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glBlendFunc( GL_SRC_ALPHA, GL_ONE );

			break;
		case SGSScene::Material::AT_TEXTURE:
		case SGSScene::Material::AT_ALPHATEST:
			glDepthMask( GL_FALSE );
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0 );

			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			alpha = material.alpha;

			break;
		case SGSScene::Material::AT_MATERIAL:
			if( material.alpha == 255 ) {
				glDisable( GL_BLEND );
				glDisable( GL_ALPHA_TEST );

				glDepthMask( GL_TRUE );
			}
			else {
				glDepthMask( GL_FALSE );
				glDisable( GL_ALPHA_TEST );

				glEnable( GL_BLEND );
				glBlendFunc( GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA );

				glBlendColor( 1.0, 1.0, 1.0, material.alpha / 255.0 );
			}
			break;
		case SGSScene::Material::AT_MULTIPLY:
			glDepthMask( GL_FALSE );
			glDisable( GL_ALPHA_TEST );

			glEnable( GL_BLEND );
			glBlendFunc( GL_ZERO, GL_SRC_COLOR );

			break;
		case SGSScene::Material::AT_MULTIPLY_2:
			glDepthMask( GL_FALSE );
			glDisable( GL_ALPHA_TEST );

			glEnable( GL_BLEND );
			glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );

			break;
		}

		/*if( material.doubleSided ) {
		glDisable( GL_CULL_FACE );
		}
		else {
		glEnable( GL_CULL_FACE );
		}*/

		glColor4ub( material.diffuse.r, material.diffuse.g, material.diffuse.b, alpha );

		GL::DisplayList::end();
	}
	Texture2D::unbind();
}

void SGSSceneRenderer::prerenderDebugInfos() {
	{
		debug.boundingSpheres.begin();

		for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
			const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].bounding.sphere;

			debug.boundingSpheres.setPosition( Eigen::Vector3f::Map( boundingSphere.center ) );
			glColor3f( 0.0, 1.0, 1.0 );
			debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
		}
		debug.boundingSpheres.end();
	}
	{
		debug.terrainBoundingSpheres.begin();
		for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
			const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;

			debug.boundingSpheres.setPosition( Eigen::Vector3f::Map( boundingSphere.center ) );
			glColor3f( 1.0, 1.0, 0.0 );
			debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
		}
		debug.terrainBoundingSpheres.end();
	}
}

void SGSSceneRenderer::initShadowMap() {
	sunShadowMapSize = 8192;
	sunShadowMap.load( 0, GL_DEPTH_COMPONENT32F, sunShadowMapSize, sunShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr );

	SimpleGL::setClampToBorderST( sunShadowMap );
	SimpleGL::setLinearMinMag( sunShadowMap );

	sunShadowMap.parameter( GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE );
	sunShadowMap.parameter( GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	sunShadowMap.parameter( GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
}

void SGSSceneRenderer::renderShadowmap( const RenderContext &renderContext ) {
	using namespace Eigen;
	initShadowMap();

	// calculate bounding box
	AlignedBox3f sceneBoundingBox;
	for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
		const SGSScene::BoundingBox &boundingBox = scene->terrain.tiles[tileIndex].bounding.box;

		sceneBoundingBox.extend( AlignedBox3f( Vector3f::Map( boundingBox.min ), Vector3f::Map( boundingBox.max ) ) );
	}

	for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
		const SGSScene::BoundingBox &boundingBox = scene->subObjects[subObjectIndex].bounding.box;

		sceneBoundingBox.extend( AlignedBox3f( Vector3f::Map( boundingBox.min ), Vector3f::Map( boundingBox.max ) ) );
	}

	initShadowMapProjectionMatrix( sceneBoundingBox, Vector3f( 0.0, -1.0, -1.0 ).normalized() );

	ScopedFramebufferObject fbo;
	fbo.attach( sunShadowMap, GL_DEPTH_ATTACHMENT, 0 );

	fbo.bind();
	fbo.setDrawBuffers();

	glDisable( GL_CULL_FACE );
	glCullFace( GL_FRONT );

	glPushAttrib( GL_VIEWPORT_BIT );
	glViewport( 0, 0, sunShadowMapSize, sunShadowMapSize );

	glClear( GL_DEPTH_BUFFER_BIT );
	glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

	glMatrixMode( GL_PROJECTION );
	glLoadMatrix( sunProjectionMatrix );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	shadowMapProgram.use();		

	buildDrawLists( sunProjectionMatrix, renderContext );

	// draw terrain
	{
		Texture2D::unbind();
		terrainMesh.vao.bind();		
		glDrawElements( GL_TRIANGLES, scene->terrain.indices.size(), GL_UNSIGNED_INT, nullptr );
		terrainMesh.vao.unbind();
	}

	{
		staticObjectsMesh.vao.bind();

		GLuint *firstIndex = nullptr;
		for( int i = 0 ; i < solidLists.size() ; i++ ) {
			const int subObjectIndex = solidLists[i];
			glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
		}

		glEnable( GL_ALPHA_TEST );
		glAlphaFunc( GL_GREATER, 0.2f );

		for( int i = 0 ; i < alphaLists.size() ; i++ ) {
			const int subObjectIndex = alphaLists[i];
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			const auto &material = subObject.material;
			if( material.alphaType == SGSScene::Material::AT_ADDITIVE ) {
				continue;
			}
			if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
				textures[ material.textureIndex[0] ].bind();
				Texture2D::enable();
			}
			else {
				Texture2D::unbind();
			}

			glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
		}

		staticObjectsMesh.vao.unbind();
	}

	// state reset
	{
		glDisable( GL_BLEND );
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_CULL_FACE );

		glDepthMask( GL_TRUE );
		glDisable( GL_TEXTURE_2D );
	}

	// reset the buffer state
	fbo.unbind();
	fbo.resetDrawBuffers();

	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glPopAttrib();

	//glStringMarkerGREMEDY( 0, "shadow map done" );
}

void SGSSceneRenderer::buildDrawLists( const Eigen::Matrix4f &projectionView, const RenderContext &renderContext ) {
	Eigen::FrustumPlanesMatrixf frustumPlanes = Eigen::Frustum::normalize( Eigen::projectionToFrustumPlanes * projectionView );

	if( debug.updateRenderLists ) {
		terrainLists.clear();
		for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
			const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;
			if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Map< const Eigen::Vector3f>( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
				terrainLists.push_back( tileIndex );
			}
		}

		solidLists.clear();
		alphaLists.clear();
		for( int objectIndex = 0 ; objectIndex < scene->numSceneObjects ; ++objectIndex ) {
			const SGSScene::Object &object = scene->objects[objectIndex];

			if( objectIndex == renderContext.disabledInstanceIndex || object.modelId == renderContext.disabledModelIndex ) {
				continue;
			}

			const int endSubObject = object.startSubObject + object.numSubObjects;
			for( int subObjectIndex = object.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
				const SGSScene::SubObject &subObject = scene->subObjects[subObjectIndex];
				const SGSScene::BoundingSphere &boundingSphere = subObject.bounding.sphere;

				if( Eigen::Frustum::isInside( frustumPlanes, Eigen::Vector3f::Map( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
					if( scene->subObjects[subObjectIndex].material.alphaType == SGSScene::Material::AT_NONE ) {
						solidLists.push_back( subObjectIndex );
					}
					else {
						alphaLists.push_back( subObjectIndex );
					}
				}
			}
		}
	}
}

void SGSSceneRenderer::render( const Eigen::Matrix4f &projectionView, const Eigen::Vector3f &worldViewerPosition, const RenderContext &renderContext ) {
	buildDrawLists( projectionView, renderContext );
	sortAlphaList( worldViewerPosition );

	glDisable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	// set the shadow map
	glActiveTexture( GL_TEXTURE1 );
	sunShadowMap.bind();
	glActiveTexture( GL_TEXTURE0 );

	// terrain rendering
	{
		glDisable( GL_BLEND );
		glDisable( GL_ALPHA_TEST );

		glDepthMask( GL_TRUE );

		bakedTerrainTexture.bind();
		bakedTerrainTexture.enable();
		terrainProgram.use();

		// hack
		glUniform( terrainProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );
		glUniform( terrainProgram.uniformLocations[ "sunShadowProjection" ], sunProjectionMatrix );

		SimpleGL::ImmediateMultiDrawElements multiDrawElements;
		multiDrawElements.reserve( terrainLists.size() );			
		GLuint *firstIndex = nullptr;
		for( int i = 0 ; i < terrainLists.size() ; i++ ) {
			const int tileIndex = terrainLists[i];
			multiDrawElements.push_back( scene->terrain.tiles[tileIndex].numIndices, firstIndex + scene->terrain.tiles[tileIndex].startIndex );
		}

		if( !multiDrawElements.empty() ) {
			terrainMesh.vao.bind();
			multiDrawElements( GL_TRIANGLES, GL_UNSIGNED_INT );
			terrainMesh.vao.unbind();
		}
	}

	// object rendering
	{
		objectProgram.use();

		// hack
		glUniform( objectProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );
		glUniform( objectProgram.uniformLocations[ "sunShadowProjection" ], sunProjectionMatrix );

		staticObjectsMesh.vao.bind();

		GLuint *firstIndex = nullptr;
		for( int i = 0 ; i < solidLists.size() ; i++ ) {
			const int subObjectIndex = solidLists[i];
			materialDisplayLists[ subObjectIndex ].call();
			glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
		}

		for( int i = 0 ; i < alphaLists.size() ; i++ ) {
			const int subObjectIndex = alphaLists[i];
			materialDisplayLists[ subObjectIndex ].call();
			glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
		}

		staticObjectsMesh.vao.unbind();
	}

	// dynamic object rendering
	{
		objectProgram.use();

		glUniform( objectProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );
		glUniform( objectProgram.uniformLocations[ "sunShadowProjection" ], sunProjectionMatrix );

		staticObjectsMesh.vao.bind();
		
		for( int instanceIndex = 0 ; instanceIndex < instances.size() ; ++instanceIndex ) {
			const Instance &instance = instances[ instanceIndex ];
			if( renderContext.disabledModelIndex != instance.modelId && renderContext.disabledInstanceIndex != scene->objects.size() + instanceIndex ) {
				drawInstance( instances[ instanceIndex ] );
			}
		}

		staticObjectsMesh.vao.unbind();
	}

	// make sure this is turned on again, otherwise glClear wont work correctly...
	glDepthMask( GL_TRUE );

	{
		// more state resets for debug rendering
		glDisable( GL_BLEND );
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_CULL_FACE );

		glDepthMask( GL_TRUE );
		glActiveTexture( GL_TEXTURE1 );
		Texture2D::unbind();
		glActiveTexture( GL_TEXTURE0 );
		Texture2D::unbind();

		Program::useFixed();

		if( debug.showBoundingSpheres ) {
			debug.boundingSpheres.render();
		}
		if( debug.showTerrainBoundingSpheres ) {
			debug.terrainBoundingSpheres.render();
		}

		DebugRender::ImmediateCalls lightFrustum;
		lightFrustum.begin();
		glMatrixMode( GL_MODELVIEW );
		glMultMatrix( sunProjectionMatrix.inverse() );
		lightFrustum.drawBox( Eigen::Vector3f::Constant( 2.0 ), true, true );
		lightFrustum.end();
	}
}

void SGSSceneRenderer::sortAlphaList( const Eigen::Vector3f &worldViewerPosition ) {
	boost::sort(
		alphaLists, [&, this] ( int indexA, int indexB ) {
			return ( Eigen::Vector3f::Map( scene->subObjects[indexA].bounding.sphere.center ) - worldViewerPosition).squaredNorm() >
				( Eigen::Vector3f::Map( scene->subObjects[indexB].bounding.sphere.center ) - worldViewerPosition).squaredNorm();
	}
	);
}

void SGSSceneRenderer::initShadowMapProjectionMatrix( const Eigen::AlignedBox3f &boundingBox, const Eigen::Vector3f &direction ) {
#if 0
	// determine the main direction
	int mainAxis;
	{
		auto weightedDirection = direction.cwiseProduct( boundingBox.sizes() );
		weightedDirection.cwiseAbs().maxCoeff( &mainAxis );
	}

	int permutation[3] = { (mainAxis + 1) % 3, (mainAxis + 2) % 3, mainAxis };
	auto permutedDirection = permute( direction, permutation );

	Eigen::AlignedBox3f permutedBox;
	permutedBox.extend( permute( boundingBox.min(), permutation ) );
	permutedBox.extend( permute( boundingBox.max(), permutation ) );

	/*sunProjectionMatrix = Eigen::createShearProjectionMatrixLH( )
	Eigen::createOrthoProjectionMatrixLH( permutedBox.min(), permutedBox.max() ) * unpermutedToPermutedMatrix( permutation );*/
	sunProjectionMatrix = Eigen::createOrthoProjectionMatrixLH( permutedBox.min(), permutedBox.max() ) * unpermutedToPermutedMatrix( permutation );
#else
	using namespace Eigen;

	Matrix4f lightRotation = Eigen::createViewerMatrixLH( Vector3f::Zero(), direction, Vector3f::UnitX() );
	AlignedBox3f lightVolumeBox;
	for( int cornerIndex = 0 ; cornerIndex < 8 ; ++cornerIndex ) {
		lightVolumeBox.extend( (lightRotation * boundingBox.corner( AlignedBox3f::CornerType( cornerIndex ) ).homogeneous().eval()).hnormalized() );
	}

	sunProjectionMatrix = Eigen::createOrthoProjectionMatrixLH( lightVolumeBox.min(), lightVolumeBox.max() ) * lightRotation;
#endif
}

void SGSSceneRenderer::mergeTextures( const ScopedTextures2D &textures ) {
	typedef std::pair<int, int> Resolution;
	// distribute by resolutions
	boost::unordered_map<Resolution, std::vector<int> > resolutions;

	std::vector<RectSize> rectSizes;
	rectSizes.reserve( textures.handles.size() );

	for( int textureIndex = 0 ; textureIndex < textures.handles.size() ; textureIndex++ ) {
		int width, height;
		textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_WIDTH, &width );
		textures[ textureIndex ].getLevelParameter( 0, GL_TEXTURE_HEIGHT, &height );

		resolutions[ Resolution( width + 4, height + 4 ) ].push_back( textureIndex );

		RectSize rectSize = { width + 4, height + 4 };
		rectSizes.push_back( rectSize );
	}

	mergedTextureInfos.resize( textures.handles.size() );

	const int maxTextureSize = 8192;
	int mergedTextureSize = maxTextureSize;
	MaxRectsBinPack packer;

	packer.Init( mergedTextureSize, mergedTextureSize );
	std::vector<Rect> packedRects;
	packer.Insert( std::vector<RectSize>( rectSizes ), MaxRectsBinPack::RectBestShortSideFit );
	std::cout << packer.Occupancy() << "\n";

	packedRects = std::move( packer.GetRectangles() );

	int stepSize = maxTextureSize / 2;
	for( int step = 0 ; step < 4 ; ++step, stepSize /= 2 ) {
		const int testTextureSize = mergedTextureSize - stepSize;

		packer.Init( testTextureSize, testTextureSize );
		packer.Insert( std::vector<RectSize>( rectSizes ), MaxRectsBinPack::RectBestShortSideFit );

		if( packer.GetRectangles().size() == rectSizes.size() ) {
			std::cout << packer.Occupancy() << "\n";

			mergedTextureSize = testTextureSize;
			packedRects = std::move( packer.GetRectangles() );
		}
	}

	for( int i = 0 ; i < packedRects.size() ; i++ ) {
		const Rect &packedRect = packedRects[i];

		// find a fitting texture index
		auto &bin = resolutions[ Resolution( packedRect.width, packedRect.height ) ];
		int textureIndex = bin.back();
		bin.pop_back();

		auto &info = mergedTextureInfos[textureIndex];
		info.offset[0] = packedRect.x + 2;
		info.offset[1] = packedRect.y + 2;
		info.size[0] = packedRect.width - 4;
		info.size[1] = packedRect.height - 4;
	}

	// output texture
	// using uint instead of char[4]
	typedef unsigned __int32 RGBA;
	RGBA *mergedTextureData = new RGBA[ mergedTextureSize * mergedTextureSize ];
	{
		boost::timer::auto_cpu_timer timer( "mergeTextures blitting: %ws wall, %us user + %ss system = %ts CPU (%p%)\n" );

		Concurrency::task_group blits;

		for( int textureIndex = 0 ; textureIndex < textures.handles.size() ; ++textureIndex ) {
			auto &info = mergedTextureInfos[textureIndex];

			// download the uncompressed texture
			RGBA *textureData = new RGBA[ info.size[0] * info.size[1] ];
			textures[ textureIndex ].getImage( 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData );

			blits.run( [&, info, textureData]() {
#define Wrap( x, width ) (((x) + (width)) % (width))
#define Texel( x, y ) textureData[ Wrap(x, info.size[0]) + Wrap(y, info.size[1]) * info.size[0] ]
#define MergedTexel( x, y ) mergedTextureData[ (x) + (y) * mergedTextureSize ]

				for( int y = -2 ; y < info.size[1] + 2 ; ++y ) {
					int mergedY = info.offset[1] + y;
					for( int x = -2 ; x < info.size[0] + 2 ; ++x ) {
						int mergedX = info.offset[0] + x;

						MergedTexel( mergedX, mergedY ) = Texel( x, y );
					}
				}
#undef MergedTexel
#undef Texel
#undef Wrap
				delete[] textureData;
			});
		}

		blits.wait();
	}

	// upload the texture and compress it
	mergedTexture.load( 0, GL_RGBA8, mergedTextureSize, mergedTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, mergedTextureData );
	// TODO: add helper functions for this (but not to object wrapper..?) [9/19/2012 kirschan2]
	mergedTexture.parameter( GL_TEXTURE_WRAP_S, GL_REPEAT );
	mergedTexture.parameter( GL_TEXTURE_WRAP_T, GL_REPEAT );
	mergedTexture.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	mergedTexture.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR );

	delete[] mergedTextureData;
}

void SGSSceneRenderer::drawModel( SGSScene::Model &model ) {
	GLuint *firstIndex = nullptr;

	int endSubObject = model.startSubObject + model.numSubObjects;
	for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
		materialDisplayLists[ subObjectIndex ].call();
		glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
	}
}

void SGSSceneRenderer::drawInstance( Instance &instance ) {
	glMatrixLoad( GL_MODELVIEW, instance.transformation );
	drawModel( scene->models[ instance.modelId ] );
}

void SGSSceneRenderer::addInstance( const Instance &instance ) {
	instances.push_back( instance );

	refillDynamicOptixBuffers();
}

