Generic abstract factory
========================

This is a C++ 11 single-header implementation of [abstract factory pattern](https://en.wikipedia.org/wiki/Abstract_factory_pattern)
in a generic way. Mostly inspired by A. Alexandrescu and his book ["Modern C++ design"](https://www.amazon.com/Modern-Design-Generic-Programming-Patterns/dp/0201704315) but implemented using modern C++.

## Examples

### Basic usage
Pass interfaces to `abstract_factory`, concretes to `concrete_factory` and it
will automate their creation.
```c++
using AFactory = abstract_factory<utils::tl<IProductA, IProductB>>;
using CFactory = concrete_factory<AFactory, utils::tl<ProductA, ProductB>>;

CFactory concreteFactory;
AFactory* abstractFactory = &concreteFactory;

std::unique_ptr<IProductA> a = abstractFactory->create<IProductA>();
std::unique_ptr<IProductB> b = abstractFactory->create<IProductB>();
```

### Customize product return type
Default product return type is `std::unique_ptr<T>` but it can be changed:
```c++
struct IProductA
{
    using ret_type = std::shared_ptr<IProductA>;
};

std::shared_ptr<IProductA> a = abstractFactory->create<IProductA>();
std::unique_ptr<IProductB> b = abstractFactory->create<IProductB>();
```
You can also change default product return type for the whole factory using 
custom `abstract_creator`, see *Customize abstract_creator* section.  
Note that return type doesn't have to be the pointer, plain values are also 
supported, see *Customize concrete_creator* section.

### Customize product arguments
You can specify `create()` arguments for specific product, by default they will
be passed to concrete product's constructor.
```c++
struct IProductA
{
    using ctor_args = utils::tl<int, bool>;
    using ret_type = std::shared_ptr<IProductA>;
};
struct ProductA
{
    ProductA(int, bool) {}
};

std::shared_ptr<IProductA> a = abstractFactory->create<IProductA>(1, true);
```

### Customize abstract creator
Internally, temlpated `create<>()` forwards call to virtual function `create()`
that declared for each product type using abstract creator. It's purpose is 
to create function signature from given interface. `default_abstract_creator` uses members 
`ret_type` and `ctor_args` for product return type and function arguments 
accordingly, but you can create your own implementation that will work in 
another way. Here is example of changing default return type, which is
`std::unique_ptr<T>` by default, to raw pointer:
```c++
struct IProductA {};

//make raw pointer default ret_type
template<typename Abstract>
using CustomAbstractCreator = abstract_creator_interface<
    Abstract,
    utils::get_ret_type_t<Abstract, utils::type_identity<Abstract*>>,
    utils::get_ctor_args_t<Abstract>
>;

using AFactory = abstract_factory<
    utils::tl<IProductA>, CustomAbstractCreator>;
using CFactory = concrete_factory<AFactory, utils::tl<ProductA>>;

CFactory concreteFactory;
AFactory* abstractFactory = &concreteFactory;

IProductA *a = abstractFactory->create<IProductA>();
```

### Customize concrete creator
Concrete creator actually creates products. `default_concrete_creator` can 
handle situations when expression `ret_type{ new Product(ctor_args...) }` 
is valid. In other words, if product can be constructed from arguments specified
in its `ctor_args` and `ret_type` is raw/unique/shared pointer, default creator
will do the work.  
You can customize it for values or any special creation policy:
```c++
struct IValue
{
	using ret_type = int;
};

template<typename Context, typename Concrete, typename Base, typename Enabled = void>
class CustomCreator
	: public default_concrete_creator<Context, Concrete, Base>
{
};

//specialization for IValue
template<typename Concrete, typename Base, typename Ret, typename... Args>
class CustomCreator<utils::tl<IValue, Ret, utils::tl<Args...>>, Concrete, Base>
	: public Base
{
	Ret create(utils::type_identity<IValue>, Args... args) override
	{
		return 12;
	}
};

using AFactory = abstract_factory<utils::tl<IValue>>;
using CFactory = concrete_factory<AFactory, utils::tl<IValue>, CustomCreator>;

CFactory concreteFactory;
AFactory* abstractFactory = &concreteFactory;

int value = abstractFactory->create<IValue>();
```
For example of using prototype-based creator, see main.cpp.

### Adapt existing interfaces
If you have interface and you need to use `ret_type`/`ctor_args` but you 
can't/don't want to change it, there's a way to adapt it:
```c++
/*
You can specify either ret_type or ctor_args:
using IAdaptedProduct = utils::make_factory_interface<
	IProductA, std::shared_ptr<IProductA>
>;
using IAdaptedProduct = utils::make_factory_interface<
	IProductA, utils::tl<int, bool>
>;
*/
using IAdaptedProduct = utils::make_factory_interface<
	IProductA, std::shared_ptr<IProductA>, utils::tl<int, bool>
>;
using AFactory = abstract_factory<utils::tl<IAdaptedProduct>>;
using CFactory = concrete_factory<AFactory, utils::tl<ProductA>>;

std::shared_ptr<IProductA> a = abstractFactory->create<IAdaptedProduct>(1, true);
```

### Error detection
It detects common mistakes:
- wrong product type:
```c++
using AFactory = abstract_factory<utils::tl<IProductA, IProductB>>;
using CFactory = concrete_factory<AFactory, utils::tl<ProductA, ProductB>>;

CFactory concreteFactory;
AFactory* abstractFactory = &concreteFactory;

// static_assert: "abstract_factory::create(): wrong product type"
auto c = abstractFactory->create<IProductC>();
```
- wrong arguments for product:
```c++
struct IProductA
{
	using ctor_args = utils::tl<int, bool>;
	using ret_type = std::shared_ptr<IProductA>;
};
struct ProductA
{
	ProductA(int, bool) {}
};

// static_assert: "abstract_factory::create(): wrong arguments"
std::shared_ptr<IProductA> a = abstractFactory->create<IProductA>(1, std::string{});
```
- ill-formed product list:
```c++
// static_assert: "Abstract and concrete lists are of different length"
using AFactory = abstract_factory<utils::tl<IProductA, IProductB>>;
using CFactory = concrete_factory<AFactory, utils::tl<ProductA>>;
```
- usage of default concrete creator with product that can't be constructed from `ctor_args`:
```c++
struct IProductA
{
	using ctor_args = utils::tl<int, bool>;
	using ret_type = std::shared_ptr<IProductA>;
};
struct ProductA
{
	//ProductA(int, bool) {}
};

//static_assert: "Product is not constructible from a given set of arguments"
std::shared_ptr<IProductA> a = abstractFactory->create<IProductA>(1, true);
```
- usage of default concrete creator when `ret_type` is not constructible from `Product*`:
```c++
struct IProductA
{
	using ctor_args = utils::tl<int, bool>;
	using ret_type = int;
};
struct ProductA
{
	ProductA(int, bool) {}
};

//static_assert: "ret_type is not constructible from Concrete*"
auto a = abstractFactory->create<IProductA>(1, true);
```