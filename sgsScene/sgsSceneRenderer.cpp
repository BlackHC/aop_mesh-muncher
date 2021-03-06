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
#include "optixRenderer.h"

#include <mathUtility.h>

#include <grid.h>

#include <autoTimer.h>

using namespace Eigen;

void SGSSceneRenderer::bakeTerrainTexture( int detailFactor, float textureDetailFactor ) {
	glPushAttrib( GL_ALL_ATTRIB_BITS );

	const size_t numLayers = scene->terrain.layers.size();

	// create and load weight textures
	ScopedTextures2D weightTextures( numLayers );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	for( size_t layer = 0 ; layer < numLayers ; ++layer ) {
		auto weightTexture = weightTextures.get( layer );
		weightTexture.load( 0, GL_INTENSITY, scene->terrain.layerSize[0], scene->terrain.layerSize[1], 0, GL_RED, GL_UNSIGNED_BYTE, &scene->terrain.layers[ layer ].weights.front() );
		SimpleGL::setRepeatST( weightTexture );
		SimpleGL::setLinearMinMag( weightTexture );
	}

	ScopedFramebufferObject fbo;

	Vector2i mapSize( scene->terrain.mapSize[0], scene->terrain.mapSize[1] );
	if( mapSize.isZero() ) {
		mapSize.setConstant( 1 );
	}

	Vector2i bakeSize = mapSize * detailFactor;

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
		try {
			shaders.shaders.clear();

			loadShaderCollection( shaders, "sgsScene.shaders" );

			terrainProgram.surfaceShader = shaders[ "terrain" ];
			terrainProgram.vertexShader = shaders[ "sgsMesh" ];

			objectProgram.surfaceShader = shaders[ "object" ];
			objectProgram.vertexShader = shaders[ "sgsMesh" ];

			previewObjectProgram.surfaceShader = shaders[ "previewObject" ];
			previewObjectProgram.vertexShader = shaders[ "sgsMesh" ];

			shadowMapProgram.vertexShader = shaders[ "shadowMapMesh" ];
			shadowMapProgram.surfaceShader = shaders[ "shadowMapSurface" ];

			voxelizerShaders.shaders.clear();
			loadShaderCollection( voxelizerShaders, "voxelizer.shaders" );

			voxelizerSplatterProgram.vertexShader = voxelizerShaders[ "splatterVertexShader" ];
			voxelizerSplatterProgram.geometryShader = voxelizerShaders[ "splatterGeometryShader" ];
			voxelizerSplatterProgram.fragmentShader = voxelizerShaders[ "splatterFragmentShader" ];

			voxelizerMuxerProgram.vertexShader = voxelizerShaders[ "muxer" ];

			if(
				terrainProgram.build( shaders ) &&
				objectProgram.build( shaders ) &&
				previewObjectProgram.build( shaders ) &&
				shadowMapProgram.build( shaders ) &&

				voxelizerSplatterProgram.build( voxelizerShaders ) &&
				voxelizerMuxerProgram.build( voxelizerShaders )
			) {
				break;
			}
		}
		catch( std::exception &e) {
			std::cerr << e.what() << std::endl;
		}
		__debugbreak();
	}
}

