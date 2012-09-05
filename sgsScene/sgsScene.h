#pragma once

#include "serializer.h"
#include "serializer_std.h"

struct SGSScene {
	struct Texture {
		std::string name;
		int width;
		int height;
		int format;
		std::vector<unsigned char> image;

		SERIALIZER_DEFAULT_IMPL( (name)(width)(height)(format)(image) );
	};

	struct Vertex {
		float position[3];
		float normal[3];
		float uv[2];
	};

	struct SubObject {
		std::string modelName;
		int objectIndex;

		struct Material {
			int textureIndex;
		};

		Material material;

		// for rendering
		int startIndex;
		int numIndices;

		SERIALIZER_DEFAULT_IMPL( (objectIndex)(modelName)(startIndex)(numIndices)(material.textureIndex) );
	};

	std::vector<Vertex> vertices;
	std::vector<unsigned> indices;

	std::vector<SubObject> subObjects;
	std::vector<Texture> textures;

	int numObjects;

	std::map< std::string, int > textureNameIdMap;

	SERIALIZER_DEFAULT_IMPL( (numObjects)(subObjects)(textures)(textureNameIdMap)(vertices)(indices) );

	SGSScene() : numObjects( 0 ) {}
};

#if 1
SERIALIZER_ENABLE_RAW_MODE( SGSScene::Vertex );
#else
SERIALIZER_DEFAULT_EXTERN_IMPL( SGSScene::Vertex, (position)(normal)(uv) );
#endif
