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
#include <set>

#include "materialLibrary.h"

struct ObjModel {
	void Init( niven::IRenderSystem::Ptr renderSystem, niven::Render::EffectManager &effectManager, niven::IO::Path &objPath );
	void Draw( niven::Render::IRenderContext *renderContext );

	void SetObjectVisibility( const niven::String &objectName, bool visible );
	void SetGroupVisibilty( const niven::String &groupName, bool visible );

	struct SubModel {
		niven::SimpleMesh::Ptr mesh;
		niven::Render::ITexture::Ptr texture;
		niven::Render::Effect* effect;
		niven::IVertexLayout::Ptr vertexLayout;
		niven::IIndexBuffer::Ptr indexBuffer;
		niven::IVertexBuffer::Ptr vertexBuffer;

		niven::String objectName;
		std::set<niven::String> groupNames;
		bool visible;
	};

	std::vector<SubModel> subModels;
	std::set<niven::String> groupNames;
	std::set<niven::String> objectNames;

	niven::Render::ITexture::Ptr nullTexture_;
};
