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
		float uv[1][2];
	};

	struct Color4ub {
		unsigned char r, g, b, a;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct Color3ub {
		unsigned char r, g, b;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct BoundingSphere {
		float center[3];
		float radius;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct BoundingBox {
		float min[3], max[3];

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct Material {
		int textureIndex[1];

		Color3ub ambient;
		Color3ub diffuse;
		Color3ub specular;
		float specularSharpness;

		unsigned char alpha;

		bool doubleSided;
		bool wireFrame;

		enum AlphaType {
			AT_NONE,
			AT_MATERIAL, // material only alpha
			AT_TEXTURE, // texture * material 
			AT_ADDITIVE, // additive
			AT_MULTIPLY,
			AT_MULTIPLY_2,
			AT_ALPHATEST // like AT_TEXTURE
		};

		AlphaType alphaType;

		SERIALIZER_DEFAULT_IMPL( (alphaType)(textureIndex)(doubleSided)(wireFrame)(ambient)(diffuse)(specular)(alpha)(specularSharpness) )
		//SERIALIZER_ENABLE_RAW_MODE();
	};

	struct Bounding {
		BoundingBox box;
		BoundingSphere sphere;

		SERIALIZER_ENABLE_RAW_MODE();
	};

	struct SubObject {
		std::string subModelName;

		Material material;

		// for rendering
		int startIndex;
		int numIndices;

		int startVertex;
		int numVertices;

		Bounding bounding;

		SERIALIZER_DEFAULT_IMPL( (subModelName)(startVertex)(numVertices)(startIndex)(numIndices)(material)(bounding) );
	};

	struct Model {
		int startSubObject;
		int numSubObjects;

		Bounding bounding;

		SERIALIZER_DEFAULT_IMPL( (startSubObject)(numSubObjects)(bounding) );
	};

	struct Object : Model {
		int modelId;
		// column major in OGL (== row major in DX)
		float transformation[16];

		// TODO: add inheritance support to serializer? [9/23/2012 kirschan2]
		SERIALIZER_DEFAULT_IMPL( (modelId)(startSubObject)(numSubObjects)(bounding)(transformation) );
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

		struct Tile {
			Bounding bounding;

			int startIndex;
			int numIndices;

			// both are raw
			SERIALIZER_ENABLE_RAW_MODE();
		};

		static const int TILE_SIZE = 8;
		static const int EDGES_PER_TILE = TILE_SIZE - 1;
		static const int VERTICES_PER_TILE = TILE_SIZE * TILE_SIZE;
		static const int TRIANGLES_PER_TILE = 2 * (TILE_SIZE - 1) * (TILE_SIZE - 1);
		static const int INDICES_PER_TILE = 3 * TRIANGLES_PER_TILE;

		int mapSize[2];
		int layerSize[2];

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		
		std::vector<Layer> layers;

		std::vector<Tile> tiles;

		//std::vector< unsigned int > blockLayerMask;

		SERIALIZER_DEFAULT_IMPL( (mapSize)(layerSize)(vertices)(indices)(layers)(tiles) );
	};

	// scene data first, then prototype object data
	std::vector<Vertex> vertices;
	std::vector<unsigned> indices;
	
	std::vector<std::string> modelNames;
	
	// first scene subobjects, then prototype subobjects
	std::vector<SubObject> subObjects;
	// first scene objects
	std::vector<Object> objects;
	// in model id order
	std::vector<Model> models;

	int numSceneVertices;
	int numSceneIndices;
	int numSceneSubObjects;
	int numSceneObjects;

	std::vector<Texture> textures;

	Terrain terrain;

	SERIALIZER_DEFAULT_IMPL( (numSceneVertices)(numSceneIndices)(numSceneSubObjects)(numSceneObjects)(modelNames)(models)(objects)(subObjects)(textures)(vertices)(indices)(terrain) );

	SGSScene() {}
};

SERIALIZER_ENABLE_RAW_MODE_EXTERN( SGSScene::Vertex );
