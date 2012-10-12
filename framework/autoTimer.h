#pragma once

#include <boost/timer/timer.hpp>
#include <boost/format.hpp>

#include "logger.h"

struct AutoTimer {
	Log::Scope logScope;

	boost::timer::cpu_timer timer;

	AutoTimer( const std::string &function, const boost::format &format ) {
		init( function, format.str() );
	}

	AutoTimer( const std::string &function, const std::string &msg = std::string() ) {
		init( function, msg );
	}

	void init( const std::string &function, const std::string &msg ) {
		if( !msg.empty() ) {
			logScope.log( function + ", " + msg + ":", -1 );
		}
		else {
			logScope.log( function + ":", -1 );
		}
	}

	operator bool() { return true; }

	~AutoTimer() {
		timer.stop();
		logScope.log( timer.format( 6, "* %ws wall, %us user + %ss system = %ts CPU (%p%)" ) );
	}
};

#define _AUTO_TIMER_MERGE_2( x, y ) x ## y
#define _AUTO_TIMER_MERGE( x, y ) _AUTO_TIMER_MERGE_2( x, y )
// TODO: name refactoring [10/1/2012 kirschan2]
#define AUTO_TIMER_FOR_FUNCTION(...) AutoTimer _AUTO_TIMER_MERGE( _auto_timer, __COUNTER__ )( __FUNCTION__, __VA_ARGS__ )
#define AUTO_TIMER_NAMED( name, ... ) AutoTimer name( __FUNCTION__, __VA_ARGS__ )
#define AUTO_TIMER_DEFAULT( ... ) AutoTimer autoTimer( __FUNCTION__, __VA_ARGS__ )

#define AUTO_TIMER_BLOCK( ... ) if( auto scopedTimer = AutoTimer( __FUNCTION__, __VA_ARGS__ ) )
#define AUTO_TIMER_MEASURE( ... ) if( auto scopedTimer = AutoTimer( __FUNCTION__, __VA_ARGS__ ) )