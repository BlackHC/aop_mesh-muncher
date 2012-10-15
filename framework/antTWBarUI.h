#pragma once
#include <vector>
#include <functional>
#include <utility>

#include "anttwbargroup.h"

#include <boost/lexical_cast.hpp>

namespace AntTWBarUI {
	// support two creation modes: simply wrap an object or make it more complex by instantiating the UI element yourself

	enum ContainerType {
		CT_GROUP,
		CT_SEPARATOR,
		CT_EMBEDDED
	};


	// TODO: add move most things into a separate detail namespace [10/1/2012 kirschan2]
	struct UniqueID {
		typedef unsigned ID;
		static ID idCounter;
		const ID id;

		UniqueID() : id( createUniqueID() ) {}

		static ID createUniqueID() {
			return idCounter++;
		}
	};

	struct InternalName : UniqueID {
		const std::string internalName;

		InternalName() : internalName( boost::lexical_cast<std::string>( id ) ) {
		}
	};

	struct InternalBar : InternalName {
		TwBar *twBar;

		InternalBar() : twBar( nullptr ) {}

		bool hasBar() const {
			return twBar != nullptr;
		}

		void create() {
			if( !twBar ) {
				twBar = TwNewBar( internalName.c_str() );
			}
		}

		void destroy() {
			if( twBar ) {
				TwDeleteBar( twBar );
				twBar = nullptr;
			}
		}

		void unlink() {
			TwRemoveAllVars( twBar );
		}

		void setLabel( const std::string &label ) {
			TwSetParam( twBar, NULL, "label", TW_PARAM_CSTRING, 1, label.empty() ? " " : label.c_str() );
		}

		~InternalBar() {
			destroy();
		}

		void define( const std::string &text ) {
			TwDefine( (internalName + " " + text).c_str() );
		}
	};

	// this class wraps the actual anttweakbar api and stores all data to be able to recreate an object
	struct InternalElement : InternalName {
		bool topLevel;
		const InternalElement *group;
		InternalBar *bar;
		// removable as item that can be removed using TwRemoveVar (ie not a group)
		bool removable;
		bool spawned;

		InternalElement() : topLevel( false ), group( nullptr ), bar( nullptr ), removable( false ), spawned( false ) {}
		~InternalElement() {
			// make sure we delete ourselves on destruction
			remove();
		}

		std::string getQualifiedInternalName() const {
			return bar->internalName + "/" + internalName;
		}

		void define( const std::string &text ) {
			TwDefine( (getQualifiedInternalName() + " " + text).c_str() );
		}

		void nest( InternalBar &bar ) {
			group = nullptr;
			this->bar = &bar;
			topLevel = true;
		}

		void nest( const InternalElement &group ) {
			this->group = &group;
			this->bar = group.bar;

			topLevel = false;
		}

		void setGroup() {
			if( group && !group->topLevel ) {
				TwSetParam( bar->twBar, internalName.c_str(), "group", TW_PARAM_CSTRING, 1, group->internalName.c_str() );
			}
			else {
				TwSetParam( bar->twBar, internalName.c_str(), "group", TW_PARAM_CSTRING, 1, std::string().c_str() );
			}
		}

		void setLabel( const std::string &label ) {
			TwSetParam( bar->twBar, internalName.c_str(), "label", TW_PARAM_CSTRING, 1, label.empty() ? " " : label.c_str() );
		}

		void setExpand( bool expand ) {
			int tmp = expand;
			TwSetParam( bar->twBar, internalName.c_str(), "opened", TW_PARAM_INT32, 1, &tmp );
		}

		bool getExpand() {
			int tmp;
			TwGetParam( bar->twBar, internalName.c_str(), "opened", TW_PARAM_INT32, 1, &tmp );
			return tmp != 0;
		}

		void unnest() {
			remove();

			group = nullptr;
			bar = nullptr;
			topLevel = false;
		}

		void remove() {
			if( removable ) {
				// remove ourself whatever we are (except if we are a group)
				TwRemoveVar( bar->twBar, internalName.c_str() );
				removable = false;
			}
			spawned = false;
		}

