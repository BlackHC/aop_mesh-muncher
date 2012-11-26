#include "aopCandidateSidebarUI.h"

#include "viewportContext.h"

void aop::LocalCandidateBarUI::init() {
	application->widgetRoot.addEventHandler( make_nonallocated_shared( clipper ) );
	clipper.addEventHandler( make_nonallocated_shared( scroller ) );

	const float buttonWidth = 0.1f;
	const float buttonAbsPadding = 32;
	const float buttonVerticalPadding = buttonAbsPadding / ViewportContext::context->framebufferHeight;
	const float buttonHorizontalPadding = buttonAbsPadding / ViewportContext::context->framebufferWidth;

	const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
	const float barHeight = buttonHeight / 8.0f;

	const float totalEntryHeight = buttonHeight + barHeight + buttonVerticalPadding;

	//const int numQueryResults = std::min<int>( 30, (int) queryResults.size() );
	const int numQueryResults = queryResults.size();
	const int numDisplayedModels = std::min<int>( numQueryResults, static_cast< int >( 1.0f / totalEntryHeight ) );
	const float totalHeight = totalEntryHeight * numDisplayedModels;

	scroller.size[0] = buttonWidth;
	scroller.size[1] = totalHeight;

	scroller.scrollStep = totalEntryHeight;

	for( int i = 0 ; i < numQueryResults ; i++ ) {
		const float score = queryResults[ i ].score;
		const int modelIndex = queryResults[i].sceneModelIndex;

		scroller.addEventHandler(
			std::make_shared< ActionModelButton >(
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight ),
				Eigen::Vector2f( buttonWidth, buttonHeight ),
				modelIndex,
				application->world->sceneRenderer,
				[this, i] () {
					if( sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) ) {
						log( application->world->scene.modelNames[ queryResults[i].sceneModelIndex ] );
						application->editor.selectModel( queryResults[i].sceneModelIndex );
					}
					else {
						application->world->addInstance( queryResults[i].sceneModelIndex, queryResults[i].transformation );
					}
				}
			)
		);
		scroller.addEventHandler(
			std::make_shared< ProgressBarWidget >(
				score,
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight + buttonHeight ),
				Eigen::Vector2f( buttonWidth, barHeight )
			)
		);
	}

	scroller.updateScrollArea();
	clipper.updateLocalArea();
}

void aop::LocalCandidateBarUI::refresh() {
	const auto pos = application->cameraView.viewerContext.projectionView * queryObb.transformation.translation().homogeneous().matrix();
	if( pos[2] < -pos[3] ) {
		clipper.visible = false;
		return;
	}
	else {
		clipper.visible = true;
	}

	const auto screenBox = Eigen_getTransformedAlignedBox( application->cameraView.viewerContext.projectionView * queryObb.transformation, queryObb.asLocalAlignedBox3f() );

	Eigen::Vector2f offset = screenBox.corner( Eigen::AlignedBox3f::TopRightCeil ).head<2>();
	offset.y() *= -1.0f;

	const float scale = std::max<float>( 0.025f, screenBox.sizes().head<2>().norm() ) / 4.0f; // 4.0 = screenWidth * screenHeight in -1..1x-1..1 coords

	clipper.transformChain.setOffset( offset * 0.5f + Eigen::Vector2f::Constant( 0.5f ) );
	clipper.transformChain.setScale( 2.0f * scale );
}

void aop::CandidateSidebarUI::setModels( const QueryResults &_queryResults  ) {
	clear();
	queryResults = _queryResults;

	const float buttonWidth = 0.1f;
	const float buttonAbsPadding = 32;
	const float buttonVerticalPadding = buttonAbsPadding / ViewportContext::context->framebufferHeight;
	const float buttonHorizontalPadding = buttonAbsPadding / ViewportContext::context->framebufferWidth;

	// TODO: this was in init but meh
	sidebar.transformChain.setOffset( Eigen::Vector2f( 1 - buttonHorizontalPadding - buttonWidth, 0 ) );

	const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
	const float barHeight = buttonHeight / 8.0f;

	const float totalEntryHeight = buttonHeight + barHeight + buttonVerticalPadding;

	/*
	const int maxNumCandidates = 30;
	const int numQueryResults = std::min<int>( maxNumCandidates, queryResults.size() );
	*/
	const int numQueryResults = queryResults.size();

	sidebar.minY = 0;
	sidebar.maxY = (numQueryResults - 1 ) * totalEntryHeight;
	sidebar.scrollStep = totalEntryHeight;

	for( int i = 0 ; i < numQueryResults ; i++ ) {
		const float score = queryResults[ i ].score;
		const int modelIndex = queryResults[i].sceneModelIndex;
		Eigen::Affine3f transformation = queryResults[i].transformation;

		sidebar.addEventHandler(
			std::make_shared< ActionModelButton >(
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight ),
				Eigen::Vector2f( buttonWidth, buttonHeight ),
				modelIndex,
				application->world->sceneRenderer,
				[this, i] () {
					if( sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) ) {
						log( application->world->scene.modelNames[ queryResults[i].sceneModelIndex ] );
						application->editor.selectModel( queryResults[i].sceneModelIndex );
					}
					else {
						application->world->addInstance( queryResults[i].sceneModelIndex, queryResults[i].transformation );
					}
			}
		) );
		sidebar.addEventHandler(
			std::make_shared< ProgressBarWidget >(
				score,
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight + buttonHeight ),
				Eigen::Vector2f( buttonWidth, barHeight )
			)
		);
	}
}
