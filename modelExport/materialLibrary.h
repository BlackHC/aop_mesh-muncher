#pragma once

#include <niven.Core.String.h>
#include <niven.Core.Text.SimpleLexer.h>

#include <map>

struct MaterialLibrary {
	struct Material {
		niven::String texture;

		Material() {}
		Material( const niven::String &texture ) : texture( texture ) {}
	};

	std::map<niven::String, Material> materialMap;

	static void LoadMTL( niven::IO::IStream& stream, MaterialLibrary &mtl );
};