		void makeSeparator( const std::string &def ) {
			TwAddSeparator( bar->twBar, internalName.c_str(), def.c_str() );
			removable = true;
			spawned = true;

			setGroup();
		}

		void makeGroup() {
			spawned = true;

			setGroup();
		}

		void makeButton( TwButtonCallback callback, void *data, const std::string &def ) {
			TwAddButton( bar->twBar, internalName.c_str(), callback, data, def.c_str() );
			removable = true;
			spawned = true;

			setGroup();
		}

		void makeVariableRW( TwType type, void *var, const std::string &def ) {
			TwAddVarRW( bar->twBar, internalName.c_str(), type, var, def.c_str() );
			removable = true;
			spawned = true;

			setGroup();
		}

		void makeVariableCB( TwType type, TwGetVarCallback getCallback, TwSetVarCallback setCallback, void *clientData, const std::string &def) {
			TwAddVarCB( bar->twBar, internalName.c_str(), type, setCallback, getCallback, clientData, def.c_str() );
			removable = true;
			spawned = true;

			setGroup();
		}
	};

	struct Container;

	struct Element {
		// TODO: replace all uses of std::shared_ptr< Element > with Element::SPtr [10/12/2012 kirschan2]
		typedef std::shared_ptr< Element > SPtr;

		Element() : linked( false ), parent() {}

		bool isLinked() {
			return linked;
		}

		void link() {
			if( linked ) {
				unlink();
			}

			doLink();

			linked = true;
		}

		void unlink() {
			if( !linked ) {
				return;
			}

			doUnlink();

			linked = false;
		}

		Container * getParent() const {
			return parent;
		}

		void refresh() {
			doRefresh();
		}

		virtual ~Element() {
			// we don't need to worry about unregistering with our parent here, because containers hold a shared_ptr to their children
			// so no child will go out of scope while it is a child :)
		}

	private:
		virtual void doLink() = 0;
		virtual void doUnlink() = 0;
		virtual void doRefresh() = 0;

	private:
		// returns false, if the new parent is the old parent
		bool changeParent( Container *newParent );

		friend struct Container;

		bool linked;
		Container *parent;
	};

	struct Expanded {
		void set( bool newExpanded ) {
			expanded = newExpanded;

			link();
		}

		/*bool get() const {
			return expanded;
		}*/

		Expanded( InternalElement &element, const bool expanded ) : element( element ), expanded( expanded ) {}

		void link() {
			if( element.spawned ) {
				element.setExpand( expanded );
			}
		}

		void unlink() {
			if( element.spawned ) {
				expanded = element.getExpand();
			}
		}
	private:
		// this is only backed up from AntTweakBar on unlink, so inbetween the value might be incorrect
		bool expanded;
		InternalElement &element;
	};

	struct Container : Element {
		Container( ContainerType containerType, const std::string &name = std::string() )
			: containerType( containerType ), name( name ), linkedAsBar( false ), expanded( group, true ) {
		}

		Container( const std::string &name = std::string() )
			: containerType( CT_GROUP ), name( name ), linkedAsBar( false ), expanded( group, true ) {
		}

		void add( const std::shared_ptr<Element> &child ) {
			if( child->parent ) {
				child->parent->remove( child.get() );
			}
			if( child->changeParent( this ) ) {
				doAdd( child );
			}
		}

		void remove( Element *child ) {
			auto childPtr = doRemove( child );
			if( childPtr ) {
				childPtr->changeParent( nullptr );
			}
		}

		const InternalElement &getGroup() {
			if( containerType == CT_GROUP || !parent ) {
				return group;
			}
			else /* embed && parent */ {
				return parent->getGroup();
			}
		}

		void setName( const std::string &name ) {
			this->name = name;
		}

		void setExpanded( bool newExpanded ) {
			expanded.set( newExpanded );
		}

	protected:
		virtual void doRefresh() {}

	private:
		virtual void doLinkChildren() = 0;
		virtual void doUnlinkChildren() = 0;