void SGSSceneRenderer::processScene( const std::shared_ptr<SGSScene> &scene, const char *cacheFilename ) {
	this->scene = scene;

	bool cacheChanged = false;
	Cache cache;
	AUTO_TIMER_BLOCK( "load cache" ) {
		Serializer::BinaryReader reader( cacheFilename, Cache::VERSION );
		if( reader.valid() ) {
			reader.get( cache );
		}

		// reset the cache if the scene has changed
		if( cache.magicStamp != getSceneHash() ) {
			cache = Cache();
			cache.magicStamp = getSceneHash();
		}
	}

	// load textures
	int numTextures = scene->textures.size();
	textures.resize( numTextures );

	for( int textureIndex = 0 ; textureIndex < numTextures ; ++textureIndex ) {
		const auto &rawContent = scene->textures[ textureIndex ].rawContent;

		SOIL_load_OGL_texture_from_memory( &rawContent.front(), rawContent.size(), 0, textures[ textureIndex ].handle, SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_MIPMAPS | SOIL_FLAG_TEXTURE_REPEATS );
	}

	// create white texture
	{
		int data = ~0;
		whiteTexture.load( 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
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
	visibleSolidInstancedSubObjects.reserve( scene->subObjects.size() );
	visibleTransparentInstancedSubObjects.reserve( scene->subObjects.size() );
	instancedSubObjects.reserve( scene->objects.size() * 3  );
	refreshInstancedSubObjects();

	loadStaticBuffers();
	setVertexArrayObjects();

	updateSceneBoundingBox();

	prepareMaterialDisplayLists();

	prerenderDebugInfos();


	// flush the cache if necessary
	if( cacheChanged ) {
		AUTO_TIMER( "store cache" );

		Serializer::BinaryWriter writer( cacheFilename, Cache::VERSION );
		writer.put( cache );
	}
}

template< typename T >
T *getVectorData( std::vector< T > &container ) {
	if( container.empty() ) {
		return nullptr;
	}
	return &container.front();
}

void SGSSceneRenderer::loadStaticBuffers() {
	staticObjectsMesh.vertexBuffer.bufferData( scene->vertices.size() * sizeof( SGSScene::Vertex ), getVectorData( scene->vertices ), GL_STATIC_DRAW );
	staticObjectsMesh.indexBuffer.bufferData( scene->indices.size() * sizeof( unsigned ), getVectorData( scene->indices ), GL_STATIC_DRAW );

	terrainMesh.vertexBuffer.bufferData( scene->terrain.vertices.size() * sizeof( SGSScene::Terrain::Vertex ), getVectorData( scene->terrain.vertices ), GL_STATIC_DRAW );
	terrainMesh.indexBuffer.bufferData( scene->terrain.indices.size() * sizeof( unsigned ), getVectorData( scene->terrain.indices ), GL_STATIC_DRAW );
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

		unsigned char alpha = 255;

		switch( material.alphaType ) {
		default:
		case SGSScene::Material::AT_NONE:
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );

			glDepthMask( GL_TRUE );

			break;
		case SGSScene::Material::AT_ADDITIVE:
			glDisable( GL_ALPHA_TEST );
			glDepthMask( GL_FALSE );	

			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE );

			break;
		case SGSScene::Material::AT_MATERIAL:
			if( material.alpha == 255 ) {
				glDepthMask( GL_TRUE );

				glDisable( GL_BLEND );
				glDisable( GL_ALPHA_TEST );
			}
			else {
				glDepthMask( GL_TRUE );
				glDisable( GL_ALPHA_TEST );

				glEnable( GL_BLEND );
				glBlendFunc( GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA );

				glBlendColor( 1.0f, 1.0f, 1.0f, material.alpha / 255.0f );
			}
			break;
		case SGSScene::Material::AT_TEXTURE:
			glDepthMask( GL_FALSE );
			
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0 );
			
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			alpha = material.alpha;

			break;
		case SGSScene::Material::AT_ALPHATEST:
			glDepthMask( GL_TRUE );

			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0 );
			
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			alpha = material.alpha;
			
			break;
		case SGSScene::Material::AT_MULTIPLY:
			glDepthMask( GL_FALSE );
			glDisable( GL_ALPHA_TEST );

			glEnable( GL_BLEND );
			glBlendFunc( GL_ZERO, GL_CONSTANT_ALPHA );

			glBlendColor( 1.0f, 1.0f, 1.0f, material.alpha / 255.0f );

			break;
		case SGSScene::Material::AT_MULTIPLY_2:
			glDepthMask( GL_FALSE );
			glDisable( GL_ALPHA_TEST );

			glEnable( GL_BLEND );
			glBlendFunc( GL_DST_COLOR, GL_CONSTANT_ALPHA );

			glBlendColor( 1.0f, 1.0f, 1.0f, material.alpha / 255.0f );

			break;
		}

		/*if( material.doubleSided ) {
		glDisable( GL_CULL_FACE );
		}
		else {
		glEnable( GL_CULL_FACE );
		}*/

		if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
			textures[ material.textureIndex[0] ].bind();
		}
		else {
			whiteTexture.bind();
		}
		Texture2D::enable();

		glColor4ub( material.diffuse.r, material.diffuse.g, material.diffuse.b, alpha );

		GL::DisplayList::end();
	}
	Texture2D::unbind();
}

