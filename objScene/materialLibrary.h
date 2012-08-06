#pragma once

#include <niven.Core.String.h>
#include <niven.Core.Text.SimpleLexer.h>
#include <niven.Core.Color.h>

#include <map>

struct MaterialLibrary {
	struct Material {
		niven::String texture;
		niven::Color3f diffuseColor;

		Material() : diffuseColor( 1.0, 1.0, 1.0 ) {}
	};

	std::map<niven::String, Material> materialMap;

	static void LoadMTL( niven::IO::IStream& stream, MaterialLibrary &mtl );
};