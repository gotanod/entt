#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <gtest/gtest.h>
#include <entt/entity/component.hpp>
#include <entt/entity/storage.hpp>
#include "../common/config.h"
#include "../common/throwing_allocator.hpp"
#include "../common/throwing_type.hpp"
#include "../common/tracked_memory_resource.hpp"

struct pointer_stable {
    static constexpr auto in_place_delete = true;
    int value{};
};

inline bool operator==(const pointer_stable &lhs, const pointer_stable &rhs) {
    return lhs.value == rhs.value;
}

inline bool operator<(const pointer_stable &lhs, const pointer_stable &rhs) {
    return lhs.value < rhs.value;
}

struct update_from_destructor {
    update_from_destructor(entt::storage<update_from_destructor> &ref, entt::entity other)
        : storage{&ref},
          target{other} {}

    update_from_destructor(update_from_destructor &&other) noexcept
        : storage{std::exchange(other.storage, nullptr)},
          target{std::exchange(other.target, entt::null)} {}

    update_from_destructor &operator=(update_from_destructor &&other) noexcept {
        storage = std::exchange(other.storage, nullptr);
        target = std::exchange(other.target, entt::null);
        return *this;
    }

    ~update_from_destructor() {
        if(target != entt::null && storage->contains(target)) {
            storage->erase(target);
        }
    }

private:
    entt::storage<update_from_destructor> *storage{};
    entt::entity target{entt::null};
};

struct create_from_constructor {
    create_from_constructor(entt::storage<create_from_constructor> &ref, entt::entity other)
        : child{other} {
        if(child != entt::null) {
            ref.emplace(child, ref, entt::null);
        }
    }

    entt::entity child;
};

template<>
struct entt::component_traits<std::unordered_set<char>> {
    static constexpr auto in_place_delete = true;
    static constexpr auto page_size = 4u;
};

template<typename Type>
struct Storage: testing::Test {
    using type = Type;
};

template<typename Type>
using StorageDeathTest = Storage<Type>;

using StorageTypes = ::testing::Types<int, pointer_stable>;

TYPED_TEST_SUITE(Storage, StorageTypes, );
TYPED_TEST_SUITE(StorageDeathTest, StorageTypes, );

TYPED_TEST(Storage, Constructors) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    ASSERT_EQ(pool.policy(), entt::deletion_policy{traits_type::in_place_delete});
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = pool.get_allocator());
    ASSERT_EQ(pool.type(), entt::type_id<value_type>());

    pool = entt::storage<value_type>{std::allocator<value_type>{}};

    ASSERT_EQ(pool.policy(), entt::deletion_policy{traits_type::in_place_delete});
    ASSERT_NO_FATAL_FAILURE([[maybe_unused]] auto alloc = pool.get_allocator());
    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
}

TYPED_TEST(Storage, Move) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 3);

    ASSERT_TRUE(std::is_move_constructible_v<decltype(pool)>);
    ASSERT_TRUE(std::is_move_assignable_v<decltype(pool)>);

    entt::storage<value_type> other{std::move(pool)};

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());

    ASSERT_EQ(pool.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{3});

    ASSERT_EQ(other.get(entt::entity{3}), value_type{3});

    entt::storage<value_type> extended{std::move(other), std::allocator<value_type>{}};

    ASSERT_TRUE(other.empty());
    ASSERT_FALSE(extended.empty());

    ASSERT_EQ(other.type(), entt::type_id<value_type>());
    ASSERT_EQ(extended.type(), entt::type_id<value_type>());

    ASSERT_EQ(other.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(extended.at(0u), entt::entity{3});

    ASSERT_EQ(extended.get(entt::entity{3}), value_type{3});

    pool = std::move(extended);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_TRUE(extended.empty());

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());
    ASSERT_EQ(extended.type(), entt::type_id<value_type>());

    ASSERT_EQ(pool.at(0u), entt::entity{3});
    ASSERT_EQ(other.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(extended.at(0u), static_cast<entt::entity>(entt::null));

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{3});

    other = entt::storage<value_type>{};
    other.emplace(entt::entity{42}, 42);
    other = std::move(pool);

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());

    ASSERT_EQ(pool.at(0u), static_cast<entt::entity>(entt::null));
    ASSERT_EQ(other.at(0u), entt::entity{3});

    ASSERT_EQ(other.get(entt::entity{3}), value_type{3});
}

TYPED_TEST(Storage, Swap) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;
    entt::storage<value_type> other;

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());

    pool.emplace(entt::entity{42}, 41);

    other.emplace(entt::entity{9}, 8);
    other.emplace(entt::entity{3}, 2);
    other.erase(entt::entity{9});

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_EQ(other.size(), 1u + traits_type::in_place_delete);

    pool.swap(other);

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(other.type(), entt::type_id<value_type>());

    ASSERT_EQ(pool.size(), 1u + traits_type::in_place_delete);
    ASSERT_EQ(other.size(), 1u);

    ASSERT_EQ(pool.at(traits_type::in_place_delete), entt::entity{3});
    ASSERT_EQ(other.at(0u), entt::entity{42});

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{2});
    ASSERT_EQ(other.get(entt::entity{42}), value_type{41});
}

TYPED_TEST(Storage, Capacity) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    pool.reserve(42);

    ASSERT_EQ(pool.capacity(), traits_type::page_size);
    ASSERT_TRUE(pool.empty());

    pool.reserve(0);

    ASSERT_EQ(pool.capacity(), traits_type::page_size);
    ASSERT_TRUE(pool.empty());
}

TYPED_TEST(Storage, ShrinkToFit) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    for(std::size_t next{}; next < traits_type::page_size; ++next) {
        pool.emplace(entt::entity(next));
    }

    pool.emplace(entt::entity{traits_type::page_size});
    pool.erase(entt::entity{traits_type::page_size});
    pool.compact();

    ASSERT_EQ(pool.capacity(), 2 * traits_type::page_size);
    ASSERT_EQ(pool.size(), traits_type::page_size);

    pool.shrink_to_fit();

    ASSERT_EQ(pool.capacity(), traits_type::page_size);
    ASSERT_EQ(pool.size(), traits_type::page_size);

    pool.clear();

    ASSERT_EQ(pool.capacity(), traits_type::page_size);
    ASSERT_EQ(pool.size(), 0u);

    pool.shrink_to_fit();

    ASSERT_EQ(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 0u);
}

TYPED_TEST(Storage, Raw) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 3);
    pool.emplace(entt::entity{12}, 6);
    pool.emplace(entt::entity{42}, 9);

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{3});
    ASSERT_EQ(std::as_const(pool).get(entt::entity{12}), value_type{6});
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{9});

    ASSERT_EQ(pool.raw()[0u][0u], value_type{3});
    ASSERT_EQ(std::as_const(pool).raw()[0u][1u], value_type{6});
    ASSERT_EQ(pool.raw()[0u][2u], value_type{9});
}

TYPED_TEST(Storage, Iterator) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, value_type>();
    testing::StaticAssertTypeEq<typename iterator::pointer, value_type *>();
    testing::StaticAssertTypeEq<typename iterator::reference, value_type &>();

    entt::storage<value_type> pool;
    pool.emplace(entt::entity{3}, 42);

    iterator end{pool.begin()};
    iterator begin{};

    begin = pool.end();
    std::swap(begin, end);

    ASSERT_EQ(begin, pool.begin());
    ASSERT_EQ(end, pool.end());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.index(), 0);
    ASSERT_EQ(end.index(), -1);

    ASSERT_EQ(begin++, pool.begin());
    ASSERT_EQ(begin--, pool.end());

    ASSERT_EQ(begin + 1, pool.end());
    ASSERT_EQ(end - 1, pool.begin());

    ASSERT_EQ(++begin, pool.end());
    ASSERT_EQ(--begin, pool.begin());

    ASSERT_EQ(begin += 1, pool.end());
    ASSERT_EQ(begin -= 1, pool.begin());

    ASSERT_EQ(begin + (end - begin), pool.end());
    ASSERT_EQ(begin - (begin - end), pool.end());

    ASSERT_EQ(end - (end - begin), pool.begin());
    ASSERT_EQ(end + (begin - end), pool.begin());

    ASSERT_EQ(begin[0u], *pool.begin().operator->());

    ASSERT_LT(begin, end);
    ASSERT_LE(begin, pool.begin());

    ASSERT_GT(end, begin);
    ASSERT_GE(end, pool.end());

    ASSERT_EQ(begin.index(), 0);
    ASSERT_EQ(end.index(), -1);

    pool.emplace(entt::entity{42}, 3);
    begin = pool.begin();

    ASSERT_EQ(begin.index(), 1);
    ASSERT_EQ(end.index(), -1);

    ASSERT_EQ(begin[0u], value_type{3});
    ASSERT_EQ(begin[1u], value_type{42});
}

