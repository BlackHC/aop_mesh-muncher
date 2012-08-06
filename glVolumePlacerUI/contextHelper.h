#pragma once

// only empty default constructors supported! use default_values() method or similar
template<typename Context>
struct AsExecutionContext {
	static const Context *context;
	const Context *previous;

	AsExecutionContext() {
		if( context ) {
			*static_cast<Context*>(this) = *context;
		}
		else {
			static_cast<Context*>(this)->setDefault();
		}

		previous = context;
		context = static_cast<const Context*>(this);
	}

	~AsExecutionContext() {
		context = previous;
	}

	void setDefault() {}
};

template<typename Context>
const Context *AsExecutionContext<Context>::context = nullptr;

/*
TODO: lean execution context using static and 


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
