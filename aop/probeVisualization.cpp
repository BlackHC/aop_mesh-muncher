#include "debugRender.h"
#include "optixEigenInterop.h"
#include "sgsInterface.h"

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes ) {
	DebugRender::begin();
	glColor3f( 0.0, 1.0, 1.0 );
	glBegin( GL_LINES );
	for( auto probe = probes.begin() ; probe != probes.end() ; ++probe ) {
		Eigen::glVertex( Eigen::map( probe->position ) );
		Eigen::glVertex( Eigen::map( probe->position ) + Eigen::map( probe->direction ) * resolution / 3 );
	}
	glEnd();
	DebugRender::end();
}