TYPED_TEST(Storage, ConstIterator) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::const_iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, value_type>();
    testing::StaticAssertTypeEq<typename iterator::pointer, const value_type *>();
    testing::StaticAssertTypeEq<typename iterator::reference, const value_type &>();

    entt::storage<value_type> pool;
    pool.emplace(entt::entity{3}, 42);

    iterator cend{pool.cbegin()};
    iterator cbegin{};
    cbegin = pool.cend();
    std::swap(cbegin, cend);

    ASSERT_EQ(cbegin, std::as_const(pool).begin());
    ASSERT_EQ(cend, std::as_const(pool).end());
    ASSERT_EQ(cbegin, pool.cbegin());
    ASSERT_EQ(cend, pool.cend());
    ASSERT_NE(cbegin, cend);

    ASSERT_EQ(cbegin.index(), 0);
    ASSERT_EQ(cend.index(), -1);

    ASSERT_EQ(cbegin++, pool.cbegin());
    ASSERT_EQ(cbegin--, pool.cend());

    ASSERT_EQ(cbegin + 1, pool.cend());
    ASSERT_EQ(cend - 1, pool.cbegin());

    ASSERT_EQ(++cbegin, pool.cend());
    ASSERT_EQ(--cbegin, pool.cbegin());

    ASSERT_EQ(cbegin += 1, pool.cend());
    ASSERT_EQ(cbegin -= 1, pool.cbegin());

    ASSERT_EQ(cbegin + (cend - cbegin), pool.cend());
    ASSERT_EQ(cbegin - (cbegin - cend), pool.cend());

    ASSERT_EQ(cend - (cend - cbegin), pool.cbegin());
    ASSERT_EQ(cend + (cbegin - cend), pool.cbegin());

    ASSERT_EQ(cbegin[0u], *pool.cbegin().operator->());

    ASSERT_LT(cbegin, cend);
    ASSERT_LE(cbegin, pool.cbegin());

    ASSERT_GT(cend, cbegin);
    ASSERT_GE(cend, pool.cend());

    ASSERT_EQ(cbegin.index(), 0);
    ASSERT_EQ(cend.index(), -1);

    pool.emplace(entt::entity{42}, 3);
    cbegin = pool.cbegin();

    ASSERT_EQ(cbegin.index(), 1);
    ASSERT_EQ(cend.index(), -1);

    ASSERT_EQ(cbegin[0u], value_type{3});
    ASSERT_EQ(cbegin[1u], value_type{42});
}

TYPED_TEST(Storage, ReverseIterator) {
    using value_type = typename TestFixture::type;
    using reverse_iterator = typename entt::storage<value_type>::reverse_iterator;

    testing::StaticAssertTypeEq<typename reverse_iterator::value_type, value_type>();
    testing::StaticAssertTypeEq<typename reverse_iterator::pointer, value_type *>();
    testing::StaticAssertTypeEq<typename reverse_iterator::reference, value_type &>();

    entt::storage<value_type> pool;
    pool.emplace(entt::entity{3}, 42);

    reverse_iterator end{pool.rbegin()};
    reverse_iterator begin{};
    begin = pool.rend();
    std::swap(begin, end);

    ASSERT_EQ(begin, pool.rbegin());
    ASSERT_EQ(end, pool.rend());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 0);

    ASSERT_EQ(begin++, pool.rbegin());
    ASSERT_EQ(begin--, pool.rend());

    ASSERT_EQ(begin + 1, pool.rend());
    ASSERT_EQ(end - 1, pool.rbegin());

    ASSERT_EQ(++begin, pool.rend());
    ASSERT_EQ(--begin, pool.rbegin());

    ASSERT_EQ(begin += 1, pool.rend());
    ASSERT_EQ(begin -= 1, pool.rbegin());

    ASSERT_EQ(begin + (end - begin), pool.rend());
    ASSERT_EQ(begin - (begin - end), pool.rend());

    ASSERT_EQ(end - (end - begin), pool.rbegin());
    ASSERT_EQ(end + (begin - end), pool.rbegin());

    ASSERT_EQ(begin[0u], *pool.rbegin().operator->());

    ASSERT_LT(begin, end);
    ASSERT_LE(begin, pool.rbegin());

    ASSERT_GT(end, begin);
    ASSERT_GE(end, pool.rend());

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 0);

    pool.emplace(entt::entity{42}, 3);
    end = pool.rend();

    ASSERT_EQ(begin.base().index(), -1);
    ASSERT_EQ(end.base().index(), 1);

    ASSERT_EQ(begin[0u], value_type{42});
    ASSERT_EQ(begin[1u], value_type{3});
}

TYPED_TEST(Storage, ConstReverseIterator) {
    using value_type = typename TestFixture::type;
    using const_reverse_iterator = typename entt::storage<value_type>::const_reverse_iterator;

    testing::StaticAssertTypeEq<typename const_reverse_iterator::value_type, value_type>();
    testing::StaticAssertTypeEq<typename const_reverse_iterator::pointer, const value_type *>();
    testing::StaticAssertTypeEq<typename const_reverse_iterator::reference, const value_type &>();

    entt::storage<value_type> pool;
    pool.emplace(entt::entity{3}, 42);

    const_reverse_iterator cend{pool.crbegin()};
    const_reverse_iterator cbegin{};
    cbegin = pool.crend();
    std::swap(cbegin, cend);

    ASSERT_EQ(cbegin, std::as_const(pool).rbegin());
    ASSERT_EQ(cend, std::as_const(pool).rend());
    ASSERT_EQ(cbegin, pool.crbegin());
    ASSERT_EQ(cend, pool.crend());
    ASSERT_NE(cbegin, cend);

    ASSERT_EQ(cbegin.base().index(), -1);
    ASSERT_EQ(cend.base().index(), 0);

    ASSERT_EQ(cbegin++, pool.crbegin());
    ASSERT_EQ(cbegin--, pool.crend());

    ASSERT_EQ(cbegin + 1, pool.crend());
    ASSERT_EQ(cend - 1, pool.crbegin());

    ASSERT_EQ(++cbegin, pool.crend());
    ASSERT_EQ(--cbegin, pool.crbegin());

    ASSERT_EQ(cbegin += 1, pool.crend());
    ASSERT_EQ(cbegin -= 1, pool.crbegin());

    ASSERT_EQ(cbegin + (cend - cbegin), pool.crend());
    ASSERT_EQ(cbegin - (cbegin - cend), pool.crend());

    ASSERT_EQ(cend - (cend - cbegin), pool.crbegin());
    ASSERT_EQ(cend + (cbegin - cend), pool.crbegin());

    ASSERT_EQ(cbegin[0u], *pool.crbegin().operator->());

    ASSERT_LT(cbegin, cend);
    ASSERT_LE(cbegin, pool.crbegin());

    ASSERT_GT(cend, cbegin);
    ASSERT_GE(cend, pool.crend());

    ASSERT_EQ(cbegin.base().index(), -1);
    ASSERT_EQ(cend.base().index(), 0);

    pool.emplace(entt::entity{42}, 3);
    cend = pool.crend();

    ASSERT_EQ(cbegin.base().index(), -1);
    ASSERT_EQ(cend.base().index(), 1);

    ASSERT_EQ(cbegin[0u], value_type{42});
    ASSERT_EQ(cbegin[1u], value_type{3});
}

