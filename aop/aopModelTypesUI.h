#pragma once

#include "aopApplication.h"

#include "antTWBarUI.h"
#include "widgets.h"
#include "modelButtonWidget.h"

namespace aop {
	struct ModelTypesUI {
		Application *application;

		std::shared_ptr< AntTWBarUI::SimpleContainer > modelsUi;
		AntTWBarUI::SimpleContainer markedModelsUi;

		std::vector< std::string > beautifiedModelNames;
		std::vector< int > markedModels;
		
		struct ModelNameView : AntTWBarUI::SimpleStructureFactory< std::string, ModelNameView >{
			ModelTypesUI *modelTypesUI;

			ModelNameView( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add( AntTWBarUI::makeSharedButton( accessor.pull(),
						[this, &accessor] () {
							modelTypesUI->toggleMarkedModel( accessor.elementIndex );
						}
					)
				);
			}
		};

		struct MarkedModelNameView : AntTWBarUI::SimpleStructureFactory< int, MarkedModelNameView > {
			ModelTypesUI *modelTypesUI;

			MarkedModelNameView( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

			template< typename ElementAccessor >
			void setup( AntTWBarUI::Container *container, ElementAccessor &accessor ) const {
				container->add( AntTWBarUI::makeSharedVariableWithConfig<AntTWBarUI::VariableConfigs::ReadOnly>(
						"Name",
						AntTWBarUI::makeExpressionAccessor<std::string>( [&] () -> std::string & { return modelTypesUI->beautifiedModelNames[ accessor.pull() ]; } )
					)
				);
			}
		};

		void beautifyModelNames() {
			for( auto filePath = application->world->scene.modelNames.begin() ; filePath != application->world->scene.modelNames.end() ; ++filePath ) {
				std::string filename = *filePath;
				auto offset = filename.find_last_of( "/\\" );
				if( offset != std::string::npos ) {
					filename = filename.substr( offset + 1 );
				}
				beautifiedModelNames.push_back( filename );
			}
		}

		void toggleMarkedModel( int index ) {
			auto found = boost::find( markedModels, index );
			if( found == markedModels.end() ) {
				markedModels.push_back( index );
			}
			else {
				markedModels.erase( found );
			}
			boost::sort( markedModels );
		}

		void replaceMarkedModels( const std::vector<int> modelIndices ) {
			markedModels = modelIndices;
			validateMarkedModels();
		}

		void validateMarkedModels() {
			boost::erase( markedModels, boost::unique< boost::return_found_end>( boost::sort( markedModels ) ) );
		}

		void appendMarkedModels( const std::vector<int> modelIndices ) {
			boost::push_back( markedModels, modelIndices );
			validateMarkedModels();
		}

		void removeAlreadySampledModels() {
			boost::remove_erase_if( markedModels, [&] ( int modelIndex ) {
					return !application->probeDatabase.isEmpty( modelIndex );
				}
			);
		}

		void addAlreadySampledModels() {
			std::vector< int > modelIndices;

			const int numDatasets = application->probeDatabase.getNumSampledModels();
			for( int localModelIndex = 0 ; localModelIndex < numDatasets ; localModelIndex++ ) {
				const int sceneModelIndex = application->probeDatabase.getSceneModelIndex( localModelIndex );
				if( !application->probeDatabase.getSampledModels()[ localModelIndex ].isEmpty() ) {
					modelIndices.push_back( sceneModelIndex );
				}
			}

			appendMarkedModels( modelIndices );
		}

		void addAllModels() {
			markedModels.clear();
			const int numModels = application->modelDatabase.informationById.size();
			for( int modelindex = 0 ; modelindex < numModels ; modelindex++ ) {
				markedModels.push_back( modelindex );
			}
		}

		ModelTypesUI( Application *application ) : application( application ) {
			init();
		}

		struct ReplaceWithSelectionVisitor : Editor::SelectionVisitor {
			ModelTypesUI *modelTypesUI;

			ReplaceWithSelectionVisitor( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

			void visit( Editor::SGSMultiModelSelection *selection ) {
				modelTypesUI->replaceMarkedModels( selection->modelIndices );
			}
		};

		struct AppendSelectionVisitor : Editor::SelectionVisitor {
			ModelTypesUI *modelTypesUI;

			AppendSelectionVisitor( ModelTypesUI *modelTypesUI ) : modelTypesUI( modelTypesUI ) {}

			void visit( Editor::SGSMultiModelSelection *selection ) {
				modelTypesUI->appendMarkedModels( selection->modelIndices );
			}
		};

		void init() {
			beautifyModelNames();

			{
				struct MyConfig {
					enum { supportRemove = false };
				};
				modelsUi = std::make_shared< AntTWBarUI::Vector< ModelNameView, MyConfig > >( "All models", beautifiedModelNames, ModelNameView( this ), AntTWBarUI::CT_EMBEDDED );
				modelsUi->link();
			}

			{
				markedModelsUi.setName( "Marked models");
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "= {}", [this] () {
					markedModels.clear();
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "= all models", [this] () {
					addAllModels();
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "= selection", [this] () {
					ReplaceWithSelectionVisitor( this ).dispatch( application->editor.selection );
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "+= selection", [this] () {
					AppendSelectionVisitor( this ).dispatch( application->editor.selection );
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "selection =", [this] () {
					application->editor.selectModels( markedModels );
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "-= already probe-sampled", [this] () {
					removeAlreadySampledModels();
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedButton( "+= already probe-sampled", [this] () {
					addAlreadySampledModels();
				} ) );
				markedModelsUi.add( AntTWBarUI::makeSharedSeparator() );

				auto markedModelsVector = AntTWBarUI::makeSharedVector( "Models", markedModels, MarkedModelNameView( this ), AntTWBarUI::CT_EMBEDDED );
				markedModelsUi.add( markedModelsVector );
			
				markedModelsUi.link();
			}
		}

		void update() {
			modelsUi->refresh();
			markedModelsUi.refresh();
		}
	};
}