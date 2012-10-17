#include "viewportContext.h"

#include <gl/glew.h>

ViewportContext::ViewportContext( int framebufferWidth, int framebufferHeight ) :
	framebufferWidth( framebufferWidth ), framebufferHeight( framebufferHeight ),
	left( 0 ), top( 0 ), width( framebufferWidth ), height( framebufferHeight )
{
	updateGLViewport();
}

ViewportContext::ViewportContext( int left, int top, int width, int height ) :
	left( left ), top( top ), width( width ), height( height )
{
	updateGLViewport();
}

ViewportContext::ViewportContext() : AsExecutionContext( ExpectNonEmpty() ) {}

void ViewportContext::onPop() {
	updateGLViewport();
}

void ViewportContext::updateGLViewport() {
	glViewport( left, framebufferHeight - (top + height), width, height );
}

float ViewportContext::getAspectRatio() const {
	return float( framebufferWidth ) / framebufferHeight;
}

#if 0
Eigen::Vector2f ViewportContext::screenToViewport( const Eigen::Vector2i &screen ) const {
	return Eigen::Vector2f( (screen[0] - left) / float( width ), (top - screen[1]) / float( height ) );
}

Eigen::Vector2i ViewportContext::viewportToScreen( const Eigen::Vector2f &viewportCoords ) const {
	return Eigen::Vector2f( left + width * viewportCoords.x(), top - height * viewportCoords.y() );
}
#endif