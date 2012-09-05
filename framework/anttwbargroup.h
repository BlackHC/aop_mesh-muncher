#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <functional>

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

namespace AntTWBarGroupTypes {
	template< typename T >
	struct TypeMapping {
		enum {
			Type = TW_TYPE_UNDEF
		};
	};

	template< typename T >
	struct TypeMapper {
		static const int &Type;
	};

	template< typename T >
	const int &TypeMapper<T>::Type = TypeMapping< std::remove_cv< T >::type >::Type;

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
	struct TypeMapping< __int32 > {
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

	template< int twTypeValue >
	struct TwTypeMapping {
		typedef void Type;
	};

	template<>
	struct TwTypeMapping< TW_TYPE_BOOLCPP > {
		typedef bool Type;
	};

	template<>
	struct TwTypeMapping< TW_TYPE_CDSTRING > {
		typedef char * Type;
	};

	template<>
	struct TwTypeMapping< TW_TYPE_FLOAT > {
		typedef float Type;
	};

	template<>
	struct TwTypeMapping< TW_TYPE_INT32 > {
		typedef __int32 Type;
	};

	struct Vector3f {
		float x,y,z;
	};

	struct Vector3i {
		int x,y,z;
	};

	template<typename S>
	struct StdSummarizer {
		static void summarize( char *summary, size_t maxLength, const S* object) {
			std::ostringstream out;
			out << *object;
			out.str().copy( summary, maxLength );
		}
	};

	template<typename S>
	struct NullSummarizer {
		static void summarize( char *summary, size_t maxLength, const S* object) {
			*summary = 0;
		}
	};

	template<typename S, typename R = S, typename Summarizer=NullSummarizer<S> >
	struct Struct {
		const char *name;
		std::vector<TwStructMember> members;

		Struct( const char *name ) : name( name ) {}

		template<typename T>
		Struct & add( const char *name, T R::*pointer, const char *defString = nullptr ) {
			TwStructMember member;
			
			member.Name = name;
			member.Type = (TwType) TypeMapping<T>::Type;
			member.Offset = reinterpret_cast<size_t>(&(static_cast<R*>(nullptr)->*pointer));
			member.DefString = defString;
			members.push_back( member );

			return *this;
		}

		TwType define() {
			return TwDefineStruct( name, &members.front(), (unsigned int) members.size(), sizeof( S ), &SummaryCallback, /*(void*) this*/ nullptr);
		}

		static void TW_CALL SummaryCallback(char *summaryString, size_t summaryMaxLength, const void *value, void *clientData) {
			/*Struct *_ = static_cast<Struct*>(clientData);
			S *s = static_cast<const S*>(value);

			std::ostringstream out;
			out << "{ ";
			for( auto it = members.cbegin() ; it != members.cend() ; ++it ) {
				out << it->Name << "='";
				
				switch( it->Type ) {
#define TYPE_CASE( t ) \
				case t:\
						out << *reinterpret_cast< const TwTypeMapping<t>::Type *>( reinterpret_cast<const char*>(value) + it->Offset ) << " ";

				TYPE_CASE( TW_TYPE_INT32 );
#undef TYPE_CASE
				}
			}
			out << "}";*/
			Summarizer::summarize( summaryString, summaryMaxLength, static_cast<const S*>(value) );
		}
	};

	template<typename E>
	struct Enum {
		const char *name;
		std::vector<TwEnumVal> values;

		Enum( const char *name ) : name( name ) {}

		Enum & add( const char *label, E value ) {
			TwEnumVal def;
			def.Value = value;
			def.Label = label;
			values.push_back( def );

			return *this;
		}

		Enum & add( const char *label ) {
			TwEnumVal def;
			def.Label = label;
			if( !values.empty() ) {
				def.Value = values.back().Value + 1;
			}
			else {
				def.Value = 0;
			}
			values.push_back( def );

			return *this;
		}

		TwType define() {
			return TwDefineEnum( name, &values.front(), (int) values.size() );
		}
	};
}

class AntTWBarGroup
{
protected:
	AntTWBarGroup * getParent();
	TwBar * getBar();

	const std::string getBarIdentifier();
	const std::string getQualifiedIdentifier();
	const std::string getQualifiedChildIdentifier( const std::string &itemName );

