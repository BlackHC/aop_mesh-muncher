#pragma once
#include <vector>
#include <functional>
#include <utility>

#include "anttwbargroup.h"

#include <boost/lexical_cast.hpp>

namespace AntTWBarUI {
	// support two creation modes: simply wrap an object or make it more complex by instantiating the UI element yourself

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
			TwSetParam( twBar, NULL, "label", TW_PARAM_CSTRING, 1, label.c_str() );
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
		bool spawned;

		InternalElement() : topLevel( false ), group( nullptr ), bar( nullptr ), spawned( false ) {}
		
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
			TwSetParam( bar->twBar, internalName.c_str(), "label", TW_PARAM_CSTRING, 1, label.c_str() );
		}

		void unnest() {
			remove();

			group = nullptr;
			bar = nullptr;
			topLevel = false;
		}

		void remove() {
			if( spawned ) {
				// remove ourself whatever we are
				TwRemoveVar( bar->twBar, internalName.c_str() );
				spawned = false;
			}
		}

		void makeSeparator( const std::string &def ) {
			TwAddSeparator( bar->twBar, internalName.c_str(), def.c_str() );
			spawned = true;

			setGroup();
		}

		void makeGroup() {
			setGroup();
		}

		void makeButton( TwButtonCallback callback, void *data, const std::string &def ) {
			TwAddButton( bar->twBar, internalName.c_str(), callback, data, def.c_str() );
			spawned = true;

			setGroup();
		}

		void makeVariableRW( TwType type, void *var, const std::string &def ) {
			TwAddVarRW( bar->twBar, internalName.c_str(), type, var, def.c_str() );
			spawned = true;

			setGroup();
		}

		void makeVariableCB( TwType type, TwGetVarCallback getCallback, TwSetVarCallback setCallback, void *clientData, const std::string &def) {
			TwAddVarCB( bar->twBar, internalName.c_str(), type, setCallback, getCallback, clientData, def.c_str() );
			spawned = true;

			setGroup();
		}
	};

	struct Container;

	struct Element {
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

		virtual ~Element() {}

	protected:
		virtual void doLink() {}
		virtual void doUnlink() {}

	private:
		// returns false, if the new parent is the old parent
		bool changeParent( Container *newParent );

		friend struct Container;

		bool linked;
		Container *parent;
	};

	struct Container : Element {
		typedef Element Base;

		Container( bool embed )
			: embed( embed ), label( " " ) {
		}

		Container( const std::string &name = " " )
			: embed( false ), label( name ) {
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
			if( !embed || !parent ) {
				return group;
			}
			else /* embed && parent */ { 
				return parent->getGroup();
			}
		}

		void setName( const std::string &name ) {
			label = name;
		}

	protected:
		virtual void doAdd( const std::shared_ptr< Element > &child ) = 0;
		// returns if the child has been found and removed (ie the important part being if it was in the container)
		virtual std::shared_ptr< Element > doRemove( Element *child ) = 0;

		virtual void doLink() {
			Base::doLink();

			if( getParent() ) {
				// we have a parent, so we are just a group
				bar.destroy();

				if( !embed ) {
					group.nest( getParent()->getGroup() );
					dummyElement.nest( group );
					dummyElement.makeSeparator( "visible=false" );
					
					group.makeGroup();
					group.setLabel( label );
				}
				else {
					dummyElement.nest( getParent()->getGroup() );
					dummyElement.makeSeparator( std::string() );
				}
			}
			else {
				// we are the bar!
				bar.create();
				bar.setLabel( label );

				group.nest( bar );
			}
		}

		virtual void doUnlink() {
			// unlink myself
			if( bar.hasBar() ) {
				bar.unlink();
			} 
			else {
				// unlink the dummy element
				dummyElement.unnest();
				group.unnest();
			}

			Base::doUnlink();
		}

	private:
		bool embed;
		std::string label;

		InternalBar bar;

		InternalElement group;
		InternalElement dummyElement;
	};

	struct SimpleContainer : Container {
		typedef Container Base;

		SimpleContainer( bool embed )
			: Container( embed ) {
		}

		SimpleContainer( const std::string &name = " " )
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

		virtual void doLink() {
			Base::doLink();

			// link children
			for( auto child = children.begin() ; child != children.end() ; ++child ) {
				child->get()->link();	
			}
		}

		virtual void doUnlink() {
			// unlink the children
			for( auto child = children.begin() ; child != children.end() ; ++child ) {
				child->get()->unlink();	
			}

			Base::doUnlink();
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
			if( element.spawned ) {
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
			Base::doLink();

			element.nest( getParent()->getGroup() );
			element.makeSeparator( def );
		}

		InternalElement element;
		std::string def;
	};

	std::shared_ptr< Separator > makeSharedSeparator( const std::string &def = std::string() ) {
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
			Base::doLink();

			element.nest( getParent()->getGroup() );
			element.makeButton( (TwButtonCallback) Button::execute, (void*) this, def );
			name.link();
		}

		void doUnlink() {
			element.unnest();

			Base::doUnlink();
		}

		static void TW_CALL execute(Button &me) {
			me.callback();
		}

		InternalElement element;
		std::string def;

		std::function<void()> callback;
	};

	std::shared_ptr< Button > makeSharedButton( const std::string &name, const std::function<void()> &callback, const std::string &def = std::string() ) {
		return std::make_shared< Button >( name, callback, def );
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

		typedef float Color4F[4];

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
		};
	}

