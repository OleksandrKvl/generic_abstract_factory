#include <memory>
#include <cassert>

#include "generic_abstract_factory.h"

#define TYPE_ASSERT(variable, type) \
	static_assert(std::is_same<decltype(variable), type>::value, \
		"Type should be "#type)

using namespace generic_abstract_factory;

struct IUniqueProduct
{
	virtual ~IUniqueProduct() = default;
};

struct ISharedProduct
{
	using ret_type = std::shared_ptr<ISharedProduct>;

	virtual ~ISharedProduct() = default;
};

struct IRawProduct
{
	using ret_type = IRawProduct *;
	using ctor_args = utils::tl<bool, int>;

	virtual ~IRawProduct() = default;
};

template<typename T>
struct IValueProduct
{
	using ret_type = T;
	using ctor_args = utils::tl<T>;
};

template<int N>
struct IPrototypeProduct
{
	using prototype_t = std::unique_ptr<IPrototypeProduct>;
	virtual prototype_t Clone() = 0;

	virtual ~IPrototypeProduct() = default;
};

// existing product that you don't want to change
// and that should be created as shared_ptr
struct IExistingSharedProduct
{
	virtual ~IExistingSharedProduct() = default;
};

struct ExistingSharedProduct : public IExistingSharedProduct {};

template<int N>
struct PrototypeProduct : public IPrototypeProduct<N>
{
	using abstract_t = IPrototypeProduct<N>;

	typename abstract_t::prototype_t Clone() override
	{
		return typename abstract_t::prototype_t{ new PrototypeProduct() };
	}
};

struct UniqueProduct : public IUniqueProduct {};
struct SharedProduct : public ISharedProduct {};
struct RawProduct : public IRawProduct
{
	RawProduct(bool, int)
	{
	}
};
using IIntValue = IValueProduct<int>;
using IFloatValue = IValueProduct<float>;
using PrototypeProductA = PrototypeProduct<0>;
using PrototypeProductB = PrototypeProduct<1>;
using IExistingFactoryProduct = utils::make_factory_interface<
	IExistingSharedProduct, std::shared_ptr<IExistingSharedProduct>
>;


//helper to detect prototype_t member
template<typename T, typename = utils::void_t<>>
struct has_prototype : public std::false_type
{};

template<typename T>
struct has_prototype<T, utils::void_t<typename T::prototype_t>> : public std::true_type
{};

#if __cplusplus >= 201402L
template<typename T>
constexpr bool has_prototype_v = has_prototype<T>::value;
#endif

//checks whether Target is in type list
template<typename...> struct is_any_of;

template<typename Target, typename T, typename... Ts>
struct is_any_of<Target, utils::tl<T, Ts...>> : public is_any_of<Target, utils::tl<Ts...>>
{
};

template<typename Target, typename... Ts>
struct is_any_of<Target, utils::tl<Target, Ts...>> : public std::true_type
{
};

template<typename Target>
struct is_any_of<Target, utils::tl<>> : public std::false_type
{
};

#if __cplusplus >= 201402L
template<typename... Ts>
constexpr bool is_any_of_v = is_any_of<Ts...>::value;
#endif

//use default_concrete_creator for raw/unique/shared ptr products
template<typename Context, typename Concrete, typename Base, typename Enabled = void>
class CustomConcreteCreator
	: public default_concrete_creator<Context, Concrete, Base>
{
};

//specialization for values
template<typename Abstract, typename Concrete, typename Base, typename Ret, typename Arg>
class CustomConcreteCreator<utils::tl<Abstract, Ret, utils::tl<Arg>>, Concrete, Base,
	typename std::enable_if<is_any_of<Abstract, utils::tl<IIntValue, IFloatValue>>::value>::type>
	: public Base
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	Ret create(utils::type_identity<Abstract>, Arg arg) override
	{
		return arg;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

//specialization for prototypes
template<typename Abstract, typename Concrete, typename Base, typename Ret, typename... Args>
class CustomConcreteCreator<utils::tl<Abstract, Ret, utils::tl<Args...>>, Concrete, Base,
	typename std::enable_if<has_prototype<Abstract>::value>::type>
	: public Base
{
public:
	friend void SetPrototype(CustomConcreteCreator& self, typename Abstract::prototype_t newPrototype)
	{
		self.prototype = std::move(newPrototype);
	}
private:
	typename Abstract::prototype_t prototype{};
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
	Ret create(utils::type_identity<Abstract>, Args...) override
	{
		return prototype->Clone();
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

using AFactory = abstract_factory<
	utils::tl<
		IUniqueProduct, ISharedProduct, IRawProduct,
		IIntValue, IFloatValue,
		PrototypeProductA::abstract_t, PrototypeProductB::abstract_t,
		IExistingFactoryProduct
	>
>;

using CFactory = concrete_factory<AFactory, utils::tl<
		UniqueProduct, SharedProduct, RawProduct,
		IIntValue, IFloatValue,
		PrototypeProductA::abstract_t, PrototypeProductB::abstract_t,
		ExistingSharedProduct
	>,
	CustomConcreteCreator
>;

int main()
{
	CFactory concreteFactory;
	AFactory* abstractFactory = &concreteFactory;

	auto unique = abstractFactory->create<IUniqueProduct>();
	TYPE_ASSERT(unique, std::unique_ptr<IUniqueProduct>);
	assert(unique);

	auto shared = abstractFactory->create<ISharedProduct>();
	TYPE_ASSERT(shared, std::shared_ptr<ISharedProduct>);
	assert(shared);

	auto raw = abstractFactory->create<IRawProduct>(true, 1);
	TYPE_ASSERT(raw, IRawProduct*);
	assert(raw);
	
	auto intValue = abstractFactory->create<IIntValue>(12);
	TYPE_ASSERT(intValue, int);
	assert(intValue == 12);

	auto floatValue = abstractFactory->create<IFloatValue>(0.5);
	TYPE_ASSERT(floatValue, float);
	assert(floatValue == 0.5);

	SetPrototype(
		concreteFactory,
		std::unique_ptr<PrototypeProductA>{ new PrototypeProductA() }
	);
	SetPrototype(
		concreteFactory,
		std::unique_ptr<PrototypeProductB>{ new PrototypeProductB() }
	);

	auto prototypeA = abstractFactory->create<PrototypeProductA::abstract_t>();
	TYPE_ASSERT(prototypeA, std::unique_ptr<PrototypeProductA::abstract_t>);
	assert(prototypeA);

	auto prototypeB = abstractFactory->create<PrototypeProductB::abstract_t>();
	TYPE_ASSERT(prototypeB, std::unique_ptr<PrototypeProductB::abstract_t>);
	assert(prototypeB);

	auto existingProduct = abstractFactory->create<IExistingFactoryProduct>();
	TYPE_ASSERT(existingProduct, std::shared_ptr<IExistingSharedProduct>);
	assert(existingProduct);

	return 0;
}