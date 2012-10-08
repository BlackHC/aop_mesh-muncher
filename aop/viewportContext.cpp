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

// relative coords are 0..1
Eigen::Vector2f ViewportContext::screenToViewport( const Eigen::Vector2i &screen ) const {
	return Eigen::Vector2f( (screen[0] - left) / float( width ), (screen[1] - top) / float( height ) );
}