#pragma once

#include <contextHelper.h>

#include <Eigen/Eigen>

// supports y down coordinates and sets the viewport automatically
struct ViewportContext : AsExecutionContext< ViewportContext > {
	// total size
	int framebufferWidth;
	int framebufferHeight;

	// viewport
	int left, top;
	int width, height;

	ViewportContext( int framebufferWidth, int framebufferHeight );
	ViewportContext( int left, int top, int width, int height );
	ViewportContext();

	void onPop();
	void updateGLViewport();
	float getAspectRatio() const;

	Eigen::Vector2i globalToGL( const Eigen::Vector2i &screenCords ) const {
		return Eigen::Vector2i( screenCords[0], height - screenCords[1] );
	}

#if 0
	// currently unused 
	// viewport coords are relative 0..1
	// screen coords are the real in absolute OpenGL window coords
	Eigen::Vector2f screenToViewport( const Eigen::Vector2i &screenCoords ) const;	
	Eigen::Vector2i viewportToScreen( const Eigen::Vector2f &viewportCoords ) const;
#endif
};