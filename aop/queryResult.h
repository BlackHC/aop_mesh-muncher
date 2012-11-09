#pragma once

#include <Eigen/Eigen>
#include "boost/tuple/tuple.hpp"
#include <vector>

struct QueryResult {
	float score;
	int sceneModelIndex;

	Eigen::Affine3f transformation;

	QueryResult()
		: score()
		, sceneModelIndex()
		, transformation( Eigen::Affine3f::Identity() )
	{
	}

	QueryResult( int sceneModelIndex )
		: score()
		, sceneModelIndex( sceneModelIndex )
		, transformation( Eigen::Affine3f::Identity() )
	{
	}

	QueryResult( float score, int sceneModelIndex, const Eigen::Affine3f &transformation )
		: score( score )
		, sceneModelIndex( sceneModelIndex )
		, transformation( transformation )
	{
	}

	static bool greaterByScoreAndModelIndex( const QueryResult &a, const QueryResult &b ) {
		return
				boost::make_tuple( a.score, a.sceneModelIndex )
			>
				boost::make_tuple( b.score, b.sceneModelIndex )
		;
	}
};

typedef std::vector< QueryResult > QueryResults;