	private:
		virtual void doAdd( const std::shared_ptr< Element > &child ) = 0;
		// returns if the child has been found and removed (ie the important part being if it was in the container)
		virtual std::shared_ptr< Element > doRemove( Element *child ) = 0;

		virtual void doLink() {
			bool addSecondSeperator = false;

			if( getParent() ) {
				// we have a parent, so we are just a group
				bar.destroy();
				linkedAsBar = false;

				if( containerType == CT_GROUP ) {
					group.nest( getParent()->getGroup() );
					seperatorElement.nest( group );
					seperatorElement.makeSeparator( "visible=false" );

					group.makeGroup();
					group.setLabel( name );
				}
				else {
					seperatorElement.nest( getParent()->getGroup() );

					if( containerType == CT_EMBEDDED ) {
						seperatorElement.makeSeparator( "visible=false" );
					}
					else {
						seperatorElement.makeSeparator( std::string() );
						addSecondSeperator = true;
					}
				}
			}
			else {
				linkedAsBar = true;

				// we are the bar!
				bar.create();
				bar.setLabel( name );

				group.nest( bar );
			}

			doLinkChildren();

			if( addSecondSeperator ) {
				seperatorElement2.nest( getParent()->getGroup() );
				seperatorElement2.makeSeparator( std::string() );
			}

			if( !linkedAsBar && containerType == CT_GROUP ) {
				expanded.link();
			}
		}

		virtual void doUnlink() {
			if( !linkedAsBar && containerType == CT_GROUP ) {
				expanded.unlink();
			}

			doUnlinkChildren();

			seperatorElement.unnest();
			seperatorElement2.unnest();
			group.unnest();

			// unlink myself
			if( bar.hasBar() ) {
				bar.unlink();
			}
		}

	private:
		ContainerType containerType;
		std::string name;

		InternalBar bar;

		InternalElement group;
		InternalElement seperatorElement;
		InternalElement seperatorElement2;

		bool linkedAsBar;
		Expanded expanded;
	};

	struct SimpleContainer : Container {
		SimpleContainer( ContainerType containerType, const std::string &name = std::string() )
			: Container( containerType, name ) {
		}

		SimpleContainer( const std::string &name = std::string() )
			: Container( name ) {
		}

		~SimpleContainer() {
			while( !children.empty() ) {
				remove( children.back().get() );
			}
		}

		size_t size() const {
			return children.size();
		}

		void pop_back() {
			remove( children.back().get() );
		}

	protected:
		virtual void doRefresh() {
			refreshChildren();
		}

	private:
		void doAdd( const std::shared_ptr<Element> &child ) {
			children.push_back( child );
		}

		std::shared_ptr< Element > doRemove( Element *child ) {
			for( auto myChild = children.rbegin() ; myChild != children.rend() ; ++myChild ) {
				if( myChild->get() == child ) {
					auto childPtr = *myChild;
					// this is a very ugly hack, but I cant think of a better way right now
					children.erase( (myChild+1).base() );
					return childPtr;
				}
			}
			return std::shared_ptr< Element >();
		}

		virtual void doLinkChildren() {
			// link children
			for( auto child = children.begin() ; child != children.end() ; ++child ) {
				child->get()->link();
			}
		}

		virtual void doUnlinkChildren() {
			// unlink the children
			for( auto child = children.begin() ; child != children.end() ; ++child ) {
				child->get()->unlink();
			}
		}

		void refreshChildren() {
			for( auto child = children.begin() ; child != children.end() ; ++child ) {
				child->get()->refresh();
			}
		}

	private:
		std::vector< std::shared_ptr<Element> > children;
	};

	inline bool Element::changeParent( Container *newParent ) {
		if( parent == newParent ) {
			return false;
		}

		if( isLinked() ) {
			unlink();
		}
		parent = newParent;
		if( newParent && newParent->isLinked() ) {
			link();
		}
		return true;
	}

	struct Name {
		void set( const std::string &newName ) {
			bool changed = name != newName;
			name = newName;

			link();
		}

		const std::string &get() const {
			return name;
		}

		Name( InternalElement &element, const std::string &name ) : element( element ), name( name ) {}

