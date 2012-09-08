#pragma once

#include "serializer.h"

#include <string>
#include <map>

typedef std::map< std::string, std::string > VariableDeclarations;

struct Program {
	
};

struct Shader {
	enum Type {
		ST_SURFACE,
		ST_TARGET,
		ST_MESH,
		ST_OBJECT,
	};
	std::string code;
};