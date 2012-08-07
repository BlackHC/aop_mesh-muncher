#pragma once

#include <niven.Core.IO.Path.h>

#include <niven.Render.Effect.h>
#include <niven.Render.RenderContext.h>
#include <niven.Render.Texture.h>
#include <niven.Render.VertexLayout.h>
#include <niven.Render.VertexBuffer.h>
#include <niven.Render.IndexBuffer.h>
#include <niven.Render.EffectManager.h>

#include <niven.Engine.Geometry.SimpleMesh.h>
#include <niven.Geometry.Spatial.AxisAlignedBoundingBox.h>

#include <set>
#include <boost/container/flat_map.hpp>

#include <memory>

#include "materialLibrary.h"

struct ObjScene {
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

	struct Model {
		std::vector<SubModel*> subModels;
		niven::String name;

		niven::AxisAlignedBoundingBox3 boundingBox;
	};

	std::vector<SubModel> subModels;
	std::set<niven::String> groupNames;
	std::set<niven::String> objectNames;

	boost::container::flat_map< niven::String, Model* > nameModelMap;
	std::vector<Model> models;

	niven::AxisAlignedBoundingBox3 boundingBox;

	niven::Render::ITexture::Ptr nullTexture_;
};