		void link() {
			if( element.removable ) {
				element.setLabel( name );
			}
		}
	private:
		std::string name;
		InternalElement &element;
	};

	struct Separator : Element {
		typedef Element Base;

		Separator( const std::string &def = std::string() )
			:
			def( def )
		{
		}

	private:
		void doLink() {
			element.nest( getParent()->getGroup() );
			element.makeSeparator( def );
		}

		void doUnlink() {
			element.unnest();
		}

		void doRefresh() {}

		InternalElement element;
		std::string def;
	};

	inline std::shared_ptr< Separator > makeSharedSeparator( const std::string &def = std::string() ) {
		return std::make_shared< Separator >( def );
	}

	struct Button : Element {
		typedef Element Base;

		Name name;

		Button( const std::string &name, const std::function<void()> &callback, const std::string &def = std::string() )
			:
			name( element, name ),
			callback( callback ),
			def( def )
		{
		}

	private:
		void doLink() {
			element.nest( getParent()->getGroup() );
			element.makeButton( (TwButtonCallback) Button::execute, (void*) this, def );
			name.link();
		}

		void doUnlink() {
			element.unnest();
		}

		void doRefresh() {}

		static void TW_CALL execute(Button &me) {
			me.callback();
		}

		InternalElement element;
		std::string def;

		std::function<void()> callback;
	};

	inline std::shared_ptr< Button > makeSharedButton( const std::string &name, const std::function<void()> &callback, const std::string &def = std::string() ) {
		return std::make_shared< Button >( name, callback, def );
	}

	namespace Types {
		struct Quat4f {
			float coeffs[4];
		};
	}

	namespace detail {
		template< typename T >
		struct TypeMap {
			enum {
				Type = TW_TYPE_UNDEF
			};
		};

		template< typename T >
		struct TypeMapper {
			static const int &Type;
		};

		template< typename T >
		const int &TypeMapper<T>::Type = TypeMap< std::remove_cv< T >::type >::Type;

		template<>
		struct TypeMap< bool > {
			enum {
				Type = TW_TYPE_BOOLCPP
			};
		};

		template<>
		struct TypeMap< std::string > {
			enum {
				Type = TW_TYPE_STDSTRING
			};
		};

		template<>
		struct TypeMap< float > {
			enum {
				Type = TW_TYPE_FLOAT
			};
		};

		template<>
		struct TypeMap< double > {
			enum {
				Type = TW_TYPE_DOUBLE
			};
		};

		template<>
		struct TypeMap< __int32 > {
			enum {
				Type = TW_TYPE_INT32
			};
		};

		/*typedef float Color4F[4];

		template<>
		struct TypeMap< Color4F > {
			enum {
				Type = TW_TYPE_COLOR4F
			};
		};

		typedef float Direction3F[3];

		template<>
		struct TypeMap< Direction3F > {
			enum {
				Type = TW_TYPE_DIR3F
			};
		};*/

		template<>
		struct TypeMap< Types::Quat4f > {
			enum {
				Type = TW_TYPE_QUAT4F
			};
		};
	}

#if 0
	struct StructureFactory {
		typedef ... Type;

		template< typename Accessor >
		std::shared_ptr< ViewType > makeShared( Accessor &&accessor, ContainerType containerType = CT_GROUP, const std::string &name = std::string() ) const;
	};
#endif

#if 0
	template< typename Accessor >
	class ViewType {
		ViewType( Accessor &&accessor, ContainerType containerType = CT_GROUP, const std::string &name = std::string() );
	};
#endif

	template< typename _Type, template< typename Accessor > class _ViewType >
	struct StructureTypeFactory {
		typedef _Type Type;

		template< typename Accessor >
		std::shared_ptr< _ViewType< Accessor > > makeShared( Accessor &&accessor, ContainerType containerType = CT_GROUP, const std::string &name = std::string() ) const {
			return std::make_shared< _ViewType< Accessor > >( std::move( accessor ), containerType, name );
		}
	};

	template< typename Accessor >
	struct Structure : SimpleContainer {
		Accessor accessor;