TYPED_TEST(Storage, IteratorConversion) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 42);

    typename entt::storage<value_type>::iterator it = pool.begin();
    typename entt::storage<value_type>::const_iterator cit = it;

    testing::StaticAssertTypeEq<decltype(*it), value_type &>();
    testing::StaticAssertTypeEq<decltype(*cit), const value_type &>();

    ASSERT_EQ(*it.operator->(), value_type{42});
    ASSERT_EQ(*it.operator->(), *cit);

    ASSERT_EQ(it - cit, 0);
    ASSERT_EQ(cit - it, 0);
    ASSERT_LE(it, cit);
    ASSERT_LE(cit, it);
    ASSERT_GE(it, cit);
    ASSERT_GE(cit, it);
    ASSERT_EQ(it, cit);
    ASSERT_NE(++cit, it);
}

TYPED_TEST(Storage, IteratorPageSizeAwareness) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    const value_type check{42};

    for(unsigned int next{}; next < traits_type::page_size; ++next) {
        pool.emplace(entt::entity{next});
    }

    pool.emplace(entt::entity{traits_type::page_size}, check);

    // test the proper use of component traits by the storage iterator
    ASSERT_EQ(*pool.begin(), check);
}

TYPED_TEST(Storage, Getters) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{41}, 3);

    testing::StaticAssertTypeEq<decltype(pool.get({})), value_type &>();
    testing::StaticAssertTypeEq<decltype(std::as_const(pool).get({})), const value_type &>();

    testing::StaticAssertTypeEq<decltype(pool.get_as_tuple({})), std::tuple<value_type &>>();
    testing::StaticAssertTypeEq<decltype(std::as_const(pool).get_as_tuple({})), std::tuple<const value_type &>>();

    ASSERT_EQ(pool.get(entt::entity{41}), value_type{3});
    ASSERT_EQ(std::as_const(pool).get(entt::entity{41}), value_type{3});

    ASSERT_EQ(pool.get_as_tuple(entt::entity{41}), std::make_tuple(value_type{3}));
    ASSERT_EQ(std::as_const(pool).get_as_tuple(entt::entity{41}), std::make_tuple(value_type{3}));
}

ENTT_DEBUG_TYPED_TEST(StorageDeathTest, Getters) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    ASSERT_DEATH([[maybe_unused]] const auto &value = pool.get(entt::entity{41}), "");
    ASSERT_DEATH([[maybe_unused]] const auto &value = std::as_const(pool).get(entt::entity{41}), "");

    ASSERT_DEATH([[maybe_unused]] const auto value = pool.get_as_tuple(entt::entity{41}), "");
    ASSERT_DEATH([[maybe_unused]] const auto value = std::as_const(pool).get_as_tuple(entt::entity{41}), "");
}

TYPED_TEST(Storage, Value) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{42});

    ASSERT_EQ(pool.value(entt::entity{42}), &pool.get(entt::entity{42}));
}

ENTT_DEBUG_TYPED_TEST(StorageDeathTest, Value) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    ASSERT_DEATH([[maybe_unused]] const void *value = pool.value(entt::entity{42}), "");
}

TYPED_TEST(Storage, Emplace) {
    // ensure that we've at least an aggregate type to test here
    static_assert(std::is_aggregate_v<pointer_stable>, "Not an aggregate type");

    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    testing::StaticAssertTypeEq<decltype(pool.emplace({})), value_type &>();

    // aggregate types with no args enter the non-aggregate path
    ASSERT_EQ(pool.emplace(entt::entity{3}), value_type{});
    // aggregate types with args work despite the lack of support in the standard library
    ASSERT_EQ(pool.emplace(entt::entity{42}, 42), value_type{42});
}

TEST(Storage, EmplaceSelfMoveSupport) {
    // see #37 - this test shouldn't crash, that's all
    entt::storage<std::unordered_set<int>> pool;
    entt::entity entity{};

    ASSERT_EQ(pool.policy(), entt::deletion_policy::swap_and_pop);

    pool.emplace(entity).insert(42);
    pool.erase(entity);

    ASSERT_FALSE(pool.contains(entity));
}

TEST(Storage, EmplaceSelfMoveSupportInPlaceDelete) {
    // see #37 - this test shouldn't crash, that's all
    entt::storage<std::unordered_set<char>> pool;
    entt::entity entity{};

    ASSERT_EQ(pool.policy(), entt::deletion_policy::in_place);

    pool.emplace(entity).insert(42);
    pool.erase(entity);

    ASSERT_FALSE(pool.contains(entity));
}

TYPED_TEST(Storage, TryEmplace) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};
    value_type instance{42};

    ASSERT_NE(base.push(entity[0u], &instance), base.end());

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_EQ(base.index(entity[0u]), 0u);
    ASSERT_EQ(base.value(entity[0u]), &pool.get(entity[0u]));
    ASSERT_EQ(pool.get(entity[0u]), value_type{42});

    base.erase(entity[0u]);

    ASSERT_NE(base.push(std::begin(entity), std::end(entity)), base.end());

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 3u);
        ASSERT_EQ(base.index(entity[0u]), 1u);
        ASSERT_EQ(base.index(entity[1u]), 2u);
    } else {
        ASSERT_EQ(pool.size(), 2u);
        ASSERT_EQ(base.index(entity[0u]), 0u);
        ASSERT_EQ(base.index(entity[1u]), 1u);
    }

    ASSERT_EQ(pool.get(entity[0u]), value_type{});
    ASSERT_EQ(pool.get(entity[1u]), value_type{});

    base.erase(std::begin(entity), std::end(entity));

    ASSERT_NE(base.push(std::rbegin(entity), std::rend(entity)), base.end());

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 5u);
        ASSERT_EQ(base.index(entity[0u]), 4u);
        ASSERT_EQ(base.index(entity[1u]), 3u);
    } else {
        ASSERT_EQ(pool.size(), 2u);
        ASSERT_EQ(base.index(entity[0u]), 1u);
        ASSERT_EQ(base.index(entity[1u]), 0u);
    }

    ASSERT_EQ(pool.get(entity[0u]), value_type{});
    ASSERT_EQ(pool.get(entity[1u]), value_type{});
}

TEST(Storage, TryEmplaceNonDefaultConstructible) {
    using value_type = std::pair<int &, int &>;
    static_assert(!std::is_default_constructible_v<value_type>, "Default constructible types not allowed");

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(pool.type(), base.type());

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));

    ASSERT_EQ(base.push(entity[0u]), base.end());

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(base.find(entity[0u]), base.end());
    ASSERT_TRUE(pool.empty());

    int value = 42;
    value_type instance{value, value};

    ASSERT_NE(base.push(entity[0u], &instance), base.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));

    base.erase(entity[0u]);

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(pool.contains(entity[0u]));

    ASSERT_EQ(base.push(std::begin(entity), std::end(entity)), base.end());

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(base.find(entity[0u]), base.end());
    ASSERT_EQ(base.find(entity[1u]), base.end());
    ASSERT_TRUE(pool.empty());
}

TEST(Storage, TryEmplaceNonCopyConstructible) {
    using value_type = std::unique_ptr<int>;
    static_assert(!std::is_copy_constructible_v<value_type>, "Copy constructible types not allowed");

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    ASSERT_EQ(pool.type(), entt::type_id<value_type>());
    ASSERT_EQ(pool.type(), base.type());

    ASSERT_FALSE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));

    ASSERT_NE(base.push(entity[0u]), base.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_NE(base.find(entity[0u]), base.end());
    ASSERT_FALSE(pool.empty());

    value_type instance = std::make_unique<int>(3);

    ASSERT_EQ(base.push(entity[1u], &instance), base.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));

    base.erase(entity[0u]);

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(pool.contains(entity[0u]));

    ASSERT_NE(base.push(std::begin(entity), std::end(entity)), base.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_TRUE(pool.contains(entity[1u]));
    ASSERT_NE(base.find(entity[0u]), base.end());
    ASSERT_NE(base.find(entity[1u]), base.end());
    ASSERT_FALSE(pool.empty());
}

