#include "ctp.h"

#include "leanTextProcessing.h"

using namespace LeanTextProcessing;

struct IndentationManager {
	enum IndentationType {
		IT_UNDETERMINED,
		IT_TAB,
		IT_SPACE
	} type;

	int indentLevel;

	bool matchOneLevel( TextIterator &iterator ) {
		if( iterator.tryMatch( ' ' ) ) {
			if( type == IT_UNDETERMINED ) {
				type = IT_SPACE;
			}
			else if( type != IT_SPACE ) {
				text.error( "wrong indentation character (tab expected)!" );
			}
			return true;
		}
		else if( iterator.tryMatch( '\t' ) ) {
			if( type == IT_UNDETERMINED ) {
				type = IT_TAB;
			}
			else if( type != IT_TAB ) {
				text.error( "wrong indentation character (space expected)!" );
			}
			return true;
		}
		return false;
	}

	int determineIndentation( TextIterator &text ) {
		TextIterator::Scope scope( text );

		int localIndentLevel = 0;
		while( matchOneLevel( text ) ) {
			++localIndentLevel;
		}
		return localIndentLevel;
	}

	void eatIndentation( TextIterator &iterator ) {
		indentLevel = 0;
		while( matchOneLevel( iterator ) ) {
			++indentLevel;
		}
	}
};

namespace ctp {
	struct Processor {
		IndentationManager indentationManager;
		TextIterator iterator;

		std::string processedContent;

		Processor( const TextIterator &iterator ) : iterator( iterator ) {}

		void emitIndentation() {
			processedContent.append( indentationManager.indentLevel, '\t' );
		}

		void processBlock() {
			indentationManager.indentLevel++;
			processedContent.append( '\n' );

			// line by line
			while( !iterator.atEof() ) {
				emitIndentation();

				if( iterator.tryMatch( "\\>>>" ) ) {
					processedContent.append( ">>>" );
				}
				else if( iterator.tryMatch( ">>>" ) ) {
					processedContent.append( '\"' );
					break;
				}
				else if( iterator.tryMatch( '\n') ) {
			}
		}

		void process() {
			// read line by line and wait for a special token - either @" or <<<
			while( !iterator.atEof() ) {
				indentationManager.eatIndentation( iterator );

				emitIndentation();

				while( iterator.checkNot( '\n' ) ) {
					if( iterator.tryMatch( "<<<") ) {
						processBlock();
					}
					else if( iterator.tryMatch( "@\"") ) {
						processRawString();
					}
					else {
						processedContent.append( iterator.read() );
					}
				}
			}
		}
	};

	std::string process( const std::string &content, const std::string &sourceIdentifier /* = "" */ ) {
		using namespace LeanTextProcessing;

		TextContainer container( content, sourceIdentifier );
		Processor processor( TextIterator( container, TextPosition() ) );
		processor.process();
	}
}