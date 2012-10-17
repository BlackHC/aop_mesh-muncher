#include "aopCandidateSidebarUI.h"

void aop::LocalCandidateContainer::onRender() {
	if( !visible ) {
		return;
	}

	const auto &topLeft = ViewportContext::context->globalToGL( transformChain.pointToScreen( Eigen::Vector2f::Zero() ).cast<int>() );
	const auto &bottomRight = ViewportContext::context->globalToGL( transformChain.pointToScreen( size ).cast<int>() );

	glEnable( GL_SCISSOR_TEST );
	glScissor( topLeft.x(), bottomRight.y(), bottomRight.x() - topLeft.x() + 1, topLeft.y() - bottomRight.y() + 1 );

	CandidateContainer::onRender();

	glDisable( GL_SCISSOR_TEST );
}

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

	const int numModels = std::min<int>( 30, candidates.size() );
	const int numDisplayedModels = std::min<int>( numModels, 1.0 / totalEntryHeight );
	const float totalHeight = totalEntryHeight * numDisplayedModels;

	clipper.size.x() = buttonWidth;
	clipper.size.y() = totalHeight;

	scroller.minY = 0;
	scroller.maxY = (numModels - 1 ) * totalEntryHeight;
	scroller.scrollStep = totalEntryHeight;

	for( int i = 0 ; i < numModels ; i++ ) {
		const float score = candidates[ i ].first;
		const int modelIndex = candidates[ i ].second;

		scroller.addEventHandler(
			std::make_shared< ActionModelButton >(
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight ),
				Eigen::Vector2f( buttonWidth, buttonHeight ),
				modelIndex,
				application->world->sceneRenderer,
				[this, modelIndex] () {
					if( sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) ) {
						log( application->world->scene.modelNames[ modelIndex ] );
						application->editor.selectModel( modelIndex );
					}
					else {
						application->world->addInstance( modelIndex, queryObb.transformation.translation() );
					}
			}
		) );
		scroller.addEventHandler(
			std::make_shared< ProgressBarWidget >(
				score,
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight + buttonHeight ),
				Eigen::Vector2f( buttonWidth, barHeight )
			)
		);
	}
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

	const float scale = std::max<float>( 0.025f, screenBox.sizes().head<2>().norm() ) / 4.0; // 4.0 = screenWidth * screenHeight in -1..1x-1..1 coords

	clipper.transformChain.setOffset( offset * 0.5 + Eigen::Vector2f::Constant( 0.5 ) );
	clipper.transformChain.setScale( 2.0 * scale );
}

void aop::CandidateSidebarUI::setModels( std::vector<ScoreModelIndexPair> scoredModelIndices, const Eigen::Vector3f &position ) {
	clear();

	const float buttonWidth = 0.1f;
	const float buttonAbsPadding = 32;
	const float buttonVerticalPadding = buttonAbsPadding / ViewportContext::context->framebufferHeight;
	const float buttonHorizontalPadding = buttonAbsPadding / ViewportContext::context->framebufferWidth;

	// TODO: this was in init but meh
	sidebar.transformChain.setOffset( Eigen::Vector2f( 1 - buttonHorizontalPadding - buttonWidth, 0 ) );

	const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
	const float barHeight = buttonHeight / 8.0f;

	const float totalEntryHeight = buttonHeight + barHeight + buttonVerticalPadding;

	const int maxNumModels = 30;

	const int numModels = std::min<int>( maxNumModels, scoredModelIndices.size() );

	sidebar.minY = 0;
	sidebar.maxY = (numModels - 1 ) * totalEntryHeight;
	sidebar.scrollStep = totalEntryHeight;

	for( int i = 0 ; i < numModels ; i++ ) {
		const float score = scoredModelIndices[ i ].first;
		const int modelIndex = scoredModelIndices[ i ].second;

		sidebar.addEventHandler(
			std::make_shared< ActionModelButton >(
				Eigen::Vector2f( 0.0, buttonVerticalPadding + i * totalEntryHeight ),
				Eigen::Vector2f( buttonWidth, buttonHeight ),
				modelIndex,
				application->world->sceneRenderer,
				[this, modelIndex, position] () {
					if( sf::Keyboard::isKeyPressed( sf::Keyboard::LAlt ) ) {
						log( application->world->scene.modelNames[ modelIndex ] );
						application->editor.selectModel( modelIndex );
					}
					else {
						application->world->addInstance( modelIndex, position );
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
