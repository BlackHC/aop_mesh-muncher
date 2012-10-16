#pragma once

#include <Eigen/Eigen>

struct RenderContext {
	int disabledInstanceIndex;
	int disabledModelIndex;

	RenderContext() {
		setDefault();
	}

	void setDefault() {
		disabledInstanceIndex = -1;
		disabledModelIndex = -1;
	}
};

struct ViewerContext {
	Eigen::Matrix4f projectionView;
	Eigen::Vector3f worldViewerPosition;
};