		Structure( Accessor &&accessor, ContainerType containerType = CT_GROUP, const std::string &name = std::string() ) :
			SimpleContainer( containerType, name ),
			accessor( std::move( accessor ) )
		{
		}
	};

	template< typename _Type, typename _Derived >
	struct SimpleStructureFactory {
		typedef _Type Type;

		template< typename Accessor >
		std::shared_ptr< Container > makeShared( Accessor &&accessor, ContainerType containerType = CT_GROUP, const std::string &name = std::string() ) const {
			auto container = std::make_shared< Structure< Accessor > >( std::move( accessor ), containerType, name );

			static_cast<const _Derived*>(this)->setup( container.get(), container->accessor );

			return container;
		}

#if 0
		template< typename Accessor >
		void setup( AntTWBarUI::Container *container, Accessor &accessor ) const;
#endif
	};

#if 0
	template< typename Type >
	struct Accessor {
		Type & pull();
		void push();
	};
#endif

	template< typename _Type >
	struct ReferenceAccessor {
		typedef _Type Type;

		Type &variable;

		ReferenceAccessor( Type &variable ) : variable( variable ) {}

		Type & pull() {
			return variable;
		}

		void push() {
		}
	};

	template< typename _Type >
	ReferenceAccessor< _Type > makeReferenceAccessor( _Type &variable ) {
		return ReferenceAccessor< _Type >( variable );
	}

	template< typename _Type >
	struct ExpressionAccessor {
		typedef _Type Type;

		typedef std::function< _Type & () > LExpression;

		LExpression lExpression;

		ExpressionAccessor( const LExpression &lExpression ) : lExpression( lExpression ) {}

		_Type & pull() {
			return lExpression();
		}

		void push() {
		}
	};

	template< typename _Type >
	ExpressionAccessor< _Type > makeExpressionAccessor( const typename ExpressionAccessor<_Type>::LExpression &lExpression ) {
		return ExpressionAccessor< _Type >( lExpression );
	}

	template< typename _Type, typename _Accessor >
	struct LinkedExpressionAccessor {
		typedef _Type Type;

		typedef std::function< _Type & () > LExpression;

		LExpression lExpression;
		_Accessor &accessor;

		LinkedExpressionAccessor( const LExpression &lExpression, _Accessor &accessor ) : lExpression( lExpression ), accessor( accessor ) {}

		_Type & pull() {
			return lExpression();
		}

		void push() {
			accessor.push();
		}
	};

	template< typename _Type, typename _Accessor >
	LinkedExpressionAccessor< _Type, _Accessor > makeLinkedExpressionAccessor( const typename LinkedExpressionAccessor<_Type, _Accessor>::LExpression &lExpression, _Accessor &accessor ) {
		return LinkedExpressionAccessor< _Type, _Accessor >( lExpression, accessor );
	}

	template< typename _Type >
	struct CallbackAccessor {
		typedef _Type Type;

		typedef std::function<void(Type&)> Getter;
		typedef std::function<void(const Type&)> Setter;

		Getter getter;
		Setter setter;

		CallbackAccessor( const Getter &getter, const Setter &setter ) : getter( getter ), setter( setter ) {}

		Type & pull() {
			getter( shadow );
			return shadow;
		}

		void push() const {
			setter( shadow );
		}

		Type shadow;
	};

	//////////////////////////////////////////////////////////////////////////

	// only standard anttweakbar types are supported
	template< typename Accessor, bool readOnly = false, int forcedType = TW_TYPE_UNDEF >
	struct Variable : Element {
		typedef typename Accessor::Type Type;

		Accessor accessor;

		Name name;

		Variable( const std::string &name, Accessor &&accessor, const std::string &def = std::string() )
			:
			name( element, name ),
			accessor( std::move( accessor ) ),
			def( def )
		{
		}

