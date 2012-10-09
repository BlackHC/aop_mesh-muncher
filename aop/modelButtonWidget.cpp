#include "modelButtonWidget.h"

#include <sgsSceneRenderer.h>
#include <gl/glew.h>

#include "viewportContext.h"

using namespace Eigen;

void ModelButtonWidget::doRenderInside() {
	const Eigen::Vector2f realMinCorner = transformChain.pointToScreen( Eigen::Vector2f::Zero() );
	const Eigen::Vector2f realSize = transformChain.vectorToScreen( size );

	// init the persective matrix
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadMatrix( Eigen::createPerspectiveProjectionMatrix( 60.0, realSize[0] / realSize[1], 0.001, 100.0 ) );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	const auto worldViewerPosition = Vector3f( 0.0, 1.0, 1.0 );

	const auto boundingBox = renderer.getModelBoundingBox(modelIndex);
	const auto boundingBoxSize = boundingBox.sizes();

	const float maxWidth = std::max( boundingBoxSize.x(), boundingBoxSize.z() );
	const float maxDiagonal = sqrt( maxWidth * maxWidth + boundingBoxSize.y() * boundingBoxSize.y() );
	const float scaleFactor = 2 * sqrt( 2.0 / 3.0 ) / (maxDiagonal * sqrt(2.0));

	glLoadMatrix( Eigen::createLookAtMatrix( worldViewerPosition, Vector3f::Zero(), Vector3f::UnitY() ) );

	// see note R3 for the computation
	Eigen::glScale( Vector3f::Constant( scaleFactor ) );

	float angle = time * 2 * Math::PI / 10.0;
	glMultMatrix( Affine3f(AngleAxisf( angle, Vector3f::UnitY() )).matrix() );

	glTranslate( -boundingBox.center() );

	// TODO: scissor test [10/4/2012 kirschan2]

	glClear( GL_DEPTH_BUFFER_BIT );
	glEnable( GL_DEPTH_TEST );

	{
		ViewportContext viewport( realMinCorner[0], realMinCorner[1], realSize[0], realSize[1] );

		renderer.renderModel( worldViewerPosition, modelIndex );
	}

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glDisable( GL_DEPTH_TEST );
}
