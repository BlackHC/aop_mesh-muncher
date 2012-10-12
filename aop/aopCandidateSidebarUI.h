#pragma once

#include "aopApplication.h"

#include <memory>
#include "viewportContext.h"

namespace aop {
	struct CandidateSidebarUI {
		Application *application;

		struct CandidateContainer : WidgetContainer {
			float minY, maxY;
			float scrollStep;

			CandidateContainer() : minY(), maxY(), scrollStep() {}

			void onMouse( EventState &eventState ) {
				if( eventState.event.type == sf::Event::MouseWheelMoved ) {
					//log( boost::format( "mouse wheel moved %i" ) % eventState.event.mouseWheel.delta );
					
					auto offset = transformChain.getOffset();
					offset.y() = std::min( -minY, std::max( -maxY, offset.y() + eventState.event.mouseWheel.delta * scrollStep ) );
					transformChain.setOffset( offset );

					eventState.accept();
				}
				else {
					WidgetContainer::onMouse( eventState );
				}
			}
		};
		
		CandidateContainer sidebar;

		struct CandidateModelButton : ModelButtonWidget {
			CandidateModelButton(
				const Eigen::Vector2f &offset,
				const Eigen::Vector2f &size,
				int modelIndex,
				SGSSceneRenderer &renderer,
				const std::function<void()> &action = nullptr
			) :
				ModelButtonWidget( offset, size, modelIndex, renderer ),
				action( action )
			{}

			std::function<void()> action;

		private:
			void onAction() {
				if( action ) {
					action();
				}
			}
		};

		CandidateSidebarUI( Application *application ) : application( application ) {
			init();
		}

		void init() {				
			application->widgetRoot.addEventHandler( make_nonallocated_shared( sidebar ) );
		}

		void clear() {
			for( auto element = sidebar.eventHandlers.begin() ; element != sidebar.eventHandlers.end() ; ++element ) {
				application->eventSystem.onEventHandlerRemove( element->get() );
			}
			sidebar.eventHandlers.clear();
		}

		void addModels( std::vector<int> modelIndices, const Eigen::Vector3f &position ) {
			const float buttonWidth = 0.1;
			const float buttonAbsPadding = 32;
			const float buttonVerticalPadding = buttonAbsPadding / ViewportContext::context->framebufferHeight;
			const float buttonHorizontalPadding = buttonAbsPadding / ViewportContext::context->framebufferWidth;

			// TODO: this was in init but meh
			sidebar.transformChain.setOffset( Eigen::Vector2f( 1 - buttonHorizontalPadding - buttonWidth, 0 ) );

			const float buttonHeight = buttonWidth * ViewportContext::context->getAspectRatio();
			const float buttonHeightWithPadding = buttonHeight + buttonVerticalPadding;

			const int maxNumModels = 30;

			// add 5 at most
			const int numModels = std::min<int>( maxNumModels, modelIndices.size() );

			sidebar.minY = 0;
			sidebar.maxY = (numModels - 1 ) * buttonHeightWithPadding;
			sidebar.scrollStep = buttonHeightWithPadding;
			
			for( int i = 0 ; i < numModels ; i++ ) {
				const int modelIndex = modelIndices[ i ];

				sidebar.addEventHandler(
					std::make_shared< CandidateModelButton >(
						Eigen::Vector2f( 0.0, buttonVerticalPadding + i * buttonHeightWithPadding ),
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
					)
				);
			}
		}
	};

	inline std::shared_ptr<CandidateSidebarUI> createCandidateSidebarUI( Application *application ) {
		return std::make_shared<CandidateSidebarUI>( application );
	}
}