	private:
		void doLink() {
			element.nest( getParent()->getGroup() );
			int type = forcedType;
			if( type == TW_TYPE_UNDEF ) {
				type = detail::TypeMapper< Type >::Type;
			}

			if( type == TW_TYPE_UNDEF ) {
				//error
				std::cerr << "TW_TYPE_UNDEF, unsupported type!";
			}
			else if( type == TW_TYPE_STDSTRING ) {
				element.makeVariableCB( (TwType) TW_TYPE_CDSTRING, (TwGetVarCallback) Variable::staticGetStdString, (TwSetVarCallback) (readOnly ? nullptr : Variable::staticSetStdString), (void*) this, def );
			}
			else {
				element.makeVariableCB( (TwType) type, (TwGetVarCallback) Variable::staticGet, (TwSetVarCallback) (readOnly ? nullptr : Variable::staticSet), (void*) this, def );
			}
			name.link();
		}

		void doUnlink() {
			element.unnest();
		}

		void doRefresh() {}

		static void TW_CALL staticGet(Type &value, Variable &me) {
			value = me.accessor.pull();
		}

		static void TW_CALL staticSet(const Type &value, Variable &me) {
			me.accessor.pull() = value;
			me.accessor.push();
		}

		static void TW_CALL staticGetStdString( char **value, Variable &me) {
			TwCopyCDStringToLibrary( value, reinterpret_cast<std::string*>(&me.accessor.pull())->c_str() );
		}

		static void TW_CALL staticSetStdString(const char **value, Variable &me) {
			*reinterpret_cast<std::string*>(&me.accessor.pull()) = *value;
			me.accessor.push();
		}

		InternalElement element;
		std::string def;
	};

	template< typename Accessor >
	Variable< Accessor > makeVariable( const std::string &name, Accessor &&accessor, const std::string &def = std::string() ) {
		return Variable< Accessor >( name, std::move( accessor ), def );
	}

	template< typename Accessor >
	std::shared_ptr< Variable< Accessor > > makeSharedVariable( const std::string &name, Accessor &&accessor, const std::string &def = std::string() ) {
		return std::make_shared< Variable< Accessor > >( name, std::move( accessor ), def );
	}

	template< typename Accessor >
	Variable< Accessor, true > makeReadOnlyVariable( const std::string &name, Accessor &&accessor, const std::string &def = std::string() ) {
		return Variable< Accessor, true >( name, std::move( accessor ), def );
	}

	template< typename Accessor >
	std::shared_ptr< Variable< Accessor, true > > makeSharedReadOnlyVariable( const std::string &name, Accessor &&accessor, const std::string &def = std::string() ) {
		return std::make_shared< Variable< Accessor, true > >( name, std::move( accessor ), def );
	}

	template< int forcedType, typename Accessor >
	std::shared_ptr< Variable< Accessor, false, forcedType > > makeSharedVariableWithType( const std::string &name, Accessor &&accessor, const std::string &def = std::string() ) {
		return std::make_shared< Variable< Accessor, false, forcedType > >( name, std::move( accessor ), def );
	}

	//////////////////////////////////////////////////////////////////////////
	template< typename Accessor >
	struct Label : Element {
		Accessor nameAccessor;

		Label( Accessor &&nameAccessor, const std::function<void()> &callback, const std::string &def = std::string() )
			:
			nameAccessor( std::move( nameAccessor ) ),
			callback( callback ),
			def( def )
		{
		}

	private:
		void doLink() {
			element.nest( getParent()->getGroup() );
			element.makeButton( (TwButtonCallback) Button::execute, (void*) this, def );

			updateName();
		}

		void doUnlink() {
			element.unnest();
		}

		void doRefresh() {
			updateName();
		}

		void updateName() {
			const auto &name = nameAccessor.pull();
			if( cachedName != name ) {
				cachedName = name;
				element.setLabel( name );
			}
		}

		static void TW_CALL execute(Button &me) {
			me.callback();
		}

		std::string cachedName;

		InternalElement element;
		std::string def;

		std::function<void()> callback;
	};

	template< typename Accessor >
	std::shared_ptr< Label< Accessor > > makeSharedLabel( Accessor &&nameAccessor, const std::function<void()> &callback, const std::string &def = std::string() ) {
		return std::make_shared< Label< Accessor > >( std::move( nameAccessor ), callback, def );
	}