TYPED_TEST(Storage, Patch) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    entt::entity entity{42};

    auto callback = [](auto &&elem) {
        if constexpr(std::is_class_v<std::remove_reference_t<decltype(elem)>>) {
            ++elem.value;
        } else {
            ++elem;
        }
    };

    pool.emplace(entity, 0);

    ASSERT_EQ(pool.get(entity), value_type{0});

    pool.patch(entity);
    pool.patch(entity, callback);
    pool.patch(entity, callback, callback);

    ASSERT_EQ(pool.get(entity), value_type{3});
}

ENTT_DEBUG_TYPED_TEST(StorageDeathTest, Patch) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    ASSERT_DEATH(pool.patch(entt::null), "");
}

TYPED_TEST(Storage, Insert) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};
    typename entt::storage<value_type>::iterator it{};

    it = pool.insert(std::begin(entity), std::end(entity), value_type{99});

    ASSERT_EQ(it, pool.cbegin());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_TRUE(pool.contains(entity[1u]));

    ASSERT_FALSE(pool.empty());
    ASSERT_EQ(pool.size(), 2u);
    ASSERT_EQ(pool.get(entity[0u]), value_type{99});
    ASSERT_EQ(pool.get(entity[1u]), value_type{99});
    ASSERT_EQ(*it++.operator->(), value_type{99});
    ASSERT_EQ(*it.operator->(), value_type{99});

    const value_type values[2u] = {value_type{42}, value_type{3}};

    pool.erase(std::begin(entity), std::end(entity));
    it = pool.insert(std::rbegin(entity), std::rend(entity), std::begin(values));

    ASSERT_EQ(it, pool.cbegin());

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 4u);
        ASSERT_EQ(pool.at(2u), entity[1u]);
        ASSERT_EQ(pool.at(3u), entity[0u]);
        ASSERT_EQ(pool.index(entity[0u]), 3u);
        ASSERT_EQ(pool.index(entity[1u]), 2u);
    } else {
        ASSERT_EQ(pool.size(), 2u);
        ASSERT_EQ(pool.at(0u), entity[1u]);
        ASSERT_EQ(pool.at(1u), entity[0u]);
        ASSERT_EQ(pool.index(entity[0u]), 1u);
        ASSERT_EQ(pool.index(entity[1u]), 0u);
    }

    ASSERT_EQ(pool.get(entity[0u]), value_type{3});
    ASSERT_EQ(pool.get(entity[1u]), value_type{42});
    ASSERT_EQ(*it++.operator->(), value_type{3});
    ASSERT_EQ(*it.operator->(), value_type{42});
}

TYPED_TEST(Storage, Erase) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, entt::entity{9}};
    const value_type values[3u]{value_type{0}, value_type{1}, value_type{2}};

    pool.insert(std::begin(entity), std::end(entity), std::begin(values));
    pool.erase(std::begin(entity), std::end(entity));

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 3u);
        ASSERT_TRUE(pool.at(2u) == entt::tombstone);
    } else {
        ASSERT_EQ(pool.size(), 0u);
    }

    pool.insert(std::begin(entity), std::end(entity), std::begin(values));
    pool.erase(entity, entity + 2u);

    ASSERT_EQ(*pool.begin(), values[2u]);

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 6u);
        ASSERT_EQ(pool.index(entity[2u]), 5u);
    } else {
        ASSERT_EQ(pool.size(), 1u);
    }

    pool.erase(entity[2u]);

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 6u);
        ASSERT_TRUE(pool.at(5u) == entt::tombstone);
    } else {
        ASSERT_EQ(pool.size(), 0u);
    }
}

TYPED_TEST(Storage, CrossErase) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;
    entt::sparse_set set;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    pool.emplace(entity[0u], 3);
    pool.emplace(entity[1u], 42);
    set.push(entity[1u]);
    pool.erase(set.begin(), set.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.raw()[0u][0u], value_type{3});
}

TYPED_TEST(Storage, Remove) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, entt::entity{9}};
    const value_type values[3u]{value_type{0}, value_type{1}, value_type{2}};

    pool.insert(std::begin(entity), std::end(entity), std::begin(values));

    ASSERT_EQ(pool.remove(std::begin(entity), std::end(entity)), 3u);
    ASSERT_EQ(pool.remove(std::begin(entity), std::end(entity)), 0u);

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 3u);
        ASSERT_TRUE(pool.at(2u) == entt::tombstone);
    } else {
        ASSERT_EQ(pool.size(), 0u);
    }

    pool.insert(std::begin(entity), std::end(entity), std::begin(values));

    ASSERT_EQ(pool.remove(entity, entity + 2u), 2u);
    ASSERT_EQ(*pool.begin(), values[2u]);

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 6u);
        ASSERT_EQ(pool.index(entity[2u]), 5u);
    } else {
        ASSERT_EQ(pool.size(), 1u);
    }

    ASSERT_TRUE(pool.remove(entity[2u]));
    ASSERT_FALSE(pool.remove(entity[2u]));

    if constexpr(traits_type::in_place_delete) {
        ASSERT_EQ(pool.size(), 6u);
        ASSERT_TRUE(pool.at(5u) == entt::tombstone);
    } else {
        ASSERT_EQ(pool.size(), 0u);
    }
}

TYPED_TEST(Storage, CrossRemove) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;
    entt::sparse_set set;

    entt::entity entity[2u]{entt::entity{3}, entt::entity{42}};

    pool.emplace(entity[0u], 3);
    pool.emplace(entity[1u], 42);
    set.push(entity[1u]);
    pool.remove(set.begin(), set.end());

    ASSERT_TRUE(pool.contains(entity[0u]));
    ASSERT_FALSE(pool.contains(entity[1u]));
    ASSERT_EQ(pool.raw()[0u][0u], value_type{3});
}

TYPED_TEST(Storage, Clear) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    entt::entity entity[3u]{entt::entity{3}, entt::entity{42}, entt::entity{9}};

    pool.insert(std::begin(entity), std::end(entity));

    ASSERT_EQ(pool.size(), 3u);

    pool.clear();

    ASSERT_EQ(pool.size(), 0u);

    pool.insert(std::begin(entity), std::end(entity));
    pool.erase(entity[2u]);

    ASSERT_EQ(pool.size(), 2u + traits_type::in_place_delete);

    pool.clear();

    ASSERT_EQ(pool.size(), 0u);
}

TYPED_TEST(Storage, Compact) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    ASSERT_TRUE(pool.empty());

    pool.compact();

    ASSERT_TRUE(pool.empty());

    pool.emplace(entt::entity{0}, value_type{0});
    pool.compact();

    ASSERT_EQ(pool.size(), 1u);

    pool.emplace(entt::entity{42}, value_type{42});
    pool.erase(entt::entity{0});

    ASSERT_EQ(pool.size(), 1u + traits_type::in_place_delete);
    ASSERT_EQ(pool.index(entt::entity{42}), traits_type::in_place_delete);
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{42});

    pool.compact();

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_EQ(pool.index(entt::entity{42}), 0u);
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{42});

    pool.emplace(entt::entity{0}, value_type{0});
    pool.compact();

    ASSERT_EQ(pool.size(), 2u);
    ASSERT_EQ(pool.index(entt::entity{42}), 0u);
    ASSERT_EQ(pool.index(entt::entity{0}), 1u);
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{42});
    ASSERT_EQ(pool.get(entt::entity{0}), value_type{0});

    pool.erase(entt::entity{0});
    pool.erase(entt::entity{42});
    pool.compact();

    ASSERT_TRUE(pool.empty());
}

TYPED_TEST(Storage, SwapElements) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 3);
    pool.emplace(entt::entity{12}, 6);
    pool.emplace(entt::entity{42}, 9);

    pool.erase(entt::entity{12});

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{3});
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{9});
    ASSERT_EQ(pool.index(entt::entity{3}), 0u);
    ASSERT_EQ(pool.index(entt::entity{42}), 1u + traits_type::in_place_delete);

    pool.swap_elements(entt::entity{3}, entt::entity{42});

    ASSERT_EQ(pool.get(entt::entity{3}), value_type{3});
    ASSERT_EQ(pool.get(entt::entity{42}), value_type{9});
    ASSERT_EQ(pool.index(entt::entity{3}), 1u + traits_type::in_place_delete);
    ASSERT_EQ(pool.index(entt::entity{42}), 0u);
}

