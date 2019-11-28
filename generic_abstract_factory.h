#ifndef GENERIC_ABSTRACT_FACTORY_H
#define GENERIC_ABSTRACT_FACTORY_H

#include <type_traits>
#include <memory>
#include <utility>

namespace generic_abstract_factory
{
	namespace utils
	{
		template<typename...> using void_t = void;

		template<typename T>
		struct type_identity
		{
			using type = T;
		};

		template<typename T>
		using type_identity_t = typename type_identity<T>::type;

		template<typename... Ts> struct type_list;

		template<typename... Ts>
		using tl = type_list<Ts...>;

		template <class Class, class Member, class... Args>
		auto invoke(Member Class::* f, Args&& ... args)
			-> decltype((std::declval<Class>().*f)(std::forward<Args>(args)...));

		template<typename Void, typename...>
		struct is_invocable_memfn_impl : public std::false_type
		{
		};

		template<typename... Ts>
		struct is_invocable_memfn_impl<
			void_t<decltype(invoke(std::declval<Ts>()...))>, Ts...
		>
			: public std::true_type
		{
		};

		template<typename... Ts>
		struct is_invocable_memfn : public is_invocable_memfn_impl<void, Ts...>
		{
		};
		
#if __cplusplus >= 201402L
		template<typename... Ts>
		constexpr bool is_invocable_memfn_v = is_invocable_memfn<Ts...>::value;
#endif

		template<typename T, typename Ret, typename Args = utils::tl<>>
		struct make_factory_interface : public T
		{
			using ret_type = Ret;
			using ctor_args = Args;
		};

		template<typename T, typename... Args>
		struct make_factory_interface<T, utils::tl<Args...>> : public T
		{
			using ctor_args = utils::tl<Args...>;
		};

		template<typename T, typename = utils::void_t<>>
		struct get_ctor_args
		{
			using type = utils::tl<>;
		};

		template<typename T>
		struct get_ctor_args<T, utils::void_t<typename T::ctor_args>>
		{
			using type = typename T::ctor_args;
		};

		template<typename T>
		using get_ctor_args_t = typename get_ctor_args<T>::type;

		template<typename T, typename Default, typename = utils::void_t<>>
		struct get_ret_type
		{
			using type = typename Default::type;
		};

		template<typename T, typename Default>
		struct get_ret_type<T, Default, utils::void_t<typename T::ret_type>>
		{
			using type = typename T::ret_type;
		};

		template<typename T, typename Default>
		using get_ret_type_t = typename get_ret_type<T, Default>::type;

		template<template<typename...>class, typename...>
		struct generate_creators;

		template<
			template<typename...>class Creator,
			typename Root,
			typename Context,
			typename... Contexts,
			typename C0,
			typename... Cs
		>
		struct generate_creators<
			Creator, Root, utils::tl<Context, Contexts...>, utils::tl<C0, Cs...>
		>
		{
			static_assert(sizeof...(Contexts) == sizeof...(Cs),
				"Abstract and concrete lists are of different length");

			using type = Creator<
				Context,
				C0,
				typename generate_creators<
				Creator,
				Root,
				utils::tl<Contexts...>,
				utils::tl<Cs...>
				>::type
			>;
		};

		template<
			template<typename...>class Creator,
			typename Root,
			typename Context,
			typename C0
		>
			struct generate_creators<Creator, Root, utils::tl<Context>, utils::tl<C0>>
		{
			using type = Creator<Context, C0, Root>;
		};

		struct convertible_to_any
		{
			template<typename T>
			operator T();
		};
	} //namespace utils

	template<typename...> class abstract_creator_interface;

	template<typename Abstract, typename Ret, typename... Args>
	class abstract_creator_interface<Abstract, Ret, utils::tl<Args...>>
	{
	public:
		using context = utils::tl<Abstract, Ret, utils::tl<Args...>>;

		virtual Ret create(utils::type_identity<Abstract>, Args...) = 0;
		virtual ~abstract_creator_interface() = default;
	};

	template<typename Abstract>
	using default_abstract_creator = abstract_creator_interface<
		Abstract,
		utils::get_ret_type_t<
			Abstract, utils::type_identity<std::unique_ptr<Abstract>>
		>,
		utils::get_ctor_args_t<Abstract>
	>;

	template<typename...> class default_concrete_creator;

	template<
		typename Abstract,
		typename Concrete,
		typename Base,
		typename Ret,
		typename... Args
	>
	class default_concrete_creator<
		utils::tl<Abstract, Ret, utils::tl<Args...>>, Concrete, Base
	>
		: public Base
	{
		static_assert(std::is_constructible<Concrete, Args...>::value,
			"Product is not constructible from a given set of arguments");
		static_assert(std::is_constructible<Ret, Concrete*>::value,
			"ret_type is not constructible from Concrete*");
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
		Ret create(utils::type_identity<Abstract>, Args... args) override
		{
			return Ret{ new Concrete(std::forward<Args>(args)...) };
		}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	};

	template<
		typename AbstractList,
		template<typename...>class Creator = default_abstract_creator
	>
	class abstract_factory;

	template<typename... AbstractList, template<typename...>class Creator>
	class abstract_factory<utils::tl<AbstractList...>, Creator>
		: protected Creator<AbstractList>...
	{
	public:
		using context_list = utils::tl<typename Creator<AbstractList>::context...>;

		template<typename Abstract, typename... Args,
			typename = typename std::enable_if<std::is_base_of<
				Creator<Abstract>,
				abstract_factory
			>::value>::type
		>
		auto create(Args&& ...args) ->
			decltype(Creator<Abstract>::create(
				utils::type_identity<Abstract>{}, std::forward<Args>(args)...)
			)
		{
			Creator<Abstract>* creator = this;

			return creator->create(
				utils::type_identity<Abstract>{}, std::forward<Args>(args)...
			);
		}

		template<typename Abstract, typename... Args, 
			typename = typename std::enable_if<!std::is_base_of<
				Creator<Abstract>,
				abstract_factory
			>::value  
			|| !utils::is_invocable_memfn<
				decltype(&Creator<Abstract>::create),
				utils::type_identity<Abstract>,
				Args...
			>::value>::type>
		utils::convertible_to_any create(Args && ...)
		{
			static_assert(std::is_base_of<
				Creator<Abstract>,
				abstract_factory
			>::value,
				"abstract_factory::create(): wrong product type"
			);
			
			static_assert(utils::is_invocable_memfn<
				decltype(&Creator<Abstract>::create),
				utils::type_identity<Abstract>,
				Args...
			>::value,
				"abstract_factory::create(): wrong arguments"
			);
			
			return {};
		}
	};

	template<
		typename AbstractFactory,
		typename ConcreteList,
		template<typename...>class Creator = default_concrete_creator
	>
	class concrete_factory
		: public utils::generate_creators<
			Creator,
			AbstractFactory,
			typename AbstractFactory::context_list,
			ConcreteList
		>::type
	{
	};
} // namespace generic_abstract_factory

#endif // GENERIC_ABSTRACT_FACTORY_H