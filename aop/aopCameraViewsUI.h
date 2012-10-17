#pragma once

#include "aopApplication.h"

#include "antTWBarUI.h"

namespace aop {
		struct CameraViewsUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

		struct NamedCameraStateView : AntTWBarUI::SimpleStructureFactory< aop::SceneSettings::NamedCameraState, NamedCameraStateView >{
			Application *application;

			NamedCameraStateView( Application *application ) : application( application ) {}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add(
					AntTWBarUI::makeSharedVariable(
						"Name",
						AntTWBarUI::makeMemberAccessor( accessor, &aop::SceneSettings::NamedCameraState::name )
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Set default",
						[&] () {
							auto &views = application->sceneSettings.views;
							std::swap( views.begin(), views.begin() + accessor.elementIndex );
						}
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Use",
						[&] () {
							accessor.pull().pushTo( this->application->mainCamera );
						}
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Replace",
						[&] () {
							accessor.pull().pullFrom( this->application->mainCamera );
							accessor.push();
						}
					)
				);
			}
		};

		CameraViewsUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			ui.setName( "Camera views" );

			ui.add( AntTWBarUI::makeSharedButton(
					"Add current view",
					[this] () {
						application->sceneSettings.views.push_back( aop::SceneSettings::NamedCameraState() );
						application->sceneSettings.views.back().pullFrom( application->mainCamera );
					}
				)
			);
			ui.add( AntTWBarUI::makeSharedButton(
					"Clear all",
					[this] () {
						application->sceneSettings.views.clear();
					}
				)
			);

			auto cameraStatesView = AntTWBarUI::makeSharedVector( application->sceneSettings.views, NamedCameraStateView( application ) );
			ui.add( cameraStatesView );

			ui.link();
		}

		void update() {
			ui.refresh();
		}
	};
}