TYPED_TEST(Storage, Iterable) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::iterable::iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, std::tuple<entt::entity, value_type &>>();
    testing::StaticAssertTypeEq<typename iterator::pointer, entt::input_iterator_pointer<std::tuple<entt::entity, value_type &>>>();
    testing::StaticAssertTypeEq<typename iterator::reference, typename iterator::value_type>();

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    pool.emplace(entt::entity{1}, 99);
    pool.emplace(entt::entity{3}, 42);

    auto iterable = pool.each();

    iterator end{iterable.begin()};
    iterator begin{};

    begin = iterable.end();
    std::swap(begin, end);

    ASSERT_EQ(begin, iterable.begin());
    ASSERT_EQ(end, iterable.end());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base(), base.begin());
    ASSERT_EQ(end.base(), base.end());

    ASSERT_EQ(std::get<0>(*begin.operator->().operator->()), entt::entity{3});
    ASSERT_EQ(std::get<1>(*begin.operator->().operator->()), value_type{42});
    ASSERT_EQ(std::get<0>(*begin), entt::entity{3});
    ASSERT_EQ(std::get<1>(*begin), value_type{42});

    ASSERT_EQ(begin++, iterable.begin());
    ASSERT_EQ(begin.base(), ++base.begin());
    ASSERT_EQ(++begin, iterable.end());
    ASSERT_EQ(begin.base(), base.end());

    for(auto [entity, element]: iterable) {
        testing::StaticAssertTypeEq<decltype(entity), entt::entity>();
        testing::StaticAssertTypeEq<decltype(element), value_type &>();
        ASSERT_TRUE(entity != entt::entity{1} || element == value_type{99});
        ASSERT_TRUE(entity != entt::entity{3} || element == value_type{42});
    }
}

TYPED_TEST(Storage, ConstIterable) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::const_iterable::iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, std::tuple<entt::entity, const value_type &>>();
    testing::StaticAssertTypeEq<typename iterator::pointer, entt::input_iterator_pointer<std::tuple<entt::entity, const value_type &>>>();
    testing::StaticAssertTypeEq<typename iterator::reference, typename iterator::value_type>();

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    pool.emplace(entt::entity{1}, 99);
    pool.emplace(entt::entity{3}, 42);

    auto iterable = std::as_const(pool).each();

    iterator end{iterable.cbegin()};
    iterator begin{};

    begin = iterable.cend();
    std::swap(begin, end);

    ASSERT_EQ(begin, iterable.cbegin());
    ASSERT_EQ(end, iterable.cend());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base(), base.begin());
    ASSERT_EQ(end.base(), base.end());

    ASSERT_EQ(std::get<0>(*begin.operator->().operator->()), entt::entity{3});
    ASSERT_EQ(std::get<1>(*begin.operator->().operator->()), value_type{42});
    ASSERT_EQ(std::get<0>(*begin), entt::entity{3});
    ASSERT_EQ(std::get<1>(*begin), value_type{42});

    ASSERT_EQ(begin++, iterable.begin());
    ASSERT_EQ(begin.base(), ++base.begin());
    ASSERT_EQ(++begin, iterable.end());
    ASSERT_EQ(begin.base(), base.end());

    for(auto [entity, element]: iterable) {
        testing::StaticAssertTypeEq<decltype(entity), entt::entity>();
        testing::StaticAssertTypeEq<decltype(element), const value_type &>();
        ASSERT_TRUE(entity != entt::entity{1} || element == value_type{99});
        ASSERT_TRUE(entity != entt::entity{3} || element == value_type{42});
    }
}

TYPED_TEST(Storage, IterableIteratorConversion) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 42);

    typename entt::storage<value_type>::iterable::iterator it = pool.each().begin();
    typename entt::storage<value_type>::const_iterable::iterator cit = it;

    testing::StaticAssertTypeEq<decltype(*it), std::tuple<entt::entity, value_type &>>();
    testing::StaticAssertTypeEq<decltype(*cit), std::tuple<entt::entity, const value_type &>>();

    ASSERT_EQ(it, cit);
    ASSERT_NE(++cit, it);
}

TYPED_TEST(Storage, IterableAlgorithmCompatibility) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 42);

    const auto iterable = pool.each();
    const auto it = std::find_if(iterable.begin(), iterable.end(), [](auto args) { return std::get<0>(args) == entt::entity{3}; });

    ASSERT_EQ(std::get<0>(*it), entt::entity{3});
}

TYPED_TEST(Storage, ReverseIterable) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::reverse_iterable::iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, std::tuple<entt::entity, value_type &>>();
    testing::StaticAssertTypeEq<typename iterator::pointer, entt::input_iterator_pointer<std::tuple<entt::entity, value_type &>>>();
    testing::StaticAssertTypeEq<typename iterator::reference, typename iterator::value_type>();

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    pool.emplace(entt::entity{1}, 99);
    pool.emplace(entt::entity{3}, 42);

    auto iterable = pool.reach();

    iterator end{iterable.begin()};
    iterator begin{};

    begin = iterable.end();
    std::swap(begin, end);

    ASSERT_EQ(begin, iterable.begin());
    ASSERT_EQ(end, iterable.end());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base(), base.rbegin());
    ASSERT_EQ(end.base(), base.rend());

    ASSERT_EQ(std::get<0>(*begin.operator->().operator->()), entt::entity{1});
    ASSERT_EQ(std::get<1>(*begin.operator->().operator->()), value_type{99});
    ASSERT_EQ(std::get<0>(*begin), entt::entity{1});
    ASSERT_EQ(std::get<1>(*begin), value_type{99});

    ASSERT_EQ(begin++, iterable.begin());
    ASSERT_EQ(begin.base(), ++base.rbegin());
    ASSERT_EQ(++begin, iterable.end());
    ASSERT_EQ(begin.base(), base.rend());

    for(auto [entity, element]: iterable) {
        testing::StaticAssertTypeEq<decltype(entity), entt::entity>();
        testing::StaticAssertTypeEq<decltype(element), value_type &>();
        ASSERT_TRUE(entity != entt::entity{1} || element == value_type{99});
        ASSERT_TRUE(entity != entt::entity{3} || element == value_type{42});
    }
}

TYPED_TEST(Storage, ConstReverseIterable) {
    using value_type = typename TestFixture::type;
    using iterator = typename entt::storage<value_type>::const_reverse_iterable::iterator;

    testing::StaticAssertTypeEq<typename iterator::value_type, std::tuple<entt::entity, const value_type &>>();
    testing::StaticAssertTypeEq<typename iterator::pointer, entt::input_iterator_pointer<std::tuple<entt::entity, const value_type &>>>();
    testing::StaticAssertTypeEq<typename iterator::reference, typename iterator::value_type>();

    entt::storage<value_type> pool;
    entt::sparse_set &base = pool;

    pool.emplace(entt::entity{1}, 99);
    pool.emplace(entt::entity{3}, 42);

    auto iterable = std::as_const(pool).reach();

    iterator end{iterable.cbegin()};
    iterator begin{};

    begin = iterable.cend();
    std::swap(begin, end);

    ASSERT_EQ(begin, iterable.cbegin());
    ASSERT_EQ(end, iterable.cend());
    ASSERT_NE(begin, end);

    ASSERT_EQ(begin.base(), base.rbegin());
    ASSERT_EQ(end.base(), base.rend());

    ASSERT_EQ(std::get<0>(*begin.operator->().operator->()), entt::entity{1});
    ASSERT_EQ(std::get<1>(*begin.operator->().operator->()), value_type{99});
    ASSERT_EQ(std::get<0>(*begin), entt::entity{1});
    ASSERT_EQ(std::get<1>(*begin), value_type{99});

    ASSERT_EQ(begin++, iterable.begin());
    ASSERT_EQ(begin.base(), ++base.rbegin());
    ASSERT_EQ(++begin, iterable.end());
    ASSERT_EQ(begin.base(), base.rend());

    for(auto [entity, element]: iterable) {
        testing::StaticAssertTypeEq<decltype(entity), entt::entity>();
        testing::StaticAssertTypeEq<decltype(element), const value_type &>();
        ASSERT_TRUE(entity != entt::entity{1} || element == value_type{99});
        ASSERT_TRUE(entity != entt::entity{3} || element == value_type{42});
    }
}