	const std::string getName();
	const std::string getGroupDefineText();
	const std::string getNameDefineText( const std::string &name );
	const std::string getChildIdentifier( const std::string &itemName );

	// identifier to be used for the AntTweakBar group
	const std::string getIdentifier();

public:
	struct ButtonCallback {
		std::function<void()> callback;

		//ButtonCallback( const std::function<void> &callback ) : callback( callback ) {}

		static void TW_CALL Execute(ButtonCallback &me) {
			me.callback();
		}
	};

	template<typename V>
	struct VariableCallback {
		std::function<void (V&)> getCallback;
		std::function<void (const V&)> setCallback;

		static void TW_CALL ExecuteGet(V &variable, VariableCallback &me) {
			me.getCallback(variable);
		}

		static void TW_CALL ExecuteSet(const V &variable, VariableCallback &me) {
			me.setCallback(variable);
		}
	};

	template< class T >
	void addButton(const std::string &name, void (TW_CALL *callback )( T& ), T *clientData, const std::string &def = "", const std::string &internalName = "") {
		_addButton(name, (TwButtonCallback) callback, clientData, def, internalName );
	}

	void addButton(const std::string &name, ButtonCallback &callback, const std::string &def = "", const std::string &internalName = "") {
		_addButton(name, (TwButtonCallback) &ButtonCallback::Execute, &callback, def, internalName );
	}

	void addSeparator();

	template< class T, typename V >
	void addVarCB(const std::string &name, void (TW_CALL *setCallback)( const V & , T & ), void (TW_CALL *getCallback)( V&, T & ), T *clientData, const std::string &def = "", const std::string &internalName = "") {
		_addVarCB( name, (TwType) AntTWBarGroupTypes::TypeMapper< V >::Type, (TwSetVarCallback) setCallback, (TwGetVarCallback) getCallback, clientData, def, internalName );
	}

	template< typename V >
	void addVarCB(const std::string &name, VariableCallback< V > &callbacks, const std::string &def = "", const std::string &internalName = "") {
		_addVarCB( name, (TwType) AntTWBarGroupTypes::TypeMapper< V >::Type, (TwSetVarCallback) &VariableCallback<V>::ExecuteSet, (TwGetVarCallback) &VariableCallback<V>::ExecuteGet, &callbacks, def, internalName );
	}

	template< typename V >
	void addVarCB(const std::string &name, int type, VariableCallback< V > &callbacks, const std::string &def = "", const std::string &internalName = "") {
		_addVarCB( name, (TwType) type, (TwSetVarCallback) &VariableCallback<V>::ExecuteSet, (TwGetVarCallback) &VariableCallback<V>::ExecuteGet, &callbacks, def, internalName );
	}

	template< typename V >
	void addVarRW(const std::string &name, V &var, const std::string &def = "", const std::string &internalName = "") {
		_addVarRW( name, (TwType) AntTWBarGroupTypes::TypeMapper< V >::Type, &var, def, internalName );
	}

	template< typename V >
	void addVarRO(const std::string &name, V &var, const std::string &def = "", const std::string &internalName = "") {
		_addVarRO( name, (TwType) AntTWBarGroupTypes::TypeMapper< V >::Type, (void*) &var, def, internalName );
	}

	void changeItem( const std::string &internalName, const std::string &def );
	void changeItemLabel( const std::string &internalName, const std::string &newName );
	// returns true if the change was successful (if this group is only the child of another one, it will fail)
	bool change( const std::string &def );

	void removeItem( const std::string &name );

	void clear();

	void _addButton(const std::string &name, TwButtonCallback callback, void *clientData, const std::string &def, const std::string &internalName);
	void _addVarCB(const std::string &name, TwType type, TwSetVarCallback setCallback, TwGetVarCallback getCallback, void *clientData, const std::string &def, const std::string &internalName);
	void _addVarRW(const std::string &name, TwType type, void *var, const std::string &def = "", const std::string &internalName = "");
	void _addVarRO(const std::string &name, TwType type, void *var, const std::string &def = "", const std::string &internalName = "");

private:
	static int s_groupCounter;
	int m_groupIndex;

	AntTWBarGroup * const m_parent;

	std::string m_name;
	TwBar * const m_bar;	
	bool m_initialized;
	bool m_visible;
	bool m_expanded;

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