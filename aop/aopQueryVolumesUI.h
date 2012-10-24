#pragma once

#include "aopApplication.h"

#include <antTWBarUI.h>
#include "aopAntTWBarUIHelpers.h"

namespace aop {
	struct TargetVolumesUI {
		Application *application;

		AntTWBarUI::SimpleContainer ui;

		struct NamedTargetVolumeView : AntTWBarUI::SimpleStructureFactory< aop::SceneSettings::NamedTargetVolume, NamedTargetVolumeView > {
			TargetVolumesUI *targetVolumesUI;

			NamedTargetVolumeView( TargetVolumesUI *targetVolumesUI ) : targetVolumesUI( targetVolumesUI ) {}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add(
					AntTWBarUI::makeSharedVariableWithConfig< AntTWBarUI::VariableConfigs::SetContainerName >( 
						"Name",
						AntTWBarUI::makeMemberAccessor( accessor, &aop::SceneSettings::NamedTargetVolume::name )
					)
				);
				container->add(
					EigenVector3fUIFactory().makeShared( 
						AntTWBarUI::CallbackAccessor<Eigen::Vector3f>(
							[&] ( Eigen::Vector3f &shadow ) {
								shadow = accessor.pull().volume.transformation.translation();
							},
							[&] ( const Eigen::Vector3f &shadow ) {
								accessor.pull().volume.transformation.translation() = shadow;
							}
						),
						AntTWBarUI::CT_GROUP,
						"Position"
					)
				);
				container->add(
					EigenRotationMatrix().makeShared( 
						AntTWBarUI::CallbackAccessor<Eigen::Matrix3f>(
							[&] ( Eigen::Matrix3f &shadow ) {
								shadow = accessor.pull().volume.transformation.linear();
							},
							[&] ( const Eigen::Matrix3f &shadow ) {
								accessor.pull().volume.transformation.linear() = shadow;
							}
						),
						AntTWBarUI::CT_EMBEDDED
					)
				);
				container->add(
					AntTWBarUI::makeSharedButton(
						"Select",
						[&] () {
							targetVolumesUI->application->editor.selectObb( accessor.elementIndex );
						}
					)
				);
			}
		};

		TargetVolumesUI( Application *application ) : application( application ) {
			init();
		}

		void init() {
			ui.setName( "Query volumes" );
			auto uiVector = AntTWBarUI::makeSharedVector( "Volumes", application->sceneSettings.volumes, NamedTargetVolumeView( this ) );

			ui.add( uiVector );
			ui.add( AntTWBarUI::makeSharedButton( "Add new",
					[this, uiVector] () {
						const auto &camera = this->application->mainCamera;
						aop::SceneSettings::NamedTargetVolume volume;

						volume.volume.size = Eigen::Vector3f::Constant( 5.0 );
						volume.volume.transformation =
							Eigen::Translation3f( camera.getPosition() + 5.0 * camera.getDirection() ) *
							camera.getViewRotation().transpose()
						;

						this->application->sceneSettings.volumes.push_back( volume );
					}
				)
			);
			ui.link();
		}

		void update() {
			ui.refresh();
		}

	};
}