void SGSSceneRenderer::prerenderDebugInfos() {
	{
		/*debug.boundingSpheres.begin();

		for( int subObjectIndex = 0 ; subObjectIndex < scene->numSceneSubObjects ; ++subObjectIndex ) {
			const SGSScene::BoundingSphere &boundingSphere = scene->subObjects[subObjectIndex].bounding.sphere;

			debug.boundingSpheres.setPosition( Vector3f::Map( boundingSphere.center ) );
			glColor3f( 0.0, 1.0, 1.0 );
			debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
		}
		debug.boundingSpheres.end();*/
	}
	{
		debug.terrainBoundingSpheres.begin();
		for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
			const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;

			debug.boundingSpheres.setPosition( Vector3f::Map( boundingSphere.center ) );
			glColor3f( 1.0f, 1.0f, 0.0f );
			debug.boundingSpheres.drawAbstractSphere( boundingSphere.radius, true, 5 );
		}
		debug.terrainBoundingSpheres.end();
	}
}

void SGSSceneRenderer::initShadowMap() {
	sunShadowMapSize = 8192;
	sunShadowMap.load( 0, GL_DEPTH_COMPONENT24, sunShadowMapSize, sunShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr );

	SimpleGL::setClampToBorderST( sunShadowMap );
	SimpleGL::setLinearMinMag( sunShadowMap );

	//sunShadowMap.parameter( GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE );
	sunShadowMap.parameter( GL_TEXTURE_COMPARE_MODE, GL_NONE );
	//sunShadowMap.parameter( GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
}

void SGSSceneRenderer::updateSceneBoundingBox() {
	sceneBoundingBox.setEmpty();

	for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
		const SGSScene::BoundingBox &boundingBox = scene->terrain.tiles[tileIndex].bounding.box;

		sceneBoundingBox.extend( AlignedBox3f( Vector3f::Map( boundingBox.min ), Vector3f::Map( boundingBox.max ) ) );
	}

	for( int instancedSubObjectIndex = 0 ; instancedSubObjectIndex < instancedSubObjects.size() ; instancedSubObjectIndex++ ) {
		const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

		const int subObjectIndex = instancedSubObject.subObjectIndex;
		const SGSScene::BoundingBox &rawBoundingBox = scene->subObjects[subObjectIndex].bounding.box;

		const AlignedBox3f boundingBox( Vector3f::Map( rawBoundingBox.min ), Vector3f::Map( rawBoundingBox.max ) );
		const auto transformedBoundingBox = Eigen_getTransformedAlignedBox( getInstanceTransformation( instancedSubObject.instanceIndex ), boundingBox );
		sceneBoundingBox.extend( transformedBoundingBox );
	}
}