TYPED_TEST(Storage, ReverseIterableIteratorConversion) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 42);

    typename entt::storage<value_type>::reverse_iterable::iterator it = pool.reach().begin();
    typename entt::storage<value_type>::const_reverse_iterable::iterator cit = it;

    testing::StaticAssertTypeEq<decltype(*it), std::tuple<entt::entity, value_type &>>();
    testing::StaticAssertTypeEq<decltype(*cit), std::tuple<entt::entity, const value_type &>>();

    ASSERT_EQ(it, cit);
    ASSERT_NE(++cit, it);
}

TYPED_TEST(Storage, ReverseIterableAlgorithmCompatibility) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{3}, 42);

    const auto iterable = pool.reach();
    const auto it = std::find_if(iterable.begin(), iterable.end(), [](auto args) { return std::get<0>(args) == entt::entity{3}; });

    ASSERT_EQ(std::get<0>(*it), entt::entity{3});
}

TYPED_TEST(Storage, SortOrdered) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    entt::entity entity[5u]{entt::entity{12}, entt::entity{42}, entt::entity{7}, entt::entity{3}, entt::entity{9}};
    value_type values[5u]{value_type{12}, value_type{9}, value_type{6}, value_type{3}, value_type{1}};

    pool.insert(std::begin(entity), std::end(entity), values);
    pool.sort([&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    ASSERT_TRUE(std::equal(std::rbegin(entity), std::rend(entity), pool.entt::sparse_set::begin(), pool.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(values), std::rend(values), pool.begin(), pool.end()));
}

TYPED_TEST(Storage, SortReverse) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    entt::entity entity[5u]{entt::entity{12}, entt::entity{42}, entt::entity{7}, entt::entity{3}, entt::entity{9}};
    value_type values[5u]{value_type{1}, value_type{3}, value_type{6}, value_type{9}, value_type{12}};

    pool.insert(std::begin(entity), std::end(entity), values);
    pool.sort([&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    ASSERT_TRUE(std::equal(std::begin(entity), std::end(entity), pool.entt::sparse_set::begin(), pool.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::begin(values), std::end(values), pool.begin(), pool.end()));
}

TYPED_TEST(Storage, SortUnordered) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    entt::entity entity[5u]{entt::entity{12}, entt::entity{42}, entt::entity{7}, entt::entity{3}, entt::entity{9}};
    value_type values[5u]{value_type{6}, value_type{3}, value_type{1}, value_type{9}, value_type{12}};

    pool.insert(std::begin(entity), std::end(entity), values);
    pool.sort([&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    auto begin = pool.begin();
    auto end = pool.end();

    ASSERT_EQ(*(begin++), values[2u]);
    ASSERT_EQ(*(begin++), values[1u]);
    ASSERT_EQ(*(begin++), values[0u]);
    ASSERT_EQ(*(begin++), values[3u]);
    ASSERT_EQ(*(begin++), values[4u]);
    ASSERT_EQ(begin, end);

    ASSERT_EQ(pool.data()[0u], entity[4u]);
    ASSERT_EQ(pool.data()[1u], entity[3u]);
    ASSERT_EQ(pool.data()[2u], entity[0u]);
    ASSERT_EQ(pool.data()[3u], entity[1u]);
    ASSERT_EQ(pool.data()[4u], entity[2u]);
}

TYPED_TEST(Storage, SortRange) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    entt::entity entity[5u]{entt::entity{12}, entt::entity{42}, entt::entity{7}, entt::entity{3}, entt::entity{9}};
    value_type values[5u]{value_type{3}, value_type{6}, value_type{1}, value_type{9}, value_type{12}};

    pool.insert(std::begin(entity), std::end(entity), values);
    pool.sort_n(0u, [&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    ASSERT_TRUE(std::equal(std::rbegin(entity), std::rend(entity), pool.entt::sparse_set::begin(), pool.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(values), std::rend(values), pool.begin(), pool.end()));

    pool.sort_n(2u, [&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    ASSERT_EQ(pool.raw()[0u][0u], values[1u]);
    ASSERT_EQ(pool.raw()[0u][1u], values[0u]);
    ASSERT_EQ(pool.raw()[0u][2u], values[2u]);

    ASSERT_EQ(pool.data()[0u], entity[1u]);
    ASSERT_EQ(pool.data()[1u], entity[0u]);
    ASSERT_EQ(pool.data()[2u], entity[2u]);

    pool.sort_n(5u, [&pool](auto lhs, auto rhs) { return pool.get(lhs) < pool.get(rhs); });

    auto begin = pool.begin();
    auto end = pool.end();

    ASSERT_EQ(*(begin++), values[2u]);
    ASSERT_EQ(*(begin++), values[0u]);
    ASSERT_EQ(*(begin++), values[1u]);
    ASSERT_EQ(*(begin++), values[3u]);
    ASSERT_EQ(*(begin++), values[4u]);
    ASSERT_EQ(begin, end);

    ASSERT_EQ(pool.data()[0u], entity[4u]);
    ASSERT_EQ(pool.data()[1u], entity[3u]);
    ASSERT_EQ(pool.data()[2u], entity[1u]);
    ASSERT_EQ(pool.data()[3u], entity[0u]);
    ASSERT_EQ(pool.data()[4u], entity[2u]);
}

TYPED_TEST(Storage, RespectDisjoint) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> lhs;
    entt::storage<value_type> rhs;

    entt::entity lhs_entity[3u]{entt::entity{3}, entt::entity{12}, entt::entity{42}};
    value_type lhs_values[3u]{value_type{3}, value_type{6}, value_type{9}};

    lhs.insert(std::begin(lhs_entity), std::end(lhs_entity), lhs_values);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));

    lhs.sort_as(rhs);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));
}

TYPED_TEST(Storage, RespectOverlap) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> lhs;
    entt::storage<value_type> rhs;

    entt::entity lhs_entity[3u]{entt::entity{3}, entt::entity{12}, entt::entity{42}};
    value_type lhs_values[3u]{value_type{3}, value_type{6}, value_type{9}};

    lhs.insert(std::begin(lhs_entity), std::end(lhs_entity), lhs_values);

    entt::entity rhs_entity[1u]{entt::entity{12}};
    value_type rhs_values[1u]{value_type{6}};

    rhs.insert(std::begin(rhs_entity), std::end(rhs_entity), rhs_values);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.entt::sparse_set::begin(), rhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_values), std::rend(rhs_values), rhs.begin(), rhs.end()));

    lhs.sort_as(rhs);

    auto begin = lhs.begin();
    auto end = lhs.end();

    ASSERT_EQ(*(begin++), lhs_values[1u]);
    ASSERT_EQ(*(begin++), lhs_values[2u]);
    ASSERT_EQ(*(begin++), lhs_values[0u]);
    ASSERT_EQ(begin, end);

    ASSERT_EQ(lhs.data()[0u], lhs_entity[0u]);
    ASSERT_EQ(lhs.data()[1u], lhs_entity[2u]);
    ASSERT_EQ(lhs.data()[2u], lhs_entity[1u]);
}

TYPED_TEST(Storage, RespectOrdered) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> lhs;
    entt::storage<value_type> rhs;

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    value_type lhs_values[5u]{value_type{1}, value_type{2}, value_type{3}, value_type{4}, value_type{5}};

    lhs.insert(std::begin(lhs_entity), std::end(lhs_entity), lhs_values);

    entt::entity rhs_entity[6u]{entt::entity{6}, entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    value_type rhs_values[6u]{value_type{6}, value_type{1}, value_type{2}, value_type{3}, value_type{4}, value_type{5}};

    rhs.insert(std::begin(rhs_entity), std::end(rhs_entity), rhs_values);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.entt::sparse_set::begin(), rhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_values), std::rend(rhs_values), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.entt::sparse_set::begin(), rhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_values), std::rend(rhs_values), rhs.begin(), rhs.end()));
}

