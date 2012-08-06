#include <niven.Core.Core.h>

#include "materialLibrary.h"

using namespace niven;

void MaterialLibrary::LoadMTL( IO::IStream& stream, MaterialLibrary &mtl ) {
	SimpleLexer lexer( stream, SimpleLexerFlags::Ignore_Comments | SimpleLexerFlags::Pound_Comments | SimpleLexerFlags::Ignore_Whitespace | SimpleLexerFlags::No_Cpp_Comments );
	Token token;

	const char *newMaterialKeyword = "newmtl";
	const char *diffuseMapKeyword = "map_Kd";
	const char *diffuseColorKeyword = "Kd";

	while( lexer.ReadToken(token) && token.GetStringValue() == newMaterialKeyword ) {
		lexer.ReadToken(token);
		String materialName = token.GetStringValue();

		while( lexer.ReadToken(token) ) {
			if( token.GetStringValue() == diffuseColorKeyword ) {
				niven::Color3f color;

				lexer.ReadToken(token);
				color[0] = token.GetFloatValue();
				lexer.ReadToken(token);
				color[1] = token.GetFloatValue();
				lexer.ReadToken(token);
				color[2] = token.GetFloatValue();

				mtl.materialMap[ materialName ].diffuseColor = color;
			}
			else if( token.GetStringValue() == diffuseMapKeyword ) {
				lexer.ReadToken(token);
				String textureName = token.GetStringValue();

				mtl.materialMap[ materialName ].texture = textureName;
			}
			else if( token.GetStringValue() == newMaterialKeyword ) {
				lexer.UnreadToken();
				break;
			}
		}
	}
}