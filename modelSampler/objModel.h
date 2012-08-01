#pragma once

#include "gl/glew.h"

#include <niven.Core.IO.Path.h>

#include <niven.Render.Effect.h>
#include <niven.Engine.Render.EffectManager.h>
#include <niven.Render.RenderContext.h>
#include <niven.Render.Texture.h>
#include <niven.Render.VertexLayout.h>
#include <niven.Render.VertexBuffer.h>
#include <niven.Render.IndexBuffer.h>

#include <niven.Engine.Geometry.SimpleMesh.h>
#include <niven.Engine.Spatial.AxisAlignedBoundingBox.h>

#include <set>
#include <boost/container/flat_map.hpp>

#include <memory>

#include "materialLibrary.h"

struct ObjSceneGL {
	void Init( niven::IO::Path &objPath );
	void Draw();

	void SetObjectVisibility( const niven::String &objectName, bool visible );
	void SetGroupVisibilty( const niven::String &groupName, bool visible );

	struct SubModel {
		niven::SimpleMesh::Ptr mesh;
		GLuint displayList;
		GLuint texture;
		GLfloat diffuseColor[3];

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
