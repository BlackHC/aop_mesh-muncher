#pragma once

#include "widgets.h"

struct SGSSceneRenderer;

struct ModelButtonWidget : ButtonWidget {
	SGSSceneRenderer &renderer;
	int modelIndex;
	
	// use to rotate the object continuously
	float time;

	ModelButtonWidget( const Eigen::Vector2f &offset, const Eigen::Vector2f &size, int modelIndex, SGSSceneRenderer &renderer ) :
		ButtonWidget( offset, size ),
		modelIndex( modelIndex ),
		renderer( renderer ),
		time( 0 )
	{}

	void doUpdate( EventSystem &eventSystem, const float frameDuration, const float elapsedTime ) {
		time = elapsedTime;
	}

	void doRenderInside();
	
	void onAction() {}
	void onMouseEnter() {}
	void onMouseLeave() {}
};
