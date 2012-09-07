#pragma once

#include "serializer.h"
#include "serializer_std.h"

struct SGSScene {
	static const int NO_TEXTURE = -1;

	struct Texture {
		std::string name;
		std::vector<unsigned char> rawContent;

		SERIALIZER_DEFAULT_IMPL( (name)(rawContent) );
	};

	struct Vertex {
		float position[3];
		float normal[3];
		float uv[2][2];
	};

	struct Color4ub {
		unsigned char r, g, b, a;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct Color3ub {
		unsigned char r, g, b;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct Material {
		int textureIndex[2];

		Color3ub ambient;
		Color3ub diffuse;
		Color3ub specular;
		float specularSharpness;

		unsigned char alpha;

		bool doubleSided;
		bool wireFrame;

		SERIALIZER_DEFAULT_IMPL( (textureIndex)(doubleSided)(wireFrame)(ambient)(diffuse)(specular)(alpha)(specularSharpness) )
		//SERIALIZER_ENABLE_RAW_MODE();
	};

	struct SubObject {
		std::string subModelName;

		Material material;		

		// for rendering
		int startIndex;
		int numIndices;

		SERIALIZER_DEFAULT_IMPL( (subModelName)(startIndex)(numIndices)(material) );
	};

	struct Object {
		int modelId;

		int startSubObject;
		int numSubObjects;

		SERIALIZER_DEFAULT_IMPL( (modelId)(startSubObject)(numSubObjects) );
	};

	struct Terrain {
		struct Vertex {
			float position[3];
			float normal[3];
			float blendUV[2];

			SERIALIZER_ENABLE_RAW_MODE();
		};

		struct Layer {
			int textureIndex;

			std::vector<unsigned char> weights;

			SERIALIZER_DEFAULT_IMPL( (textureIndex)(weights) );
		};

		static const int BLOCK_SIZE = 8;
		int mapSize[2];

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		
		/*std::vector< Layer > layers;

		std::vector< unsigned int > blockLayerMask;*/

		SERIALIZER_DEFAULT_IMPL( (mapSize)(vertices)(indices) );
	};

	std::vector<Vertex> vertices;
	std::vector<unsigned> indices;

	std::vector<std::string> modelNames;
	std::vector<Object> objects;
	std::vector<SubObject> subObjects;
	
	std::vector<Texture> textures;

	Terrain terrain;

	SERIALIZER_DEFAULT_IMPL( (modelNames)(objects)(subObjects)(textures)(vertices)(indices)(terrain) );

	SGSScene() {}
};

SERIALIZER_ENABLE_RAW_MODE_EXTERN( SGSScene::Vertex );