#if 0
	struct TypeView {
		typedef Type Type;

		template< typename Accessor >
		void create( Container *container, Accessor &accessor ) const;
	};
#endif

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

		Type & pull() {
			return variable;
		}

		void push() {
		}
	};

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
		}

		void push() const {
			setter( shadow );
		}

	private:
		Type shadow;
	};

	//////////////////////////////////////////////////////////////////////////

	// only standard anttweakbar types are supported
	template< typename Accessor >
	struct Variable : Element {
		typedef Element Base;
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
			Base::doLink();

			element.nest( getParent()->getGroup() );
			const int type = detail::TypeMapper< Type >::Type;
			if( type == TW_TYPE_UNDEF ) {
				//error
				std::cerr << "TW_TYPE_UNDEF, unsupported type!";
			}
			else if( type == TW_TYPE_STDSTRING ) {
				element.makeVariableCB( (TwType) TW_TYPE_CDSTRING, (TwGetVarCallback) Variable::staticGetStdString, (TwSetVarCallback) Variable::staticSetStdString, (void*) this, def );
			}
			else {
				element.makeVariableCB( (TwType) type, (TwGetVarCallback) Variable::staticGet, (TwSetVarCallback) Variable::staticSet, (void*) this, def );
			}
			name.link();
		}

		void doUnlink() {
			element.unnest();

			Base::doUnlink();
		}

		static void TW_CALL staticGet(Type &value, Variable &me) {
			value = me.accessor.pull();
		}

		static void TW_CALL staticSet(const Type &value, Variable &me) {
			me.accessor.pull() = value;
			me.accessor.push();
		}

		static void TW_CALL staticGetStdString( char **value, Variable &me) {
			TwCopyCDStringToLibrary( value, me.accessor.pull().c_str() );
		}

		static void TW_CALL staticSetStdString(const char **value, Variable &me) {
			me.accessor.pull() = *value;
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

	//////////////////////////////////////////////////////////////////////////
	template< typename Accessor, typename TypeView >
	struct StructuredVariable : SimpleContainer {
		Accessor accessor;
		
		StructuredVariable( Accessor &&accessor, const TypeView &typeView, bool embed = false ) : SimpleContainer( embed ), accessor( std::move( accessor ) ) {
			typeView.create<Accessor>( this, this->accessor );
		}
	};

	template< typename TypeView, typename Accessor >
	std::shared_ptr< StructuredVariable< Accessor, TypeView > > makeSharedVariableView( Accessor &&accessor, const TypeView &typeView, bool embed = false ) {
		return std::make_shared< StructuredVariable< Accessor, TypeView > >( std::move( accessor ), typeView, embed );
	}

	template< typename TypeView, typename Accessor >
	std::shared_ptr< StructuredVariable< Accessor, TypeView > > makeSharedVariableView( Accessor &&accessor, bool embed = false ) {
		return std::make_shared< StructuredVariable< Accessor, TypeView > >( std::move( accessor ), TypeView(), embed );
	}


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

	template< class TypeView, class Config = VectorConfigDefault >
	struct Vector : SimpleContainer {
		typedef typename TypeView::Type Type;
		typedef ElementAccessor< Type > ElementAccessor;
		typedef StructuredVariable< ElementAccessor, TypeView > StructuredVariable;

		std::vector< Type > &elements;
		bool embedElements;

		TypeView typeView;
		
		Vector( std::vector< Type > &elements, TypeView &&typeView, bool embedElements = false, bool embed = false )
			: 
			SimpleContainer( embed ),
			typeView( std::move( typeView ) ),
			embedElements( embedElements ),
			elements( elements )
			{
			updateSize();
		}

		Vector( const std::string &name, std::vector< Type > &elements, TypeView &&typeView, bool embedElements = false )
			: 
			SimpleContainer( name ),
			typeView( std::move( typeView ) ),
			embedElements( embedElements ),
			elements( elements )
			{
			updateSize();
		}

		void updateSize() {
			while( size() < elements.size() ) {
				const int index = (int) size();

				auto elementView = makeSharedVariableView( ElementAccessor( elements, index ), typeView, embedElements );
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

	template< class TypeView >
	Vector< TypeView > makeVector( std::vector< typename TypeView::Type > &elements, bool embedElements = false, bool embed = false ) {
		return Vector< TypeView >( elements, TypeView(), embedElements, embed );
	}

	template< class TypeView >
	Vector< TypeView > makeVector( const std::string &name, std::vector< typename TypeView::Type > &elements, bool embedElements = false ) {
		return Vector< TypeView >( name, elements, TypeView(), embedElements );
	}

	template< class TypeView >
	Vector< TypeView > makeVector( std::vector< typename TypeView::Type > &elements, TypeView &&typeView, bool embedElements = false, bool embed = false ) {
		return Vector< TypeView >( elements, std::move( typeView ), embedElements, embed );
	}

	template< class TypeView >
	Vector< TypeView > makeVector( const std::string &name, std::vector< typename TypeView::Type > &elements, TypeView &&typeView, bool embedElements = false ) {
		return Vector< TypeView >( name, elements, std::move( typeView ), embedElements );
	}

	template< class TypeView >
	std::shared_ptr< Vector< TypeView > > makeSharedVector( std::vector< typename TypeView::Type > &elements, bool embedElements = false, bool embed = false ) {
		return std::make_shared< Vector< TypeView > >( elements, TypeView(), embedElements, embed );
	}

	template< class TypeView >
	std::shared_ptr< Vector< TypeView > > makeSharedVector( const std::string &name, std::vector< typename TypeView::Type > &elements, bool embedElements = false ) {
		return std::make_shared< Vector< TypeView > >( name, elements, TypeView(), embedElements );
	}

	template< class TypeView >
	std::shared_ptr< Vector< TypeView > > makeSharedVector( std::vector< typename TypeView::Type > &elements, TypeView &&typeView, bool embedElements = false, bool embed = false ) {
		return std::make_shared< Vector< TypeView > >( elements, std::move( typeView ), embedElements, embed );
	}

	template< class TypeView >
	std::shared_ptr< Vector< TypeView > > makeSharedVector( const std::string &name, std::vector< typename TypeView::Type > &elements, TypeView &&typeView, bool embedElements = false ) {
		return std::make_shared< Vector< TypeView > >( name, elements, std::move( typeView ), embedElements );
	}
}