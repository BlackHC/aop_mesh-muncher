#pragma once

#include <boost/format.hpp>
#include <string>
#include <exception>

namespace LeanTextProcessing {
		struct TextPosition {
		int line;
		int column;

		int index;

		TextPosition() : line( 1 ), column( 1 ), index( 0 ) {}

		void increment( bool newLine ) {
			++index;
			if( newLine ) {
				++line;
				column = 1;
			}
		}
	};

	struct TextContainer {
		std::string textIdentifier;

		std::string text;

		TextContainer( const std::string &text, const std::string &textIdentifier ) : text( text ), textIdentifier( textIdentifier ) {}
	};

	struct TextException;

	struct TextIterator {
		const TextContainer &textContainer;
		TextPosition current;

		TextIterator( const TextContainer &textContainer, const TextPosition &position ) : textContainer( textContainer ), current( position ) {}

		bool atEof() const {
			return current.index >= textContainer.text.size();
		}

		char peek() const {
			// assert !atEof()
			return textContainer.text[ current.index ];
		}

		void next() {
			current.increment( peek() == '\n' );
		}

		// helper class
		struct Scope {
			TextIterator *iterator;
			TextPosition saved;

			std::string getScopedText() {
				return iterator->textContainer.text.substr( saved.index, iterator->current.index - saved.index );
			}

			void accept() {
				if( iterator ) {
					saved = iterator->current;
				}
			}

			void reject() {
				if( iterator ) {
					iterator->current = saved;
				}
			}

			void release() {
				iterator = nullptr;
			}

			Scope( TextIterator &iterator ) : iterator( &iterator ), saved( iterator.current ) {}
			~Scope() {
				reject();
			}
		};

		// helper methods
		char read() {
			char c = peek();
			next();
			return c;			
		}
		
		bool tryMatch( const char c ) {
			if( !atEof() && peek() == c ) {
				next();
				return true;
			}
			return false;
		}

		bool tryMatch( const char *text ) {
			for( const char *p = text ; *p ; ++p ) {
				if( atEof() || peek() != *p ) {
					return false;
				}
				next();
			}
			return true;
		}

		bool tryMatchAny( const char *set ) {
			if( atEof() ) {
				return false;
			}

			for( const char *p = set ; *p ; ++p ) {
				if( peek() == *p ) {
					next();
					return true;
				}
			}
			return false;
		}

		bool check( const char c ) const {
			return !atEof() && peek() == c;
		}

		bool checkAny( const char *set ) {
			if( atEof() ) {
				return false;
			}

			for( const char *p = set ; *p ; ++p ) {
				if( peek() == *p ) {
					return true;
				}
			}
			return false;
		}

		bool checkNotAny( const char *cset ) {
			if( atEof() ) {
				return false;
			}

			for( const char *p = cset ; *p ; ++p ) {
				if( peek() == *p ) {
					return false;
				}
			}
			return true;
		}

		std::string readLine() {
			std::string text;

			do {
				text.push_back( peek() ); 
			}
			while( read() != '\n' );

			return text;
		}

		void error( const std::string &error );
	};

	struct TextException : std::exception {
		std::string textIdentifier;
		std::string text;
		TextPosition position;

		std::string error;

		std::string message;

		TextException( const TextIterator &iterator, const std::string &error ) : textIdentifier( iterator.textContainer.textIdentifier ), text( iterator.textContainer.text ), position( iterator.current ), error( error ) {
			message = boost::str( boost::format( "%s(%i:%i (%i)): %s" ) % textIdentifier % position.line % position.column % position.index % error );
		}

		virtual const char * what() const {
			return message.c_str();
		}
	};

	void TextIterator::error( const std::string &error ) {
		throw TextException( *this, error );
	}
}
