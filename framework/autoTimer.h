#pragma once

#include <boost/timer/timer.hpp>
#include <boost/format.hpp>

struct Indentation {
	static int currentIndentation;

	static std::string get( int sub = 0 ) {
		return std::string( currentIndentation + sub, ' ' );
	}

	struct Scope {
		Scope() {
			++currentIndentation;
		}

		~Scope() {
			--currentIndentation;
		}
	};
};

struct AutoTimer {
	Indentation::Scope indentationScope;
	boost::timer::cpu_timer timer;

	AutoTimer( const char *function, const std::string &msg = std::string() ) {
		std::cerr << Indentation::get( -1 ) << function;
		if( !msg.empty() ) {
			std::cerr << ", " << msg;
		}
		std::cerr << ":\n";
	}

	std::string indentation() {
		return Indentation::get();
	}

	~AutoTimer() {
		timer.stop();
		std::cerr << indentation() << timer.format( 6, "* %ws wall, %us user + %ss system = %ts CPU (%p%)\n" ) << "\n";
	}
};

#define _AUTO_TIMER_MERGE_2( x, y ) x ## y
#define _AUTO_TIMER_MERGE( x, y ) _AUTO_TIMER_MERGE_2( x, y )
// TODO: name refactoring [10/1/2012 kirschan2]
#define AUTO_TIMER_FOR_FUNCTION(...) AutoTimer _AUTO_TIMER_MERGE( _auto_timer, __COUNTER__ )( __FUNCTION__, __VA_ARGS__ )
#define AUTO_TIMER_NAMED( name, ... ) AutoTimer name( __FUNCTION__, __VA_ARGS__ )
#define AUTO_TIMER_DEFAULT( ... ) AutoTimer autoTimer( __FUNCTION__, __VA_ARGS__ )