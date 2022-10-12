#include <gtest/gtest.h>
#include <entt/core/hashed_string.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>

struct base {};
struct clazz: base {};
struct local_only {};

struct MetaContext: ::testing::Test {
    void SetUp() override {
        using namespace entt::literals;

        // global context

        entt::meta<clazz>()
            .type("foo"_hs);

        // local context

        entt::meta<local_only>(context)
            .type("quux"_hs);

        entt::meta<clazz>(context)
            .type("bar"_hs)
            .base<base>();
    }

    void TearDown() override {
        entt::meta_reset(context);
        entt::meta_reset();
    }

    entt::meta_ctx context{};
};

TEST_F(MetaContext, Resolve) {
    using namespace entt::literals;

    ASSERT_TRUE(entt::resolve<clazz>());
    ASSERT_TRUE(entt::resolve<clazz>(context));

    ASSERT_TRUE(entt::resolve<local_only>());
    ASSERT_TRUE(entt::resolve<local_only>(context));

    ASSERT_TRUE(entt::resolve(entt::type_id<clazz>()));
    ASSERT_TRUE(entt::resolve(context, entt::type_id<clazz>()));

    ASSERT_FALSE(entt::resolve(entt::type_id<local_only>()));
    ASSERT_TRUE(entt::resolve(context, entt::type_id<local_only>()));

    ASSERT_TRUE(entt::resolve("foo"_hs));
    ASSERT_FALSE(entt::resolve(context, "foo"_hs));

    ASSERT_FALSE(entt::resolve("bar"_hs));
    ASSERT_TRUE(entt::resolve(context, "bar"_hs));

    ASSERT_FALSE(entt::resolve("quux"_hs));
    ASSERT_TRUE(entt::resolve(context, "quux"_hs));

    ASSERT_EQ((std::distance(entt::resolve().cbegin(), entt::resolve().cend())), 1);
    ASSERT_EQ((std::distance(entt::resolve(context).cbegin(), entt::resolve(context).cend())), 2);
}

TEST_F(MetaContext, MetaType) {
    using namespace entt::literals;

    const auto global = entt::resolve<clazz>();
    const auto local = entt::resolve<clazz>(context);

    ASSERT_TRUE(global);
    ASSERT_TRUE(local);

    ASSERT_NE(global, local);

    ASSERT_EQ(global, entt::resolve("foo"_hs));
    ASSERT_EQ(local, entt::resolve(context, "bar"_hs));

    ASSERT_EQ(global.id(), "foo"_hs);
    ASSERT_EQ(local.id(), "bar"_hs);
}

TEST_F(MetaContext, MetaBase) {
    using namespace entt::literals;

    const auto global = entt::resolve<clazz>();
    const auto local = entt::resolve<clazz>(context);

    ASSERT_TRUE(global);
    ASSERT_TRUE(local);

    ASSERT_EQ((std::distance(global.base().cbegin(), global.base().cend())), 0);
    ASSERT_EQ((std::distance(local.base().cbegin(), local.base().cend())), 1);

    ASSERT_EQ(local.base().cbegin()->second.info(), entt::type_id<base>());

    ASSERT_FALSE(entt::resolve(entt::type_id<base>()));
    ASSERT_FALSE(entt::resolve(context, entt::type_id<base>()));
}

TEST_F(MetaContext, MetaData) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaFunc) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaCtor) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaConv) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaDtor) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaProp) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaTemplate) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaPointer) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaAssociativeContainer) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaSequenceContainer) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaAny) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, MetaHandle) {
    using namespace entt::literals;

    // TODO
}

TEST_F(MetaContext, ContextMix) {
    using namespace entt::literals;

    // TODO
}
