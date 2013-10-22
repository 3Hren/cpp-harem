#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/vector.hpp>

#define DECLARE_TAG(_value_type_, _class_) \
    struct tag_##_class_ {}; \
    struct _class_ { \
        typedef tag_##_class_ tag_type; \
        typedef _value_type_ value_type; \
        std::shared_ptr<error_context::info_impl_t<tag_type, value_type>> ptr; \
        _class_(const value_type& v) : \
            ptr(new error_context::info_impl_t<tag_type, value_type>(v)) {} \
        inline static std::size_t hash() { return typeid(tag_type).hash_code(); } \
    }

namespace cppharem {

namespace type_traits {
    namespace vector {
        template<typename... Args>
        struct from_variadic;

        template<typename T, typename... Args>
        struct from_variadic<T, Args...> {
            typedef typename boost::mpl::push_back<
                typename from_variadic<Args...>::type,
                T
            >::type type;
        };

        template<typename T>
        struct from_variadic<T> {
            typedef boost::mpl::vector<T> type;
        };

        template<>
        struct from_variadic<> {
            typedef boost::mpl::vector<> type;
        };
    }

    namespace set {
        template<typename... Args>
        struct from_variadic;

        template<typename T, typename... Args>
        struct from_variadic<T, Args...> {
            typedef typename boost::mpl::insert<
                typename from_variadic<Args...>::type,
                T
            >::type type;
        };

        template<typename T>
        struct from_variadic<T> {
            typedef boost::mpl::set<T> type;
        };

        template<>
        struct from_variadic<> {
            typedef boost::mpl::set<> type;
        };
    }

    template<typename... Args>
    struct has_duplicates {
        typedef typename boost::mpl::size<
            typename vector::from_variadic<Args...>::type
        > vector_size;

        typedef typename boost::mpl::size<
            typename set::from_variadic<Args...>::type
        > set_size;

        static const bool value = vector_size::value != set_size::value;
    };
}

namespace error_context {
    struct info_t {
        virtual
        ~info_t() {}

        virtual
        std::string
        str() const = 0;
    };

    template<typename Tag, typename T>
    struct info_impl_t : public info_t {
        typedef Tag tag_type;
        typedef T value_type;

        value_type value;

        info_impl_t(const T& value) :
            value(value)
        {}

        std::string
        str() const {
            return boost::lexical_cast<std::string>(value);
        }
    };

    namespace detail {
        static inline
        std::string
        substitute(boost::format&& message) {
            return message.str();
        }

        template<typename T, typename... Args>
        static inline
        std::string
        substitute(boost::format&& message, const T& arg, const Args&... args) {
            return substitute(std::move(message % arg.ptr->value), args...);
        }
    } // namespace detail

    template<typename... Args>
    std::string
    format(const std::string& fmt, const Args&... args) {
        boost::format formatted(fmt);
        return detail::substitute(std::move(formatted), args...);
    }
} // namespace error_context

class error_t : public std::runtime_error {
    std::string m_reason;
    std::vector<std::size_t> m_hash_index;
    std::unordered_map<std::size_t, std::shared_ptr<error_context::info_t>> m_info_map;

public:
    template<typename... Args>
    explicit error_t(const std::string& reason, Args&&... args) :
        std::runtime_error(reason),
        m_reason(error_context::format(reason, args...))
    {
        static_assert(!type_traits::has_duplicates<Args...>::value, "exception tags must not contain duplicates");
        index(std::forward<Args>(args)...);
    }

    const char*
    what() const throw() {
        return m_reason.c_str();
    }

    template<typename T>
    typename T::value_type const&
    get() const {
        typedef typename T::tag_type tag_type;
        typedef typename T::value_type value_type;
        typedef error_context::info_impl_t<tag_type, value_type> info_impl;

        auto it = m_info_map.find(T::hash());
        if (it == m_info_map.end()) {
            throw std::runtime_error("no such tag");
        }
        const std::shared_ptr<error_context::info_t>& t = it->second;
        info_impl* info = static_cast<info_impl*>(t.get());
        return info->value;
    }

private:
    template<typename T>
    inline
    void
    index(T&& t) {
        std::size_t hash = T::hash();
        m_hash_index.push_back(hash);
        m_info_map.insert(std::make_pair(hash, t.ptr));
    }

    template<typename T, typename... Args>
    inline
    void
    index(T&& t, Args&&... args) {
        index(std::move(t));
        index(std::forward<Args>(args)...);
    }
};

DECLARE_TAG(std::string, key_t);
DECLARE_TAG(std::string, collection_t);

}

#include <gtest/gtest.h>

using namespace cppharem;
using namespace ::testing;

TEST(Error, WTF) {
    try {
        throw error_t("le message '%s' - '%s'", cppharem::key_t("le key"), cppharem::collection_t("la collection"));
    } catch (const error_t& e) {
        std::cout << e.what() << std::endl;
        try {
            std::cout << e.get<cppharem::key_t>() << std::endl;
            std::cout << e.get<cppharem::collection_t>() << std::endl;
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
