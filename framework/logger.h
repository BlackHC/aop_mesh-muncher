#pragma once

#include <string>
#include <functional>
#include <boost/format.hpp>

namespace Log {
	enum Type {
		T_LOG,
		T_ERROR
	};

	// returns false if the sink should be removed
	typedef bool MessageSink( int scope, const std::string &message, Type type );

	void addSink( const std::function< MessageSink > &messageSink );

	int getScope();

	int pushScope();
	void popScope();

	namespace Sinks {
		void addStdio();
	}

	struct Scope {
		int scope;

		Scope();
		Scope( const std::string &message );
		Scope( const boost::format &message );
		~Scope();

		void log( const std::string &message, int relativeScope = 0 );
		void log( const boost::format &message, int relativeScope = 0 );

		void logError( const std::string &message, int relativeScope = 0 );
		void logError( const boost::format &message, int relativeScope = 0 );
	};

	// thread index is unused atm!
	void initThreadScope( int baseScope, int threadIndex );

	namespace Utility {
		std::string scopeToIndentation( int scope );

		// adds a newline at the end
		std::string indentString( int scope, const std::string &message, const std::string &customPrefix = std::string() );
	}

	const int CURRENT_SCOPE = -1;
}

void log( const std::string &message, int scope = Log::CURRENT_SCOPE );
void log( const boost::format &message, int scope = Log::CURRENT_SCOPE );

void logError( const std::string &message, int scope = Log::CURRENT_SCOPE );
void logError( const boost::format &message, int scope = Log::CURRENT_SCOPE );

//////////////////////////////////////////////////////////////////////////
inline Log::Scope::Scope() {
	scope = pushScope();
}

inline Log::Scope::Scope( const std::string &message ) {
	log( message );
	scope = pushScope();
}

inline Log::Scope::Scope( const boost::format &message ) {
	log( message );
	scope = pushScope();
}

inline Log::Scope::~Scope() {
	popScope();
}

inline void Log::Scope::log( const std::string &message, int relativeScope ) {
	::log( message, scope + relativeScope );
}

inline void Log::Scope::log( const boost::format &message, int relativeScope ) {
	::log( message, scope + relativeScope );
}

inline void Log::Scope::logError( const std::string &message, int relativeScope ) {
	::logError( message, scope + relativeScope );
}

inline void Log::Scope::logError( const boost::format &message, int relativeScope ) {
	::logError( message, scope + relativeScope );
}