#include <stdarg.h>
#include <assert.h>

#include "anttwbargroup.h"

int AntTWBarGroup::s_groupCounter = 0;

std::string AntTWBarGroup::format( const char *format, ... ) {
	static char buffer[ 1 << 12 ];
	va_list args;
	va_start( args, format );
	_vsnprintf( buffer, sizeof( buffer ), format, args );
	va_end( args );

	return buffer;
}

const std::string AntTWBarGroup::getIdentifier( const std::string &itemName ) {
	if( itemName != "" ) {
		return format( "%s--%s", getIdentifier().c_str(), itemName.c_str() );
	}
	return "";
}

const std::string AntTWBarGroup::getIdentifier() {
	return format( "%i", m_groupIndex );
}
void AntTWBarGroup::setVisible( bool visible ) {
	m_visible = visible;

	if( !m_initialized ) {
		return;
	}

	TwDefine( format( " %s %s ", getQualifiedIdentifier().c_str(), visible ? "show" : "hide" ).c_str() );
}

void AntTWBarGroup::setExpand( bool expand ) {
	m_expanded = expand;

	if( !m_initialized ) {
		return;
	}

	if( m_parent ) {
		TwDefine( format( " %s %s ", getQualifiedIdentifier().c_str(), expand ? "open" : "close" ).c_str() );
	}
	else if( m_visible ) {
		TwDefine( format( " %s %s ", getQualifiedIdentifier().c_str(), expand ? "show" : "iconify" ).c_str() );
	}
}

AntTWBarGroup::AntTWBarGroup( const std::string &name ) : m_name( name ), m_parent( NULL ), m_bar( createBar() ), m_initialized( false ), m_visible( true ), m_expanded( true ), m_groupIndex( s_groupCounter++ ) {
	checkInit();
}

AntTWBarGroup::AntTWBarGroup( const std::string &name, AntTWBarGroup &parent ) : m_name( name ), m_parent( &parent ), m_bar( parent.m_bar ), m_initialized( false ), m_visible( true ), m_expanded( true ), m_groupIndex( s_groupCounter++ ) {
	// don't call checkInit() because m_parent is set
}

AntTWBarGroup::AntTWBarGroup( const std::string &name, AntTWBarGroup *parent ) : m_name( name ), m_parent( parent ), m_bar( parent ? parent->m_bar : createBar() ), m_initialized( false ), m_visible( true ), m_expanded( true ), m_groupIndex( s_groupCounter++ ) {
	if( !m_parent )
		checkInit();
}

AntTWBarGroup::~AntTWBarGroup() {
	if( m_parent ) {
		TwRemoveVar( m_bar, getIdentifier().c_str() );
	}
	else {
		TwDeleteBar( m_bar );
	}
}

void AntTWBarGroup::updateDefine() {
	std::string groupDefineTex = "";
	if( m_parent ) {
		groupDefineTex = m_parent->getGroupDefineText();
	}

	TwDefine( format( " %s label='%s' %s ", getQualifiedIdentifier().c_str(), m_name.c_str(), groupDefineTex.c_str() ).c_str() );

	setVisible( m_visible );
	setExpand( m_expanded );
}

void AntTWBarGroup::setName( const std::string &name ) {
	m_name = name;
	if( m_initialized ) {
		updateDefine();
	}
}

void AntTWBarGroup::addSeparator() {
	TwAddSeparator( m_bar, "", getGroupDefineText().c_str() );
	checkInit();
}

TwBar * AntTWBarGroup::createBar() {
	return TwNewBar( getIdentifier().c_str() );	
}

void AntTWBarGroup::clear() {
	if( m_parent ) {
		if( m_initialized ) {
			TwRemoveVar( m_bar, getIdentifier().c_str() );
			// re-set the group label ASAP
			m_initialized = false;
		}
	}
	else {
		TwRemoveAllVars( m_bar );
	}
}

void AntTWBarGroup::removeItem( const std::string &name ) {
	assert( name != "" );
	if( name != "" ) {
		TwRemoveVar( m_bar, getIdentifier( name ).c_str() );
	}
}

void AntTWBarGroup::changeItem( const std::string &name, const std::string &def ) {
	assert( name != "" );
	if( name != "" ) {
		TwDefine( format( " %s %s ", getQualifiedIdentifier( name ).c_str(), def.c_str() ).c_str() );
	}
}

void AntTWBarGroup::changeItemLabel( const std::string &internalName, const std::string &newName ) {
	changeItem(internalName, format( "label='%s'", newName.c_str() ) );
}

void AntTWBarGroup::_addVarCB( const std::string &name, TwType type, TwSetVarCallback setCallback, TwGetVarCallback getCallback, void *clientData, const std::string &def, const std::string &internalName ) {
	TwAddVarCB( m_bar, getIdentifier( internalName ).c_str(), type, setCallback, getCallback, clientData, (def + getGroupDefineText() + getNameDefineText( name )).c_str() );
	checkInit();
}

void AntTWBarGroup::_addButton( const std::string &name, TwButtonCallback callback, void *clientData, const std::string &def, const std::string &internalName ) {
	TwAddButton( m_bar, getIdentifier( internalName ).c_str(), (TwButtonCallback) callback, clientData, (def + getGroupDefineText() + getNameDefineText( name )).c_str() );
	checkInit();
}

void AntTWBarGroup::_addVarRW( const std::string &name, TwType type, void *var, const std::string &def, const std::string &internalName ) {
	TwAddVarRW( m_bar, getIdentifier( internalName ).c_str(), type, var, (def + getGroupDefineText() + getNameDefineText( name )).c_str() );
	checkInit();
}

void AntTWBarGroup::_addVarRO( const std::string &name, TwType type, void *var, const std::string &def, const std::string &internalName ) {
	TwAddVarRO( m_bar, getIdentifier( internalName ).c_str(), type, var, (def + getGroupDefineText() + getNameDefineText( name )).c_str() );
	checkInit();
}

AntTWBarGroup * AntTWBarGroup::getParent() {
	return m_parent;
}

TwBar * AntTWBarGroup::getBar() {
	return m_bar;
}

const std::string AntTWBarGroup::getBarIdentifier() {
	return TwGetBarName(m_bar);
}

const std::string AntTWBarGroup::getQualifiedIdentifier() {
	if( m_parent ) {
		return m_parent->getBarIdentifier() + "/" + getIdentifier();
	}
	return getIdentifier();
}

const std::string AntTWBarGroup::getQualifiedIdentifier( const std::string &itemName ) {
	if( itemName != "" ) {
		return m_parent->getBarIdentifier() + "/" + getIdentifier( itemName );
	}
	return "";
}

const std::string AntTWBarGroup::getName() {
	return m_name;
}

const std::string AntTWBarGroup::getGroupDefineText() {
	if( m_parent ) {
		return format( " group='%s' ", getIdentifier().c_str() );
	}
	return "";
}

const std::string AntTWBarGroup::getNameDefineText( const std::string &name ) {
	return format( " label='%s' ", name.c_str() );
}

void AntTWBarGroup::checkInit() {
	if( !m_initialized ) {
		m_initialized = true;
		updateDefine();

		if( m_parent ) {
			m_parent->checkInit();
		}
	}
}

bool AntTWBarGroup::change( const std::string &def ) {
	if( m_parent ) {
		return false;
	}

	TwDefine( format( " %s %s ", getQualifiedIdentifier().c_str(), def.c_str() ).c_str() );
	return true;
}