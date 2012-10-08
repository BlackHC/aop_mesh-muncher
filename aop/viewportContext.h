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

	// relative coords are 0..1
	Eigen::Vector2f screenToViewport( const Eigen::Vector2i &screen ) const;
};