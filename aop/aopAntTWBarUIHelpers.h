#pragma once

#include <antTWBarUI.h>

#include <Eigen//Eigen>

struct EigenVector3fUIFactory : AntTWBarUI::SimpleStructureFactory< Eigen::Vector3f, EigenVector3fUIFactory > {
	template< typename Accessor >
	void setup( AntTWBarUI::Container *container, Accessor &accessor ) const {
		container->setExpanded( false );
		container->add(
			AntTWBarUI::makeSharedVariable(
				"x",
				AntTWBarUI::makeLinkedExpressionAccessor<float>( 
					[&] () -> float & {
						return accessor.pull().x();
					},
					accessor
				)
			)
		);
		container->add(
			AntTWBarUI::makeSharedVariable(
				"y",
				AntTWBarUI::makeLinkedExpressionAccessor<float>(
					[&] () -> float & {
						return accessor.pull().y();
					},
					accessor
				)
			)
		);
		container->add(
			AntTWBarUI::makeSharedVariable(
				"z",
				AntTWBarUI::makeLinkedExpressionAccessor<float>(
					[&] () -> float & {
						return accessor.pull().z();
					},
					accessor
				)
			)
		);
	}
};

struct EigenRotationMatrix : AntTWBarUI::SimpleStructureFactory< Eigen::Matrix3f, EigenRotationMatrix > {
	template< typename Accessor >
	void setup( AntTWBarUI::Container *container, Accessor &accessor ) const {
		container->add(
			AntTWBarUI::makeSharedVariable(
				"Orientation",
				AntTWBarUI::CallbackAccessor< AntTWBarUI::Types::Quat4f >(
					[&] ( AntTWBarUI::Types::Quat4f &shadow ) {
						const Eigen::Quaternionf quat( accessor.pull() );
						for( int i = 0 ; i < 4 ; i++ ) {
							shadow.coeffs[i] = quat.coeffs()[i];
						}
					},
					[&] ( const AntTWBarUI::Types::Quat4f &shadow ) {
						Eigen::Quaternionf quat;
						for( int i = 0 ; i < 4 ; i++ ) {
							quat.coeffs()[i] = shadow.coeffs[i];
						}
						accessor.pull() = quat.toRotationMatrix();
						accessor.push();
					}
				)
			)
		);
	}
};