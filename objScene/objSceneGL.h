#pragma once

#include "gl/glew.h"

#include <set>
#include <boost/container/flat_map.hpp>

#include <memory>

#include "materialLibrary.h"

#include <Eigen/Eigen>
#include <Eigen/Geometry>
#include <string>

struct ObjSceneGL {
	void Init( const char *objPath );
	void Draw();

	void SetObjectVisibility( const std::string &objectName, bool visible );
	void SetGroupVisibilty( const std::string &groupName, bool visible );

	struct SubModel {
		GLuint displayList;
		GLuint texture;
		GLfloat diffuseColor[3];

		std::string objectName;
		std::set<std::string> groupNames;
		bool visible;
	};

	struct Model {
		std::vector<SubModel*> subModels;
		std::string name;

		Eigen::AlignedBox3f boundingBox;
	};

	std::vector<SubModel> subModels;
	std::set<std::string> groupNames;
	std::set<std::string> objectNames;

	boost::container::flat_map< std::string, Model* > nameModelMap;
	std::vector<Model> models;

	Eigen::AlignedBox3f boundingBox;
};
