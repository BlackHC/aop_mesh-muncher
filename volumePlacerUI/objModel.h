#pragma once

#include <niven.Core.IO.Path.h>

#include <niven.Render.Effect.h>
#include <niven.Engine.Render.EffectManager.h>
#include <niven.Render.RenderContext.h>
#include <niven.Render.Texture.h>
#include <niven.Render.VertexLayout.h>
#include <niven.Render.VertexBuffer.h>
#include <niven.Render.IndexBuffer.h>

#include <niven.Engine.Geometry.SimpleMesh.h>

#include "materialLibrary.h"

struct ObjModel {
	void Init( niven::IRenderSystem::Ptr renderSystem, niven::Render::EffectManager &effectManager, niven::IO::Path &objPath );
	void Draw( niven::Render::IRenderContext *renderContext );

	std::vector<niven::SimpleMesh::Ptr> meshes_;
	std::vector<niven::Render::ITexture::Ptr> textures_;
	niven::Render::ITexture::Ptr nullTexture_;

	std::vector<niven::Render::Effect*> effects_;
	std::vector<niven::IVertexLayout::Ptr> vertexLayouts_;
	std::vector<niven::IIndexBuffer::Ptr> indexBuffers_;
	std::vector<niven::IVertexBuffer::Ptr> vertexBuffers_;
};
