#include "logger.h"

#include <vector>
#include <iostream>

#include <SFML/System/Lock.hpp>
#include <SFML/System/Mutex.hpp>

namespace Log {
	namespace {
		std::vector< std::function< MessageSink > > messageSinks;

		__declspec( thread ) int currentScope = 0;
		__declspec( thread ) int threadIndex = 0;
		
		sf::Mutex mutex;
	}

	namespace Utility {
		std::string scopeToIndentation( int scope ) {
			return std::string( scope, ' ' );
		}

		std::string indentString( int scope, const std::string &message, const std::string &customPrefix ) {
			const std::string indentation = customPrefix + scopeToIndentation( scope );

			std::string indentedString;

			size_t lastIndex = 0;
			while( true ) {
				size_t index = message.find( '\n', lastIndex );
				if( index == std::string::npos ) {
					break;
				}
				index += 1;

				indentedString += indentation + message.substr( lastIndex, index - lastIndex );
				lastIndex = index;
			}
			indentedString += indentation + message.substr( lastIndex ) + "\n";

			return indentedString;
		}
	}

	void addSink( const std::function< MessageSink > &messageSink ) {
		messageSinks.push_back( messageSink );
	}

	int getScope() {
		return currentScope;
	}

	int pushScope() {
		currentScope++;
		return currentScope;
	}

	void popScope() {
		if( !currentScope ) {
			__debugbreak();
		}

		currentScope--;
	}

	void initThreadScope( int baseScope, int newThreadIndex ) {
		if( currentScope && currentScope != baseScope ) {
			__debugbreak();
		}
		currentScope = baseScope;
		threadIndex = newThreadIndex;
	}

	namespace {
		void add( int scope, const std::string &message, enum Type type ) {
			sf::Lock lock( mutex );

			if( !messageSinks.empty() ) {
				for( auto sink = messageSinks.rbegin() ; sink != messageSinks.rend() ; sink++ ) {
					if( !(*sink)( scope, message, type ) ) {
						// this is a very ugly hack, but I cant think of a better way right now
						messageSinks.erase( (sink+1).base() );
					}
				}
			}
			else {
				std::cerr << Utility::indentString( scope, message );
			}
		};
	}

	namespace Sinks {
		void addStdio() {
			addSink(
				[] ( int scope, const std::string &message, enum Type type ) -> bool {
					const std::string indentedMessage = Utility::indentString( scope, message );
					switch( type ) {
					case T_ERROR:
						std::cerr << indentedMessage;
						break;
					case T_LOG:
						std::cout << indentedMessage;
						break;
					}
					return true;
				}
			);
		}
	}
}

static int translateScope( int scope ) {
	if( scope == Log::CURRENT_SCOPE ) {
		return Log::currentScope;
	}
	return scope;
}

void log( const std::string &message, int scope ) {
	Log::add( translateScope( scope ), message, Log::T_LOG );
}

void log( const boost::format &message, int scope ) {
	Log::add( translateScope( scope ), message.str(), Log::T_LOG );
}

void logError( const std::string &message, int scope ) {
	Log::add( translateScope( scope ), message, Log::T_ERROR );
}

void logError( const boost::format &message, int scope ) {
	Log::add( translateScope( scope ), message.str(), Log::T_ERROR );
}