	//////////////////////////////////////////////////////////////////////////

	template< typename _Type >
	struct ElementAccessor {
		typedef _Type Type;

		std::vector< Type > &elements;
		int elementIndex;

		ElementAccessor( std::vector< Type > &elements, int elementIndex )
			:
		elements( elements ),
			elementIndex( elementIndex )
		{
		}

		Type & pull() {
			return elements[ elementIndex ];
		}

		void push() {}
	};

	template< typename BaseAccessor, typename MemberType >
	struct MemberAccessor {
		typedef MemberType Type;
		typedef typename BaseAccessor::Type BaseType;

		BaseAccessor &baseAccessor;
		MemberType BaseType::*pointer;

		MemberAccessor( BaseAccessor &baseAccessor, MemberType BaseType::*pointer )
			:
			baseAccessor( baseAccessor ),
			pointer( pointer )
			{
		}

		MemberType &pull() {
			return baseAccessor.pull().*pointer;
		}

		void push() {
			baseAccessor.push();
		}
	};

	template< typename BaseAccessor, typename MemberType >
	MemberAccessor< BaseAccessor, MemberType > makeMemberAccessor( BaseAccessor &baseAccessor, MemberType BaseAccessor::Type::*pointer ) {
		return MemberAccessor< BaseAccessor, MemberType >( baseAccessor, pointer );
	}

	struct VectorConfigDefault {
		static const bool supportRemove = true;
	};

	template< class StructureFactory, class Config = VectorConfigDefault >
	struct Vector : SimpleContainer {
		typedef typename StructureFactory::Type Type;
		typedef ElementAccessor< Type > ElementAccessor;

		std::vector< Type > &elements;
		ContainerType elementContainerType;

		StructureFactory structureFactory;

		Vector( std::vector< Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP )
			: SimpleContainer( containerType )
			, structureFactory( std::move( structureFactory ) )
			, elementContainerType( elementContainerType )
			, elements( elements )
		{
			updateSize();
		}

		Vector( const std::string &name, std::vector< Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP )
			:
			SimpleContainer( name ),
			structureFactory( std::move( structureFactory ) ),
			elementContainerType( elementContainerType ),
			elements( elements )
		{
			updateSize();
		}

	protected:
		virtual void doRefresh() {
			SimpleContainer::doRefresh();

			updateSize();
		}

	private:
		void updateSize() {
			while( size() < elements.size() ) {
				const int index = (int) size();

				auto elementView = structureFactory.makeShared( ElementAccessor( elements, index ), elementContainerType );

				if( Config::supportRemove ) {
					elementView->add( makeSharedButton(
							"Remove",
							[this, index] () {
								elements.erase( elements.begin() + index );
								this->updateSize();
							}
						)
					);
				}

				add( elementView );
			}
			while( size() > elements.size() ) {
				pop_back();
			}
		}
	};

	template< class StructureFactory >
	Vector< StructureFactory > makeVector( std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return Vector< StructureFactory >( elements, StructureFactory(), elementContainerType, containerType );
	}

	template< class StructureFactory >
	Vector< StructureFactory > makeVector( const std::string &name, std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP ) {
		return Vector< StructureFactory >( name, elements, StructureFactory(), elementContainerType );
	}

	template< class StructureFactory >
	Vector< StructureFactory > makeVector( std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return Vector< StructureFactory >( elements, std::move( structureFactory ), elementContainerType, containerType );
	}

	template< class StructureFactory >
	Vector< StructureFactory > makeVector( const std::string &name, std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP ) {
		return Vector< StructureFactory >( name, elements, std::move( structureFactory ), elementContainerType );
	}

	template< class StructureFactory >
	std::shared_ptr< Vector< StructureFactory > > makeSharedVector( std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return std::make_shared< Vector< StructureFactory > >( elements, StructureFactory(), elementContainerType, containerType );
	}

	template< class StructureFactory >
	std::shared_ptr< Vector< StructureFactory > > makeSharedVector( const std::string &name, std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP ) {
		return std::make_shared< Vector< StructureFactory > >( name, elements, StructureFactory(), elementContainerType );
	}