void SGSSceneRenderer::renderShadowmap( const RenderContext &renderContext ) {
	// TODO: we don't have to set this every frame.. [10/19/2012 Andreas]
	initShadowMap();

	initShadowMapProjectionMatrix( sceneBoundingBox, Vector3f( 1.0, 1.0, 1.0 ).normalized() );

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

	refreshVisibilityLists( sunProjectionMatrix, renderContext );

	// draw terrain
	{
		Texture2D::unbind();
		terrainMesh.vao.bind();
		glDrawElements( GL_TRIANGLES, scene->terrain.indices.size(), GL_UNSIGNED_INT, nullptr );
		terrainMesh.vao.unbind();
	}

	// draw all instances
	{
		whiteTexture.bind();
		whiteTexture.enable();
		staticObjectsMesh.vao.bind();

		GLuint *firstIndex = nullptr;
		for( int i = 0 ; i < visibleSolidInstancedSubObjects.size() ; i++ ) {
			const int instancedSubObjectIndex = visibleSolidInstancedSubObjects[i];
			const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

			const int subObjectIndex = instancedSubObject.subObjectIndex;

			glMatrixLoad( GL_MODELVIEW, getInstanceTransformation( instancedSubObject.instanceIndex ) );
			glDrawElements(
				GL_TRIANGLES,
				scene->subObjects[ subObjectIndex ].numIndices,
				GL_UNSIGNED_INT,
				firstIndex + scene->subObjects[ subObjectIndex ].startIndex
			);
		}

		for( int i = 0 ; i < visibleTransparentInstancedSubObjects.size() ; i++ ) {
			const int instancedSubObjectIndex = visibleTransparentInstancedSubObjects[i];
			const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

			const int subObjectIndex = instancedSubObject.subObjectIndex;
			const SGSScene::SubObject &subObject = scene->subObjects[ subObjectIndex ];

			const auto &material = subObject.material;
			if( material.alphaType == SGSScene::Material::AT_ADDITIVE ) {
				continue;
			}
			if( material.textureIndex[0] != SGSScene::NO_TEXTURE ) {
				textures[ material.textureIndex[0] ].bind();				
			}
			else {
				whiteTexture.bind();
			}

			Texture2D::enable();

			glMatrixLoad( GL_MODELVIEW, getInstanceTransformation( instancedSubObject.instanceIndex ) );
			glDrawElements(
				GL_TRIANGLES,
				scene->subObjects[ subObjectIndex ].numIndices,
				GL_UNSIGNED_INT,
				firstIndex + scene->subObjects[ subObjectIndex ].startIndex
			);
		}

		staticObjectsMesh.vao.unbind();
	}

	// state reset
	{
		glMatrixLoad( GL_MODELVIEW, Eigen::Matrix4f::Identity() );
		glDisable( GL_BLEND );
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_CULL_FACE );

		glDepthMask( GL_TRUE );
		glDisable( GL_TEXTURE_2D );

		Texture2D::unbind();
		Texture2D::disable();
	}

	// reset the buffer state
	fbo.unbind();
	fbo.resetDrawBuffers();

	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glPopAttrib();

	Program::useFixed();

	glGetError();

	//glStringMarkerGREMEDY( 0, "shadow map done" );
}

void SGSSceneRenderer::refreshVisibilityLists_noCulling( const RenderContext &renderContext ) {
	if( !debug.updateRenderLists ) {
		return;
	}

	// cull terrain tiles
	terrainLists.clear();
	for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
		const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;
		terrainLists.push_back( tileIndex );
	}

	visibleSolidInstancedSubObjects.clear();
	visibleTransparentInstancedSubObjects.clear();

	for( int instancedSubObjectIndex = 0 ; instancedSubObjectIndex < instancedSubObjects.size() ; instancedSubObjectIndex++ ) {
		const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

		if(
				instancedSubObject.instanceIndex == renderContext.disabledInstanceIndex
			||
				instancedSubObject.modelIndex == renderContext.disabledModelIndex
		) {
			continue;
		}

		const SGSScene::SubObject &subObject = scene->subObjects[ instancedSubObject.subObjectIndex ];
		const SGSScene::BoundingSphere &boundingSphere = subObject.bounding.sphere;
		const Eigen::Vector3f transformedCenter = getInstanceTransformation( instancedSubObject.instanceIndex ) * Vector3f::Map( subObject.bounding.sphere.center );

		visibleSolidInstancedSubObjects.push_back( instancedSubObjectIndex );
		visibleTransparentInstancedSubObjects.push_back( instancedSubObjectIndex );
	}
}

void SGSSceneRenderer::renderSceneView( const Matrix4f &projectionView, const Vector3f &worldViewerPosition, const RenderContext &renderContext ) {
	refreshVisibilityLists( projectionView, renderContext );
	//refreshVisibilityLists_noCulling( renderContext );
	sortInstancedSubObjectsByDistance( visibleTransparentInstancedSubObjects, worldViewerPosition );

	drawScene( worldViewerPosition, renderContext, debug.showSceneWireframe );
}

