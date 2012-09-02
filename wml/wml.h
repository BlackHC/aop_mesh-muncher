#pragma once

#include <vector>
#include <utility>

#include "leanTextProcessing.h"

namespace wml {
	struct Node {
		std::string value;			
		std::vector< Node > items;

		Node & data() {
			return items[0];
		}

		template< typename T >
		T as() const {
			return boost::lexical_cast<T>( value );
		}

		const std::string & key() const {
			return value;
		}

		Node & operator[] ( const std::string &key ) {
			for( auto item = items.begin() ; item != items.end() ; ++item ) {
				if( item->value == key ) {
					return *item;
				}
			}

			// TODO: throw an exception
			throw std::out_of_range( boost::str( boost::format( "key '%s' not found!" ) % key ) );
		}

		Node & operator[] ( int i ) {
			return items[ i ];
		}

		size_t size() const {
			return items.size();
		}

		bool empty() const {
			return items.empty();
		}

		std::vector< Node * > getAll( const std::string &key ) {
			std::vector< Node * > results;

			for( auto item = items.begin() ; item != items.end() ; ++item ) {
				if( item->value == key ) {
					results.push_back( &*item );
				}
			}

			return results;
		}

		Node() {}
		Node( const std::string &value ) : value( value ) {}

		Node( Node &&node ) : value( std::move( node.value ) ), items( std::move( node.items ) ) {}

		Node & operator == ( Node &&node ) {
			value = std::move( node.value );
			items = std::move( node.items );
		}
	};

	namespace detail {
		using namespace LeanTextProcessing;

		struct Parser {
			int indentLevel;
			TextIterator textIterator;

			std::string parseIdentifier() {
				std::string text;
				while( textIterator.checkNotAny( " \t\n" ) ) {
					if( textIterator.check( ':' ) ) {
						TextIterator::Scope scope( textIterator );
						textIterator.tryMatch( "::" ) || textIterator.tryMatch( ':' );

						if( textIterator.checkNotAny( " \t\n" ) ) {
							text.append( scope.getScopedText() );
							scope.accept();
						}
						else {
							scope.reject();
							break;
						}
					}
					text.push_back( textIterator.read() );
				}

				if( text.empty() ) {
					textIterator.error( "expected identifier!" );
				}

				return text;
			}

			void skipWhitespace() {
				while( textIterator.tryMatchAny( " \t" ) )
					;
			}

			void skipEmptyLines() {
				TextIterator::Scope scope( textIterator );

				while( true ) {
					skipWhitespace();
					
					if( !textIterator.tryMatch( '\n' ) ) {
						break;
					}

					scope.accept();
				}
			}

			std::string parseEscapedString() {
				std::string text;

				if( !textIterator.tryMatch( '"' ) ) {
					textIterator.error( "'\"' expected!" );
				}
				while( textIterator.checkNotAny( "\"\n" ) ) {
					if( textIterator.tryMatch( '\\' ) ) {
						if( textIterator.atEof() ) {
							textIterator.error( "unexpected EOF!" );
						}
						const char control = textIterator.read();
						switch( control ) {
						case '\\':
						case '\'':
						case '\"':
							text.push_back( control );
							break;
						case 't':
							text.push_back( '\t' );
							break;
						case 'n':
							text.push_back( '\n' );
							break;
						default:
							textIterator.error( boost::str( boost::format( "unknown escape control character '%c'!" ) % control  ) );
						}
					}
					else {
						text.push_back( textIterator.read() );
					}
				}
				if( !textIterator.tryMatch( '"' ) ) {
					textIterator.error( "'\"' expected!" );
				}

				return text;
			}

			std::string parseUnescapedString() {
				std::string text;

				if( !textIterator.tryMatch( '\'' ) ) {
					textIterator.error( "' expected!" );
				}
				while( textIterator.checkNotAny( "'\n" ) ) {
					text.push_back( textIterator.read() );
				}
				if( !textIterator.tryMatch( '\'' ) ) {
					textIterator.error( "' expected!" );
				}

				return text;
			}