	template< class StructureFactory >
	std::shared_ptr< Vector< StructureFactory > > makeSharedVector( std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return std::make_shared< Vector< StructureFactory > >( elements, std::move( structureFactory ), elementContainerType, containerType );
	}

	template< class StructureFactory >
	std::shared_ptr< Vector< StructureFactory > > makeSharedVector( const std::string &name, std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP ) {
		return std::make_shared< Vector< StructureFactory > >( name, elements, std::move( structureFactory ), elementContainerType );
	}

	//////////////////////////////////////////////////////////////////////////
	// V2
	// TODO: create a ContainerFactory that is implemented like a StructureFactory?

	// doesnt support Config::supportRemove
	template< class StructureFactory >
	struct VectorV2 : SimpleContainer {
		typedef typename StructureFactory::Type Type;
		typedef ElementAccessor< Type > ElementAccessor;

		std::vector< Type > &elements;
		ContainerType elementContainerType;

		StructureFactory structureFactory;

		VectorV2( std::vector< Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP )
			: SimpleContainer( containerType )
			, structureFactory( std::move( structureFactory ) )
			, elementContainerType( elementContainerType )
			, elements( elements )
		{
			updateSize();
		}

		VectorV2( const std::string &name, std::vector< Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP )
			:
			SimpleContainer( name ),
			structureFactory( std::move( structureFactory ) ),
			elementContainerType( elementContainerType ),
			elements( elements )
		{
			updateSize();
		}

	protected:
		virtual void doRefresh() {
			SimpleContainer::doRefresh();

			updateSize();
		}

	private:
		void updateSize() {
			while( size() < elements.size() ) {
				const int index = (int) size();

				auto elementView = structureFactory.makeShared( ElementAccessor( elements, index ), elementContainerType );
				add( elementView );
			}
			while( size() > elements.size() ) {
				pop_back();
			}
		}
	};

	template< class StructureFactory >
	VectorV2< StructureFactory > makeVectorV2( std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return VectorV2< StructureFactory >( elements, StructureFactory(), elementContainerType, containerType );
	}

	template< class StructureFactory >
	VectorV2< StructureFactory > makeVectorV2( const std::string &name, std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP ) {
		return VectorV2< StructureFactory >( name, elements, StructureFactory(), elementContainerType );
	}

	template< class StructureFactory >
	VectorV2< StructureFactory > makeVectorV2( std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return VectorV2< StructureFactory >( elements, std::move( structureFactory ), elementContainerType, containerType );
	}

	template< class StructureFactory >
	VectorV2< StructureFactory > makeVectorV2( const std::string &name, std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP ) {
		return VectorV2< StructureFactory >( name, elements, std::move( structureFactory ), elementContainerType );
	}

	template< class StructureFactory >
	std::shared_ptr< VectorV2< StructureFactory > > makeSharedVectorV2( std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return std::make_shared< VectorV2< StructureFactory > >( elements, StructureFactory(), elementContainerType, containerType );
	}

	template< class StructureFactory >
	std::shared_ptr< VectorV2< StructureFactory > > makeSharedVectorV2( const std::string &name, std::vector< typename StructureFactory::Type > &elements, ContainerType elementContainerType = CT_GROUP ) {
		return std::make_shared< VectorV2< StructureFactory > >( name, elements, StructureFactory(), elementContainerType );
	}

	template< class StructureFactory >
	std::shared_ptr< VectorV2< StructureFactory > > makeSharedVectorV2( std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP, ContainerType containerType = CT_GROUP ) {
		return std::make_shared< VectorV2< StructureFactory > >( elements, std::move( structureFactory ), elementContainerType, containerType );
	}

	template< class StructureFactory >
	std::shared_ptr< VectorV2< StructureFactory > > makeSharedVectorV2( const std::string &name, std::vector< typename StructureFactory::Type > &elements, StructureFactory &&structureFactory, ContainerType elementContainerType = CT_GROUP ) {
		return std::make_shared< VectorV2< StructureFactory > >( name, elements, std::move( structureFactory ), elementContainerType );
	}
}