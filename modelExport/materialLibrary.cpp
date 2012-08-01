#include <niven.Core.Core.h>

#include "materialLibrary.h"

using namespace niven;

void MaterialLibrary::LoadMTL( IO::IStream& stream, MaterialLibrary &mtl ) {
	SimpleLexer lexer( stream, SimpleLexerFlags::Ignore_Comments | SimpleLexerFlags::Pound_Comments | SimpleLexerFlags::Ignore_Whitespace | SimpleLexerFlags::No_Cpp_Comments );
	Token token;

	const char *newMaterialKeyword = "newmtl";
	const char *diffuseMapKeyword = "map_Kd";

	while( lexer.ReadToken(token) && token.GetStringValue() == newMaterialKeyword ) {
		lexer.ReadToken(token);
		String materialName = token.GetStringValue();

		while( lexer.ReadToken(token) ) {
			if( token.GetStringValue() == diffuseMapKeyword ) {
				lexer.ReadToken(token);
				String textureName = token.GetStringValue();

				mtl.materialMap[ materialName ] = Material( textureName );
			}
			else if( token.GetStringValue() == newMaterialKeyword ) {
				lexer.UnreadToken();
				break;
			}
		}
	}
}