TYPED_TEST(Storage, RespectReverse) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> lhs;
    entt::storage<value_type> rhs;

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    value_type lhs_values[5u]{value_type{1}, value_type{2}, value_type{3}, value_type{4}, value_type{5}};

    lhs.insert(std::begin(lhs_entity), std::end(lhs_entity), lhs_values);

    entt::entity rhs_entity[6u]{entt::entity{5}, entt::entity{4}, entt::entity{3}, entt::entity{2}, entt::entity{1}, entt::entity{6}};
    value_type rhs_values[6u]{value_type{5}, value_type{4}, value_type{3}, value_type{2}, value_type{1}, value_type{6}};

    rhs.insert(std::begin(rhs_entity), std::end(rhs_entity), rhs_values);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.entt::sparse_set::begin(), rhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_values), std::rend(rhs_values), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    auto begin = rhs.begin();
    auto end = rhs.end();

    ASSERT_EQ(*(begin++), rhs_values[0u]);
    ASSERT_EQ(*(begin++), rhs_values[1u]);
    ASSERT_EQ(*(begin++), rhs_values[2u]);
    ASSERT_EQ(*(begin++), rhs_values[3u]);
    ASSERT_EQ(*(begin++), rhs_values[4u]);
    ASSERT_EQ(*(begin++), rhs_values[5u]);
    ASSERT_EQ(begin, end);

    ASSERT_EQ(rhs.data()[0u], rhs_entity[5u]);
    ASSERT_EQ(rhs.data()[1u], rhs_entity[4u]);
    ASSERT_EQ(rhs.data()[2u], rhs_entity[3u]);
    ASSERT_EQ(rhs.data()[3u], rhs_entity[2u]);
    ASSERT_EQ(rhs.data()[4u], rhs_entity[1u]);
    ASSERT_EQ(rhs.data()[5u], rhs_entity[0u]);
}

TYPED_TEST(Storage, RespectUnordered) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> lhs;
    entt::storage<value_type> rhs;

    entt::entity lhs_entity[5u]{entt::entity{1}, entt::entity{2}, entt::entity{3}, entt::entity{4}, entt::entity{5}};
    value_type lhs_values[5u]{value_type{1}, value_type{2}, value_type{3}, value_type{4}, value_type{5}};

    lhs.insert(std::begin(lhs_entity), std::end(lhs_entity), lhs_values);

    entt::entity rhs_entity[6u]{entt::entity{3}, entt::entity{2}, entt::entity{6}, entt::entity{1}, entt::entity{4}, entt::entity{5}};
    value_type rhs_values[6u]{value_type{3}, value_type{2}, value_type{6}, value_type{1}, value_type{4}, value_type{5}};

    rhs.insert(std::begin(rhs_entity), std::end(rhs_entity), rhs_values);

    ASSERT_TRUE(std::equal(std::rbegin(lhs_entity), std::rend(lhs_entity), lhs.entt::sparse_set::begin(), lhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(lhs_values), std::rend(lhs_values), lhs.begin(), lhs.end()));

    ASSERT_TRUE(std::equal(std::rbegin(rhs_entity), std::rend(rhs_entity), rhs.entt::sparse_set::begin(), rhs.entt::sparse_set::end()));
    ASSERT_TRUE(std::equal(std::rbegin(rhs_values), std::rend(rhs_values), rhs.begin(), rhs.end()));

    rhs.sort_as(lhs);

    auto begin = rhs.begin();
    auto end = rhs.end();

    ASSERT_EQ(*(begin++), rhs_values[5u]);
    ASSERT_EQ(*(begin++), rhs_values[4u]);
    ASSERT_EQ(*(begin++), rhs_values[0u]);
    ASSERT_EQ(*(begin++), rhs_values[1u]);
    ASSERT_EQ(*(begin++), rhs_values[3u]);
    ASSERT_EQ(*(begin++), rhs_values[2u]);
    ASSERT_EQ(begin, end);

    ASSERT_EQ(rhs.data()[0u], rhs_entity[2u]);
    ASSERT_EQ(rhs.data()[1u], rhs_entity[3u]);
    ASSERT_EQ(rhs.data()[2u], rhs_entity[1u]);
    ASSERT_EQ(rhs.data()[3u], rhs_entity[0u]);
    ASSERT_EQ(rhs.data()[4u], rhs_entity[4u]);
    ASSERT_EQ(rhs.data()[5u], rhs_entity[5u]);
}

TEST(Storage, MoveOnlyComponent) {
    using value_type = std::unique_ptr<int>;
    static_assert(!std::is_copy_assignable_v<value_type>, "Copy assignable types not allowed");
    static_assert(std::is_move_assignable_v<value_type>, "Move assignable type required");
    // the purpose is to ensure that move only components are always accepted
    [[maybe_unused]] entt::storage<value_type> pool;
}

TEST(Storage, NonMovableComponent) {
    using value_type = std::pair<const int, const int>;
    static_assert(!std::is_move_assignable_v<value_type>, "Move assignable types not allowed");
    // the purpose is to ensure that non-movable components are always accepted
    [[maybe_unused]] entt::storage<value_type> pool;
}

ENTT_DEBUG_TEST(StorageDeathTest, NonMovableComponent) {
    entt::storage<std::pair<const int, const int>> pool;
    const entt::entity entity{0};
    const entt::entity destroy{1};
    const entt::entity other{2};

    pool.emplace(entity);
    pool.emplace(destroy);
    pool.emplace(other);

    pool.erase(destroy);

    ASSERT_DEATH(pool.swap_elements(entity, other), "");
    ASSERT_DEATH(pool.compact(), "");
    ASSERT_DEATH(pool.sort([](auto &&lhs, auto &&rhs) { return lhs < rhs; }), "");
}

TYPED_TEST(Storage, CanModifyDuringIteration) {
    using value_type = typename TestFixture::type;
    using traits_type = entt::component_traits<value_type>;
    entt::storage<value_type> pool;

    auto *ptr = &pool.emplace(entt::entity{0}, 42);

    ASSERT_EQ(pool.capacity(), traits_type::page_size);

    const auto it = pool.cbegin();
    pool.reserve(traits_type::page_size + 1u);

    ASSERT_EQ(pool.capacity(), 2 * traits_type::page_size);
    ASSERT_EQ(&pool.get(entt::entity{0}), ptr);

    // this should crash with asan enabled if we break the constraint
    [[maybe_unused]] const auto &value = *it;
}

TYPED_TEST(Storage, ReferencesGuaranteed) {
    using value_type = typename TestFixture::type;
    entt::storage<value_type> pool;

    pool.emplace(entt::entity{0}, 0);
    pool.emplace(entt::entity{1}, 1);

    ASSERT_EQ(pool.get(entt::entity{0}), value_type{0});
    ASSERT_EQ(pool.get(entt::entity{1}), value_type{1});

    for(auto &&type: pool) {
        if(!(type == value_type{})) {
            type = value_type{42};
        }
    }

    ASSERT_EQ(pool.get(entt::entity{0}), value_type{0});
    ASSERT_EQ(pool.get(entt::entity{1}), value_type{42});

    auto begin = pool.begin();

    while(begin != pool.end()) {
        *(begin++) = value_type{3};
    }

    ASSERT_EQ(pool.get(entt::entity{0}), value_type{3});
    ASSERT_EQ(pool.get(entt::entity{1}), value_type{3});
}