			void skipIndentLevel() {
				for( int i = 0 ; i < indentLevel ; ++i ) {
					if( !textIterator.tryMatch( '\t' ) ) {
						textIterator.error( boost::str( boost::format( "expected %i tabs - found only %i!") % indentLevel % i  ) );
					}
				}
			}

			bool checkMinimumIndentLevel() {
				TextIterator::Scope scope( textIterator);

				for( int i = 0 ; i < indentLevel ; ++i ) {
					if( !textIterator.tryMatch( '\t') ) {
						return false;
					}
				}
				return true;
			}

			bool checkIndentLevel() {
				TextIterator::Scope scope( textIterator );

				for( int i = 0 ; i < indentLevel ; ++i ) {
					if( !textIterator.tryMatch( '\t') ) {
						return false;
					}
				}

				if( textIterator.check( '\t' ) ) {
					return false;
				}

				return true;
			}

			void ensureIndentLevel() {
				skipIndentLevel();

				if( textIterator.check( '\t' ) ) {
					textIterator.error( boost::str( boost::format( "expected %i tabs - found more!") % indentLevel ) );
				}
			}

			bool checkRestOfLineEmpty() {
				TextIterator::Scope scope( textIterator );				

				while( textIterator.tryMatchAny( " \t" ) )
					;

				return textIterator.peek() == '\n';
			}

			std::string parseIndentedText() {
				std::string text;

				bool isEmptyLine;
				while( !textIterator.atEof() && ( (isEmptyLine = checkRestOfLineEmpty()) || checkMinimumIndentLevel() )  ) {
					if( isEmptyLine ) {
						textIterator.readLine();
						text.push_back( '\n' );
					}
					else {
						skipIndentLevel();

						text.append( textIterator.readLine() );	
					}
				}

				// remove the last newline
				if( !text.empty() ) {
					text.pop_back();
				}
				
				return text;
			}

			std::string parseValue() {
				if( textIterator.check( '"' ) ) {
					return parseEscapedString();
				}
				else if( textIterator.check( '\'' ) ) {
					return parseUnescapedString();
				}
				else {
					return parseIdentifier();
				}
			}

			void expectNewline() {
				if( !textIterator.tryMatch( '\n' ) ) {
					textIterator.error( "expected newline!" );
				}
			}

			void parseMap( Node &node, bool allowEmpty = true ) {
				while( true ) {
					skipEmptyLines();

					if( !checkMinimumIndentLevel() || textIterator.atEof() ) {
						break;
					}

					ensureIndentLevel();
					skipWhitespace();

					std::string key = parseValue();

					Node childNode( key );

					skipWhitespace();

					if( textIterator.tryMatch( ':' ) ) {
						skipWhitespace();

						if( textIterator.tryMatch( ':' ) ) {
							// text literal
							skipWhitespace();
							expectNewline();

							indentLevel++;
							std::string indentedText = parseIndentedText();
							indentLevel--;

							childNode.items.push_back( Node( indentedText ) );
						}
						else {
							expectNewline();

							indentLevel++;
							parseMap( childNode, false );
							indentLevel--;
						}
					}
					else {
						parseInlineValues( childNode );
					}

					node.items.push_back( std::move( childNode ) );
				}
				if( node.items.empty() && !allowEmpty ) {
					textIterator.error( "expected non-empty map" );
				}
			}

			void parseInlineValues( Node &node ) {
				while( !textIterator.atEof() && !textIterator.tryMatch( '\n' ) ) {
					std::string value = parseValue();

					node.items.push_back( Node( value ) );

					skipWhitespace();
				}
			}

			Parser( const TextContainer &textContainer ) : indentLevel( 0 ), textIterator( textContainer, TextPosition() ) {}
		};

		inline Node parse( const std::string &text, const std::string &textIdentifier ) {
			TextContainer textContainer( text, textIdentifier );

			Parser parser( textContainer );

			Node node;
			parser.parseMap( node );
			return node;
		}

