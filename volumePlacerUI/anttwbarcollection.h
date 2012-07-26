#pragma once
#include <vector>
#include <functional>
#include <utility>

#include "anttwbargroup.h"

template< typename Value, int LABEL_MAX_LENGTH = 64 >
struct AntTWBarCollection {
	std::vector<Value> collection;

	struct Label {
		char value[LABEL_MAX_LENGTH];

		Label() {
			value[0] = 0;
		}
	};
	std::vector<Label> collectionLabels;

	std::unique_ptr<AntTWBarGroup> ui;
	std::vector<AntTWBarGroup::ButtonCallback> buttonCallbacks;

	std::function<void (const Value &)> onAction;
	std::function<std::string (const Value &, int)> getSummary;
	std::function<Value()> getNewItem;

	AntTWBarCollection() {
		getSummary = [] (const Value &, int i) { return AntTWBarGroup::format( "%i", i ); };
	}

	void init( const char *title, AntTWBarGroup *parent ) {
		ui = std::unique_ptr<AntTWBarGroup>( new AntTWBarGroup( title, parent ) );

		refresh();
	}

	void refresh() {
		ui->clear();

		collectionLabels.resize( collection.size() );

		buttonCallbacks.clear();
		buttonCallbacks.resize( collection.size() * 2 + 1 );

		if( getNewItem ) {
			buttonCallbacks.back().callback = [=] () {
				collection.push_back( getNewItem() );
				collectionLabels.push_back( AntTWBarCollection<Value>::Label() );
				refresh();
			};
			ui->addButton( "Add", buttonCallbacks.back() );
		}

		for( int i = 0 ; i < collection.size() ; ++i ) {
			buttonCallbacks[2*i].callback = [=] () {
				if( onAction ) {
					onAction( collection[i] ); 
				}
			};
			buttonCallbacks[2*i+1].callback = [=] () {
				collection.erase( collection.begin() + i );
				collectionLabels.erase( collectionLabels.begin() + i );
				refresh();
			};

			ui->addSeparator();
			ui->addButton( getSummary( collection[i], i ), buttonCallbacks[2*i] );
			ui->_addVarRW( "Label", TW_TYPE_CSSTRING(LABEL_MAX_LENGTH), &collectionLabels[i] );
			ui->addButton( "Remove", buttonCallbacks[2*i+1] );
		}		
	}
};