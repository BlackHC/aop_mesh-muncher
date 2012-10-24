#include "visualizations.h"

#include "debugRender.h"
#include "optixEigenInterop.h"
#include "sgsInterface.h"

using namespace Eigen;

void visualizeProbes( float resolution, const std::vector< SGSInterface::Probe > &probes ) {
	DebugRender::begin();
	glColor3f( 0.0, 1.0, 1.0 );
	glBegin( GL_LINES );
	for( auto probe = probes.begin() ; probe != probes.end() ; ++probe ) {
		glVertex( map( probe->position ) );
		glVertex( map( probe->position ) + map( probe->direction ) * resolution / 3 );
	}
	glEnd();
	DebugRender::end();
}

void visualizeColorGrid( const VoxelizedModel::Voxels &grid, GridVisualizationMode gvm ) {
	const float size = grid.getMapping().getResolution();

	DebugRender::begin();
	for( auto iterator = grid.getIterator() ; iterator.hasMore() ; ++iterator ) {
		const auto &normalHit = grid[ *iterator ];

		if( normalHit.numSamples != 0 ) {
			DebugRender::setPosition( grid.getMapping().getPosition( iterator.getIndex3() ) );

			Vector3f positionColor = iterator.getIndex3().cast<float>().cwiseQuotient( grid.getMapping().getSize().cast<float>() );

			switch( gvm ) {
			case GVM_POSITION:
				DebugRender::setColor( positionColor );
				break;
			case GVM_HITS:
				DebugRender::setColor( Vector3f::UnitY() * (0.5f + normalHit.numSamples / 128.0f) );
				break;
			case GVM_NORMAL:
				glColor3ubv( &normalHit.nx );
				break;
			}

			DebugRender::drawBox( Vector3f::Constant( size ), false );
		}
	}
	DebugRender::end();
}

void visualizeProbe(
	const Eigen::Vector3f &missColor,
	const RawProbe &probe,
	const RawProbeContext &probeContext,
	float maxDistance,
	float resolution,
	ProbeVisualizationMode pvm
) {
	// sin and cos of 22.5°
	const float directionDistance = 0.92387953251f * 0.5f * resolution;
	const float radius = 0.38268343236f * 0.5f * resolution ;

	const auto direction = map( probe.direction );

	DebugRender::setPosition( map( probe.position ) + direction * directionDistance );

	// build the two axes
	const Eigen::Vector3f axis1 = direction.unitOrthogonal();
	const Eigen::Vector3f axis2 = direction.cross( axis1 );

	const float occlusionFactor = float( probeContext.hitCounter ) / OptixProgramInterface::numProbeSamples;

	switch( pvm ) {
	case PVM_COLOR:
#if 0
		// blend with missColor depending on the occlusion factor
		DebugRender::setColor(
					map( OptixProgramInterface::CIELAB::toRGB(
							optix::make_float3( probeContext->Lab.x, probeContext->Lab.y, probeContext->Lab.z )
					) )
				*
					occlusionFactor
			+
				(1.0f - occlusionFactor) * missColor
		);
#else
		if( probeContext.hitCounter > 0 ) {
			DebugRender::setColor(
				map( OptixProgramInterface::CIELAB::toRGB(
					optix::make_float3( probeContext.Lab.x, probeContext.Lab.y, probeContext.Lab.z )
				) )
			);
		}
		else {
			DebugRender::setColor( missColor );
		}
#endif
		break;
	case PVM_OCCLUSION:
		DebugRender::setColor(
			(1.0f - occlusionFactor) * Vector3f::Constant( 1.0f )
		);
		break;
	case PVM_DISTANCE:
		if( occlusionFactor > 0 ) {
			DebugRender::setColor( probeContext.distance / maxDistance * Vector3f::Constant( 1.0f )	);
		}
		else {
			DebugRender::setColor( missColor );
		}
		break;
	}

	// draw the probe
	DebugRender::drawEllipse( radius, false, 20, axis1, axis2 );
}

void visualizeRawProbeContexts(
	const Eigen::Vector3f &missColor,
	float maxDistance,
	float resolution,
	const RawProbes &probes,
	const RawProbeContexts &probeContexts,
	ProbeVisualizationMode pvm
) {
	if( probes.size() != probeContexts.size() ) {
		throw std::invalid_argument( "probes.size() != probeContexts.size()" );
	}
	DebugRender::begin();
	const int numProbes = (int) probes.size();
	for( int probeIndex = 0 ; probeIndex < numProbes ; probeIndex++ ) {
		visualizeProbe(
			missColor,
			probes[ probeIndex ],
			probeContexts[ probeIndex ],
			maxDistance,
			resolution,
			pvm
		);
	}
	DebugRender::end();
}

void visualizeProbeDataset(
	const Eigen::Vector3f &missColor,
	float maxDistance,
	float resolution,
	const DBProbes &probes,
	const DBProbeContexts &probeContexts,
	ProbeVisualizationMode pvm
) {
	DebugRender::begin();
	for( auto probeContext = probeContexts.begin() ; probeContext != probeContexts.end() ; ++probeContext ) {
		const auto &probe = probes[ probeContext->probeIndex ];

		visualizeProbe(
			missColor,
		 	probe,
			*probeContext,
			maxDistance,
			resolution,
			pvm
		 );
	}
	DebugRender::end();
}