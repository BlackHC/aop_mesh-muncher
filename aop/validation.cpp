#include "validation.h"

#include "boost/range/algorithm/sort.hpp"
#include <xfunctional>

namespace Validation {
	float InstanceCounts::calculateRankExpectation() const {
		auto sortedInstanceCounts = instanceCounts;
		boost::sort( sortedInstanceCounts, std::greater<int>() );

		float expectation = 0.0f;
		for( int rank = 0 ; rank < sortedInstanceCounts.size() ; ++rank ) {
			const float frequency = float( sortedInstanceCounts[ rank ] ) / totalNumInstances;
			expectation += frequency * rank;
		}

		return expectation;
	}
}