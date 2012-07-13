#pragma once

#include <string>

#include <AntTweakBar.h>

// TODO: rename to twMemberCallback...
template< class Caller, void (Caller::*callback)() >
void TW_CALL memberCallback( Caller &caller ) {
	(caller.*callback)();
}

template< class Caller, typename T, void (Caller::*callback)( T p ) >
void TW_CALL memberCallback( T p, Caller &caller ) {
	(caller.*callback)(p);
}

class AntTWBarGroup
{
public:
	template< typename T >
	struct TypeMapping {
		enum {
			Type = TW_TYPE_UNDEF
		};
	};

	template<>
	struct TypeMapping< bool > {
		enum {
			Type = TW_TYPE_BOOLCPP
		};
	};

	template<>
	struct TypeMapping< char * > {
		enum {
			Type = TW_TYPE_CDSTRING
		};
	};

	template<>
	struct TypeMapping< float > {
		enum {
			Type = TW_TYPE_FLOAT
		};
	};

	template<>
	struct TypeMapping< double > {
		enum {
			Type = TW_TYPE_DOUBLE
		};
	};

	template<>
	struct TypeMapping< int > {
		enum {
			Type = TW_TYPE_INT32
		};
	};

	typedef float Color4F[4];

	template<>
	struct TypeMapping< Color4F > {
		enum {
			Type = TW_TYPE_COLOR4F
		};
	};

	typedef float Direction3F[3];

	template<>
	struct TypeMapping< Direction3F > {
		enum {
			Type = TW_TYPE_DIR3F
		};
	};

	struct Vector3f {
		float x,y,z;
	};



protected:
	AntTWBarGroup * getParent();
	TwBar * getBar();

	const std::string getBarIdentifier();
	const std::string getQualifiedIdentifier();
	const std::string getQualifiedIdentifier( const std::string &itemName );

	const std::string getName();
	const std::string getGroupDefineText();
	const std::string getNameDefineText( const std::string &name );
	const std::string getIdentifier( const std::string &itemName );

	// identifier to be used for the AntTweakBar group
	const std::string getIdentifier();

public:
	template< class T >
	void addButton(const std::string &name, void (TW_CALL *callback )( T& ), T *clientData, const std::string &def = "", const std::string &internalName = "") {
		_addButton(name, (TwButtonCallback) callback, clientData, def, internalName );
	}

	void addSeparator();

	template< class T, typename V >
	void addVarCB(const std::string &name, void (TW_CALL *setCallback)( const V & , T & ), void (TW_CALL *getCallback)( V&, T & ), T *clientData, const std::string &def = "", const std::string &internalName = "") {
		_addVarCB( name, (TwType) TypeMapping< V >::Type, (TwSetVarCallback) setCallback, (TwGetVarCallback) getCallback, clientData, def, internalName );
	}

	template< typename V >
	void addVarRW(const std::string &name, V &var, const std::string &def = "", const std::string &internalName = "") {
		_addVarRW( name, (TwType) TypeMapping< V >::Type, &var, def, internalName );
	}

	template< typename V >
	void addVarRO(const std::string &name, V &var, const std::string &def = "", const std::string &internalName = "") {
		_addVarRO( name, (TwType) TypeMapping< V >::Type, &var, def, internalName );
	}

	void changeItem( const std::string &internalName, const std::string &def );
	void changeItemLabel( const std::string &internalName, const std::string &newName );
	// returns true if the change was successful (if this group is only the child of another one, it will fail)
	bool change( const std::string &def );

	void removeItem( const std::string &name );

	void clear();

private:
	static int s_groupCounter;
	int m_groupIndex;

	AntTWBarGroup * const m_parent;

	std::string m_name;
	TwBar * const m_bar;	
	bool m_initialized;
	bool m_visible;
	bool m_expanded;

	void _addButton(const std::string &name, TwButtonCallback callback, void *clientData, const std::string &def, const std::string &internalName);
	void _addVarCB(const std::string &name, TwType type, TwSetVarCallback setCallback, TwGetVarCallback getCallback, void *clientData, const std::string &def, const std::string &internalName);
	void _addVarRW(const std::string &name, TwType type, void *var, const std::string &def, const std::string &internalName);
	void _addVarRO(const std::string &name, TwType type, void *var, const std::string &def, const std::string &internalName);

	void checkInit();

	void updateDefine();

	TwBar *createBar();

public:
	AntTWBarGroup( const std::string &name );
	AntTWBarGroup( const std::string &name, AntTWBarGroup &parent );
	AntTWBarGroup( const std::string &name, AntTWBarGroup *parent );

	virtual ~AntTWBarGroup();

	void setName( const std::string &name );
	void setVisible( bool visible );
	void setExpand( bool expand );

public:
	static std::string format( const char *format, ... );
};