#pragma once

/* example

struct MyContext : AsExecutionContext<MyContext> {
	int someValue;
	int otherValue;

	void setDefault() {
		someValue = 0;
		otherValue = 1;
	}
};

void funcC() {
	// can only be called if a context has been created in one of the callers
	std::cout << "funcC\n";
	std::cout << MyContext::context->someValue << "\n";
	std::cout << MyContext::context->otherValue << "\n";
}

void funcB() {
	MyContext context;
	context.someValue++;

	std::cout << "funcB\n";
	std::cout << context.someValue << "\n";
	std::cout << context.otherValue << "\n";

	funcC();
}

void funcA() {
	std::cout << "funcA\n";
	// funcB works anyhow because it obtains a default context
	funcB();
	// funcC(); would fail here

	std::cout << "creating context in funcA\n";
	MyContext context;
	context.otherValue++;

	funcB();
	funcC();
}
*/

// only empty default constructors supported! overwrite setDefault() or similar
template<typename Context>
struct AsExecutionContext {
	static Context *context;

	AsExecutionContext() {
		if( context ) {
			fromNonEmpty();
		}
		else {
			fromEmpty();
		}

		push();
	}

	struct ExpectEmpty {};

	AsExecutionContext( ExpectEmpty ) {
		if( context ) {
			throw std::logic_error( "expected empty context stack!" );
		}

		fromEmpty();
		push();
	}

	struct ExpectNonEmpty {};

	AsExecutionContext( ExpectNonEmpty ) {
		if( !context ) {
			throw std::logic_error( "expected nonempty context stack!" );
		}

		fromNonEmpty();
		push();
	}

	~AsExecutionContext() {
		pop();
	}

	void setDefault() {}
	// called after the new context has been set
	void onPush() {}
	// called after the previous context has been restored
	void onPop() {}

private:
	void fromNonEmpty() {
		*static_cast<Context*>(this) = *context;
	}

	void fromEmpty() {
		static_cast<Context*>(this)->setDefault();
	}

	void push() {
		previous = context;
		context = static_cast<Context*>(this);

		static_cast<Context*>(this)->onPush();
	}

	// TODO: this is half scope half not, mostly bullshit and needs a redesign... ie what are the real aims of this class?

	void pop() {
		context = previous;

		if( context )  {
			context->onPop();
		}
	}

	Context *previous;
};

template<typename Context>
Context *AsExecutionContext<Context>::context = nullptr;

// simple Scope class for a global variable stack
template<typename Context>
class Scope {
	Context previous;
	Context *holder;

	Scope( Context &context ) : previous( context ), holder( &context ) {}

	void reset() {
		previous = *holder;
	}

	void restore() {
		if( holder ) {
			*holder = previous;
		}
	}

	void release() {
		holder = nullptr;
	}

	~Scope() {
		restore();
	}
};

// what about inheritance?

/*
TODO: lean execution context using static

template<typename ContextData>
struct Context {
	static const ContextData &get() {
		return context;
	}

	Context() : previous( context ) {
	}

	~Context() {
		context = previous;
	}

private:
	static ContextData context;
	ContextData previous;
};

template<typename Context>
struct AsLeanExecutionContext {
	static const Context context;
	const Context *previous;

	AsExecutionContext() {
		if( static_cast<Context*>(this) != &context ) {
			*static_cast<Context*>(this) = context;
		}

		context.previous = this;
	}

	~AsExecutionContext() {
		context = *previous;
	}

	void setDefault() {}
};*/

