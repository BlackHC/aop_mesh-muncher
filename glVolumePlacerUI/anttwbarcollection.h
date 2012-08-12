#pragma once
#include <vector>
#include <functional>
#include <utility>

#include "anttwbargroup.h"

struct AntTWBarLabel {
	static const int LABEL_MAX_LENGTH = 64;
	char value[LABEL_MAX_LENGTH];

	AntTWBarLabel() {
		value[0] = 0;
	}
};

template< typename Value >
struct AntTWBarCollection {
	std::vector<Value> collection;
	
	std::vector<AntTWBarLabel> collectionLabels;

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
				collectionLabels.push_back( AntTWBarLabel() );
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
			ui->_addVarRW( "Label", TW_TYPE_CSSTRING(AntTWBarLabel::LABEL_MAX_LENGTH), &collectionLabels[i] );
			ui->addButton( "Remove", buttonCallbacks[2*i+1] );
		}		
	}
};