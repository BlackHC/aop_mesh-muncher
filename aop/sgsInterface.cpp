#include "sgsInterface.h"
#include "mathUtility.h"
#include "probeGenerator.h"

namespace SGSInterface {
	void generateProbes( int instanceIndex, float resolution, SGSSceneRenderer &renderer, std::vector<Probe> &probes, std::vector<Probe> &transformedProbes ) {
		const Eigen::AlignedBox3f &sgsBoundingBox = renderer.getUntransformedInstanceBoundingBox( instanceIndex );
		const Eigen::Matrix4f &sgsTransformation = renderer.getInstanceTransformation( instanceIndex );

		const OBB obb = makeOBB( sgsTransformation, sgsBoundingBox );
		ProbeGenerator::generateInstanceProbes( obb.size, resolution, probes );
		ProbeGenerator::transformProbes( probes, obb.transformation, transformedProbes );
	}
}