TEST(Storage, UpdateFromDestructor) {
    auto test = [](const auto target) {
        constexpr auto size = 10u;

        entt::storage<update_from_destructor> pool;

        for(std::size_t next{}; next < size; ++next) {
            const auto entity = entt::entity(next);
            pool.emplace(entity, pool, entity == entt::entity(size / 2) ? target : entity);
        }

        pool.erase(entt::entity(size / 2));

        ASSERT_EQ(pool.size(), size - 1u - (target != entt::null));
        ASSERT_FALSE(pool.contains(entt::entity(size / 2)));
        ASSERT_FALSE(pool.contains(target));

        pool.clear();

        ASSERT_TRUE(pool.empty());

        for(std::size_t next{}; next < size; ++next) {
            ASSERT_FALSE(pool.contains(entt::entity(next)));
        }
    };

    test(entt::entity{9u});
    test(entt::entity{8u});
    test(entt::entity{0u});
}

TEST(Storage, CreateFromConstructor) {
    entt::storage<create_from_constructor> pool;
    const entt::entity entity{0u};
    const entt::entity other{1u};

    pool.emplace(entity, pool, other);

    ASSERT_EQ(pool.get(entity).child, other);
    ASSERT_EQ(pool.get(other).child, static_cast<entt::entity>(entt::null));
}

TYPED_TEST(Storage, CustomAllocator) {
    using value_type = typename TestFixture::type;
    test::throwing_allocator<entt::entity> allocator{};
    entt::basic_storage<value_type, entt::entity, test::throwing_allocator<value_type>> pool{allocator};

    pool.reserve(1u);

    ASSERT_NE(pool.capacity(), 0u);

    pool.emplace(entt::entity{0});
    pool.emplace(entt::entity{1});

    decltype(pool) other{std::move(pool), allocator};

    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(other.empty());
    ASSERT_EQ(pool.capacity(), 0u);
    ASSERT_NE(other.capacity(), 0u);
    ASSERT_EQ(other.size(), 2u);

    pool = std::move(other);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 2u);

    pool.swap(other);
    pool = std::move(other);

    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(other.empty());
    ASSERT_EQ(other.capacity(), 0u);
    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 2u);

    pool.clear();

    ASSERT_NE(pool.capacity(), 0u);
    ASSERT_EQ(pool.size(), 0u);
}

TYPED_TEST(Storage, ThrowingAllocator) {
    using value_type = typename TestFixture::type;
    using allocator_type = test::throwing_allocator<value_type>;
    entt::basic_storage<value_type, entt::entity, allocator_type> pool{};
    typename std::decay_t<decltype(pool)>::base_type &base = pool;

    constexpr auto packed_page_size = decltype(pool)::traits_type::page_size;
    constexpr auto sparse_page_size = std::remove_reference_t<decltype(base)>::traits_type::page_size;

    allocator_type::trigger_on_allocate = true;

    ASSERT_THROW(pool.reserve(1u), typename allocator_type::exception_type);
    ASSERT_EQ(pool.capacity(), 0u);

    allocator_type::trigger_after_allocate = true;

    ASSERT_THROW(pool.reserve(2 * packed_page_size), typename allocator_type::exception_type);
    ASSERT_EQ(pool.capacity(), packed_page_size);

    pool.shrink_to_fit();

    ASSERT_EQ(pool.capacity(), 0u);

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(pool.emplace(entt::entity{0}, 0), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_FALSE(pool.contains(entt::entity{0}));
    ASSERT_TRUE(pool.empty());

    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;

    ASSERT_THROW(base.push(entt::entity{0}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_FALSE(base.contains(entt::entity{0}));
    ASSERT_TRUE(base.empty());

    allocator_type::trigger_on_allocate = true;

    ASSERT_THROW(pool.emplace(entt::entity{0}, 0), typename allocator_type::exception_type);
    ASSERT_FALSE(pool.contains(entt::entity{0}));
    ASSERT_NO_FATAL_FAILURE(pool.compact());
    ASSERT_TRUE(pool.empty());

    pool.emplace(entt::entity{0}, 0);
    const entt::entity entity[2u]{entt::entity{1}, entt::entity{sparse_page_size}};
    test::throwing_allocator<entt::entity>::trigger_after_allocate = true;

    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), value_type{0}), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_FALSE(pool.contains(entt::entity{sparse_page_size}));

    pool.erase(entt::entity{1});
    const value_type components[2u]{value_type{1}, value_type{sparse_page_size}};
    test::throwing_allocator<entt::entity>::trigger_on_allocate = true;
    pool.compact();

    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), std::begin(components)), test::throwing_allocator<entt::entity>::exception_type);
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_FALSE(pool.contains(entt::entity{sparse_page_size}));
}

TEST(Storage, ThrowingComponent) {
    entt::storage<test::throwing_type> pool;
    test::throwing_type::trigger_on_value = 42;

    // strong exception safety
    ASSERT_THROW(pool.emplace(entt::entity{0}, test::throwing_type{42}), typename test::throwing_type::exception_type);
    ASSERT_TRUE(pool.empty());

    const entt::entity entity[2u]{entt::entity{42}, entt::entity{1}};
    const test::throwing_type components[2u]{42, 1};

    // basic exception safety
    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), test::throwing_type{42}), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_FALSE(pool.contains(entt::entity{1}));

    // basic exception safety
    ASSERT_THROW(pool.insert(std::begin(entity), std::end(entity), std::begin(components)), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_FALSE(pool.contains(entt::entity{1}));

    // basic exception safety
    ASSERT_THROW(pool.insert(std::rbegin(entity), std::rend(entity), std::rbegin(components)), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 1u);
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_EQ(pool.get(entt::entity{1}), 1);

    pool.clear();
    pool.emplace(entt::entity{1}, 1);
    pool.emplace(entt::entity{42}, 42);

    // basic exception safety
    ASSERT_THROW(pool.erase(entt::entity{1}), typename test::throwing_type::exception_type);
    ASSERT_EQ(pool.size(), 2u);
    ASSERT_TRUE(pool.contains(entt::entity{42}));
    ASSERT_TRUE(pool.contains(entt::entity{1}));
    ASSERT_EQ(pool.at(0u), entt::entity{1});
    ASSERT_EQ(pool.at(1u), entt::entity{42});
    ASSERT_EQ(pool.get(entt::entity{42}), 42);
    // the element may have been moved but it's still there
    ASSERT_EQ(pool.get(entt::entity{1}), test::throwing_type::moved_from_value);

    test::throwing_type::trigger_on_value = 99;
    pool.erase(entt::entity{1});

    ASSERT_EQ(pool.size(), 1u);
    ASSERT_TRUE(pool.contains(entt::entity{42}));
    ASSERT_FALSE(pool.contains(entt::entity{1}));
    ASSERT_EQ(pool.at(0u), entt::entity{42});
    ASSERT_EQ(pool.get(entt::entity{42}), 42);
}

#if defined(ENTT_HAS_TRACKED_MEMORY_RESOURCE)

TYPED_TEST(Storage, NoUsesAllocatorConstruction) {
    using value_type = typename TestFixture::type;
    test::tracked_memory_resource memory_resource{};
    entt::basic_storage<value_type, entt::entity, std::pmr::polymorphic_allocator<value_type>> pool{&memory_resource};
    const entt::entity entity{};

    pool.emplace(entity);
    pool.erase(entity);
    memory_resource.reset();
    pool.emplace(entity, 0);

    ASSERT_TRUE(pool.get_allocator().resource()->is_equal(memory_resource));
    ASSERT_EQ(memory_resource.do_allocate_counter(), 0u);
    ASSERT_EQ(memory_resource.do_deallocate_counter(), 0u);
}

TEST(Storage, UsesAllocatorConstruction) {
    using string_type = typename test::tracked_memory_resource::string_type;
    test::tracked_memory_resource memory_resource{};
    entt::basic_storage<string_type, entt::entity, std::pmr::polymorphic_allocator<string_type>> pool{&memory_resource};
    const entt::entity entity{};

    pool.emplace(entity);
    pool.erase(entity);
    memory_resource.reset();
    pool.emplace(entity, test::tracked_memory_resource::default_value);

    ASSERT_TRUE(pool.get_allocator().resource()->is_equal(memory_resource));
    ASSERT_GT(memory_resource.do_allocate_counter(), 0u);
    ASSERT_EQ(memory_resource.do_deallocate_counter(), 0u);
}

#endif