		struct Emitter {
			int indentLevel;
			std::string text;

			Emitter() : indentLevel( 0 ) {}

			void emitTabs() {
				for( int i = 0 ; i < indentLevel ; ++i ) {
					text.push_back( '\t' );
				}
			}

			enum ValueType {
				VT_IDENTIFIER,
				VT_UNESCAPED_STRING,
				VT_ESCAPED_STRING,
				VT_TEXT
			};

			static ValueType determineType( const std::string &value ) {
				int numLines = 1;

				for( auto c = value.cbegin() ; c != value.cend() ; ++c ) {
					if( *c == '\n' ) {
						numLines++;						
					}					
				}

				if( numLines > 2 ) {
					return VT_TEXT;
				}

				for( auto c = value.cbegin() ; c != value.cend() ; ++c ) {
					if( *c == '\n' || *c == '\t' || *c == '\'' ) {
						return VT_ESCAPED_STRING;				
					}
				}

				for( auto c = value.cbegin() ; c != value.cend() ; ++c ) {
					if( *c == ' ' || *c == ':' ) {
						return VT_UNESCAPED_STRING;
					} 
				}

				return VT_IDENTIFIER;
			}

			void emitValue( const std::string &value, bool noTextBlocks ) {
				ValueType vt = determineType( value );

				if( vt == VT_TEXT && noTextBlocks ) {
					vt = VT_ESCAPED_STRING;
				}

				if( vt == VT_IDENTIFIER ) {
					text.append( value );
				}
				else if( vt == VT_UNESCAPED_STRING ) {
					text.push_back( '\'' );
					text.append( value );
					text.push_back( '\'' );
				}
				else if( vt == VT_TEXT ) {
					text.append( "::\n" );
					
					++indentLevel;

					emitTabs();
					for( auto c = value.cbegin() ; c != value.cend() ; ++c ) {
						text.push_back( *c );
						if( *c == '\n' ) {
							emitTabs();
						}
					}				

					text.push_back( '\n' );
					--indentLevel;
				}
				else if( vt == VT_ESCAPED_STRING ) {
					text.push_back( '\"' );

					for( auto c = value.cbegin() ; c != value.cend() ; ++c ) {
						if( *c == '\\' || *c == '\"' ) {
							text.push_back( '\\' );
							text.push_back( *c );
						}
						else if( *c == '\t' ) {
							text.append( "\\t" );
						}
						else if( *c == '\n' ) {
							text.append( "\\n" );
						}
						else {
							text.push_back( *c );
						}
					}

					text.push_back( '\"' );
				}
			}

			static bool isMap( const Node &node ) {
				for( auto item = node.items.begin() ; item != node.items.end() ; ++item ) {
					if( !item->empty() ) {
						return true;
					}
				}
				return false;
			}

			static bool isTextBlock( const Node &node ) {
				return node.size() == 1 && determineType( node.items[0].value ) == VT_TEXT;
			}

			void emitInlineValues( const Node &node ) {
				for( auto item = node.items.begin() ; item != node.items.end() ; ++item ) {
					text.push_back( ' ' );

					emitValue( item->value, true );
				}
				text.push_back( '\n' );
			}

			void emitMap( const Node &node ) {
				for( auto item = node.items.begin() ; item != node.items.end() ; ++item ) {
					emitTabs();
					// emit the key
					emitValue( item->value, true );

					if( isMap( *item ) ) {
						text.append( ":\n" );
						++indentLevel;
						emitMap( *item );
						--indentLevel;
					}
					else if( isTextBlock( *item ) ) {
						emitValue( item->items[0].value, false );
					}
					else {
						emitInlineValues( *item );
					}
				}
			}
		};

		std::string emit( const Node &node ) {
			Emitter emitter;
			emitter.emitMap( node );
			return emitter.text;
		}
	}

	Node parse( const std::string &text, const std::string &textIdentifier = "" ) {
		return detail::parse( text, textIdentifier );		
	}

	std::string emit( const Node &node ) {
		return detail::emit( node );
	}
}