void SGSSceneRenderer::renderFullScene( const Vector3f &worldViewerPosition, const RenderContext &renderContext, bool wireframe ) {
	refreshVisibilityLists_noCulling( renderContext );
	drawScene( worldViewerPosition, renderContext, wireframe );
}

void SGSSceneRenderer::drawScene( const Vector3f &worldViewerPosition, const RenderContext &renderContext, bool wireframe ) {
	// render the lists now
	glDisable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	// set the shadow map
	glActiveTexture( GL_TEXTURE1 );
	sunShadowMap.bind();
	glActiveTexture( GL_TEXTURE0 );

	glDisable( GL_BLEND );
	glDisable( GL_ALPHA_TEST );

	glDepthMask( GL_TRUE );

	if( wireframe ) {
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	}

	// terrain rendering
	{
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
		for( int i = 0 ; i < visibleSolidInstancedSubObjects.size() ; i++ ) {
			const int instancedSubObjectIndex = visibleSolidInstancedSubObjects[i];
			const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

			const int subObjectIndex = instancedSubObject.subObjectIndex;

			materialDisplayLists[ subObjectIndex ].call();

			glMatrixLoad( GL_MODELVIEW, getInstanceTransformation( instancedSubObject.instanceIndex ) );
			glDrawElements(
				GL_TRIANGLES,
				scene->subObjects[ subObjectIndex ].numIndices,
				GL_UNSIGNED_INT,
				firstIndex + scene->subObjects[ subObjectIndex ].startIndex
			);
		}

		for( int i = 0 ; i < visibleTransparentInstancedSubObjects.size() ; i++ ) {
			const int instancedSubObjectIndex = visibleTransparentInstancedSubObjects[i];
			const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

			const int subObjectIndex = instancedSubObject.subObjectIndex;

			materialDisplayLists[ subObjectIndex ].call();

			glMatrixLoad( GL_MODELVIEW, getInstanceTransformation( instancedSubObject.instanceIndex ) );
			glDrawElements(
				GL_TRIANGLES,
				scene->subObjects[ subObjectIndex ].numIndices,
				GL_UNSIGNED_INT,
				firstIndex + scene->subObjects[ subObjectIndex ].startIndex
			);
		}

		staticObjectsMesh.vao.unbind();
	}

	if( wireframe ) {
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	glMatrixLoad( GL_MODELVIEW, Eigen::Matrix4f::Identity() );

	resetState();
	{
		if( debug.showBoundingSpheres ) {
			debug.boundingSpheres.render();
		}
		if( debug.showTerrainBoundingSpheres ) {
			debug.terrainBoundingSpheres.render();
		}

		DebugRender::begin();
		glMatrixMode( GL_MODELVIEW );
		glMultMatrix( sunProjectionMatrix.inverse() );
		DebugRender::drawBox( Vector3f::Constant( 2.0 ), true, true );
		DebugRender::end();
	}

	glGetError();
}

void SGSSceneRenderer::resetState() {
	// make sure this is turned on again, otherwise glClear wont work correctly...
	glDepthMask( GL_TRUE );

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
}

void SGSSceneRenderer::renderModel( const Vector3f &worldViewerPosition, int modelIndex ) {
	glDisable( GL_CULL_FACE );
	glCullFace( GL_BACK );

	glDisable( GL_BLEND );
	glDisable( GL_ALPHA_TEST );

	previewObjectProgram.use();

	glUniform( previewObjectProgram.uniformLocations[ "viewerPosition" ], worldViewerPosition );

	staticObjectsMesh.vao.bind();

	drawModel( scene->models[ modelIndex ] );

	staticObjectsMesh.vao.unbind();

	resetState();
}

void SGSSceneRenderer::sortInstancedSubObjectsByDistance( std::vector< int > &list, const Vector3f &worldViewerPosition ) {
	boost::sort(
		list, 
		[&] ( int indexA, int indexB ) {
			return 
					( Vector3f::Map( scene->subObjects[ instancedSubObjects[ indexA ].subObjectIndex ].bounding.sphere.center ) - worldViewerPosition).squaredNorm()
				>
					( Vector3f::Map( scene->subObjects[ instancedSubObjects[ indexB ].subObjectIndex ].bounding.sphere.center ) - worldViewerPosition).squaredNorm()
			;
		}
	);
}

// direction is the direction towards the light
void SGSSceneRenderer::initShadowMapProjectionMatrix( const AlignedBox3f &boundingBox, const Vector3f &direction ) {
	Matrix4f lightRotation = createViewerMatrixLH( Vector3f::Zero(), -direction, Vector3f::UnitX() );
	AlignedBox3f lightVolumeBox;
	for( int cornerIndex = 0 ; cornerIndex < 8 ; ++cornerIndex ) {
		lightVolumeBox.extend( (lightRotation * boundingBox.corner( AlignedBox3f::CornerType( cornerIndex ) ).homogeneous().eval()).hnormalized() );
	}

	sunProjectionMatrix = createOrthoProjectionMatrixLH( lightVolumeBox.min(), lightVolumeBox.max() ) * lightRotation;
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
	AUTO_TIMER_BLOCK( "blitting" ) {
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

void SGSSceneRenderer::drawInstance( int instanceIndex ) {
	glMatrixLoad( GL_MODELVIEW, getInstanceTransformation( instanceIndex ) );
	drawModel( scene->models[ getModelIndex( instanceIndex ) ] );
}

int SGSSceneRenderer::addInstance( const Instance &instance ) {
	int instanceIndex = instances.size() + scene->objects.size();

	instances.push_back( instance );
	
	refreshInstancedSubObjects();

	refillDynamicOptixBuffers();
	updateSceneBoundingBox();
	
	return instanceIndex;
}

void SGSSceneRenderer::removeInstance( int instanceIndex ) {
	instanceIndex -= scene->objects.size();
	if( instanceIndex >= 0 ) {
		instances.erase( instances.begin() + instanceIndex );

		refreshInstancedSubObjects();

		refillDynamicOptixBuffers();
		updateSceneBoundingBox();
	}
}

namespace VoxelizedModel {
	Voxels voxelize( const IndexMapping3<> &indexMapping3, std::function<void()> renderScene, Program &splatProgram, Program &muxerProgram ) {
		AUTO_TIMER_FUNCTION();

		// TOOD: state cleanup [10/13/2012 kirschan2]
		// save all state for now
		glPushAttrib( GL_ALL_ATTRIB_BITS );
		glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

		glPixelStorei( GL_PACK_ALIGNMENT, 1 );

		unsigned __int32 *volumeChannelsData[4];

		splatProgram.use();

		GLuint volumeChannels[4];
		glGenTextures( 4, volumeChannels );
		for( int i = 0 ; i < 4 ; ++i ) {
			volumeChannelsData[i] = new unsigned __int32[ indexMapping3.count ];
			memset( volumeChannelsData[i], 0, sizeof( __int32 ) * indexMapping3.count );

			glBindTexture( GL_TEXTURE_3D, volumeChannels[i] );
			glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().z(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, volumeChannelsData[i] );

			glBindImageTexture( i, volumeChannels[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI );

			//glUniform1i( splatShader.volumeChannels[i], i );
			// TODO: we can set this in the GLSL code! [10/13/2012 kirschan2]
			glUniform1i( splatProgram.uniform( "volumeChannels", i ), i );
		}

		glBindTexture( GL_TEXTURE_3D, 0 );

		GLuint fbo;
		glGenFramebuffers( 1, &fbo );
		glBindFramebuffer( GL_FRAMEBUFFER, fbo );

		GLuint renderbuffer;

		glGenRenderbuffers( 1, &renderbuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, renderbuffer );
		int maxSize = indexMapping3.getSize().maxCoeff();
		glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA, maxSize, maxSize );
		glBindRenderbuffer( GL_RENDERBUFFER, 0 );

		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer );
		// disable framebuffer operations
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );
		glDrawBuffer( GL_NONE );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		glDepthMask( GL_FALSE );

		// GL_PROJECTION is not needed
		int permutations[3][3] = { {1,2,0}, {2,0,1}, {0,1,2} };
		const Vector3f offset = Vector3f::Constant( 0.5 );
		for( int i = 0 ; i < 3 ; ++i ) {
			int *permutation = permutations[i];
			const Vector3i permutedSize = permute( indexMapping3.getSize(), permutation );
			// the grid points are the centers of their voxel's boxes
			auto projection = Eigen::createOrthoProjectionMatrixLH( Vector3f::Zero() - offset, permutedSize.cast<float>() - offset );

			//glUniform( splatShader.mainAxisProjection[i], projection );
			//glUniform( splatShader.mainAxisPermutation[i], unpermutedToPermutedMatrix( permutation ).topLeftCorner<3,3>().matrix() );

			glUniform( splatProgram.uniform( "mainAxisProjection", i ), projection );
			// this could be set as default or we could set these in the shader!
			glUniform( splatProgram.uniform( "mainAxisPermutation", i), unpermutedToPermutedMatrix( permutation ).topLeftCorner<3,3>().matrix() );

			glViewportIndexedf( i, 0, 0, (float) permutedSize.x(), (float) permutedSize.y() );
		}

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrix( indexMapping3.positionToIndex );

		renderScene();

		muxerProgram.use();

		GLuint volume;
		glGenTextures( 1, &volume );

		glBindTexture( GL_TEXTURE_3D, volume );
		glTexStorage3D( GL_TEXTURE_3D, 1, GL_RGBA8, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().z() );

		for( int i = 0 ; i < 4 ; ++i ) {
			glBindImageTexture( i, volumeChannels[i], 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI );
			//glUniform1i( muxerShader.volumeChannels[i], i );
			// TOOD: we can set this directly in the shader [10/13/2012 kirschan2]
			glUniform1i( muxerProgram.uniform( "volumeChannels", i), i );
		}
		glBindImageTexture( 4, volume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8 );

		//glUniform1i( muxerShader.volume, 4 );
		// TODO: set this in the shader [10/13/2012 kirschan2]
		glUniform1i( muxerProgram.uniform( "volume" ), 4 );

		//glUniform3i( muxerShader.sizeHelper, indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().x() * indexMapping3.getSize().y() );
		glUniform3i( muxerProgram.uniform( "sizeHelper" ), indexMapping3.getSize().x(), indexMapping3.getSize().y(), indexMapping3.getSize().x() * indexMapping3.getSize().y() );

		glEnableClientState( GL_VERTEX_ARRAY );
		float zero[3] = {0.0, 0.0, 0.0};
		glVertexPointer( 3, GL_FLOAT, 0, &zero );

		glEnable( GL_RASTERIZER_DISCARD );
		glDrawArraysInstanced( GL_POINTS, 0, 1, indexMapping3.count );
		glDisable( GL_RASTERIZER_DISCARD );

		// read back our data
		// TODO: this is slow.. asynchronous pixel pack buffers would be better [10/13/2012 kirschan2]
		//Color4ub *volumeData = new Color4ub[ indexMapping3.count ];
		//
		// SimpleIndexer3 uses x-minor ordering[10/13/2012 kirschan2]
		Voxels voxels( indexMapping3 );
		glGetTexImage( GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, voxels.getData() );

		glBindTexture( GL_TEXTURE_3D, 0 );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		glUseProgram( 0 );

		glPopClientAttrib();
		glPopAttrib();

		delete[] volumeChannelsData[0];
		delete[] volumeChannelsData[1];
		delete[] volumeChannelsData[2];
		delete[] volumeChannelsData[3];

		glDeleteTextures( 4, volumeChannels );
		glDeleteTextures( 1, &volume );

		glDeleteFramebuffers( 1, &fbo );
		glDeleteRenderbuffers( 1, &renderbuffer );

		return voxels;
	}
}

VoxelizedModel::Voxels SGSSceneRenderer::voxelizeModel( int modelIndex, float resolution ) {
	const auto boundingBox = getModelBoundingBox( modelIndex );

	// set up the index mapping
	SimpleIndexMapping3 indexMapping = createCenteredIndexMapping( resolution, boundingBox.sizes(), boundingBox.center() );

	return VoxelizedModel::voxelize(
		indexMapping,
		[&] () {
			const auto &model = scene->models[ modelIndex ];

			staticObjectsMesh.vao.bind();
			GLuint *firstIndex = nullptr;

			int endSubObject = model.startSubObject + model.numSubObjects;
			for( int subObjectIndex = model.startSubObject ; subObjectIndex < endSubObject ; ++subObjectIndex ) {
				materialDisplayLists[ subObjectIndex ].call();
				glDrawElements( GL_TRIANGLES, scene->subObjects[ subObjectIndex ].numIndices, GL_UNSIGNED_INT, firstIndex + scene->subObjects[ subObjectIndex ].startIndex );
			}

			staticObjectsMesh.vao.unbind();
		},
		voxelizerSplatterProgram,
		voxelizerMuxerProgram
	);
}

void SGSSceneRenderer::refreshVisibilityLists( const Eigen::Matrix4f &projectionView, const RenderContext &renderContext ) {
	if( !debug.updateRenderLists ) {
		return;
	}

	FrustumPlanesMatrixf frustumPlanes = Frustum::normalize( projectionToFrustumPlanes * projectionView );

	// cull terrain tiles
	terrainLists.clear();
	for( int tileIndex = 0 ; tileIndex < scene->terrain.tiles.size() ; tileIndex++ ) {
		const SGSScene::BoundingSphere &boundingSphere = scene->terrain.tiles[tileIndex].bounding.sphere;
		if( Frustum::isInside( frustumPlanes, Map< const Vector3f>( boundingSphere.center ).eval(), -boundingSphere.radius ) ) {
			terrainLists.push_back( tileIndex );
		}
	}
	
	visibleSolidInstancedSubObjects.clear();
	visibleTransparentInstancedSubObjects.clear();

	// add the visible sub objects
	for( int instancedSubObjectIndex = 0 ; instancedSubObjectIndex < instancedSubObjects.size() ; instancedSubObjectIndex++ ) {
		const auto &instancedSubObject = instancedSubObjects[ instancedSubObjectIndex ];

		if(
				instancedSubObject.instanceIndex == renderContext.disabledInstanceIndex
			||
				instancedSubObject.modelIndex == renderContext.disabledModelIndex
		) {
			continue;
		}

		const SGSScene::SubObject &subObject = scene->subObjects[ instancedSubObject.subObjectIndex ];
		const SGSScene::BoundingSphere &boundingSphere = subObject.bounding.sphere;
		const Eigen::Vector3f transformedCenter = getInstanceTransformation( instancedSubObject.instanceIndex ) * Vector3f::Map( subObject.bounding.sphere.center );

		if( Frustum::isInside( frustumPlanes, transformedCenter, -boundingSphere.radius ) ) {
			if( subObject.material.alphaType == SGSScene::Material::AT_NONE ) {
				visibleSolidInstancedSubObjects.push_back( instancedSubObjectIndex );
			}
			else {
				visibleTransparentInstancedSubObjects.push_back( instancedSubObjectIndex );
			}
		}
	}
}

void SGSSceneRenderer::refreshInstancedSubObjects() {
	instancedSubObjects.clear();
	// start with the fixed objects
	for( int instanceIndex = 0 ; instanceIndex < getNumInstances() ; ++instanceIndex ) {
		const int modelIndex = getModelIndex( instanceIndex );
		const auto &model = getModel( modelIndex );
		for( int subObjectCounter = 0 ; subObjectCounter < model.numSubObjects ; subObjectCounter++ ) {
			instancedSubObjects.push_back( InstancedSubObject( instanceIndex, model.startSubObject + subObjectCounter, modelIndex ) );
		}
	}
}
