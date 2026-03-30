#include <gtest/gtest.h>
#include <campello_widgets/ui/drag_details.hpp>

namespace cw = systems::leal::campello_widgets;

// Helper: creates an isolated DragManager for each test
struct DragFixture : public ::testing::Test
{
    cw::DragManager mgr;

    void SetUp() override
    {
        cw::DragManager::setActive(&mgr);
    }

    void TearDown() override
    {
        cw::DragManager::setActive(nullptr);
    }
};

// -----------------------------------------------------------------------
// Session lifecycle
// -----------------------------------------------------------------------

TEST_F(DragFixture, InitiallyNotDragging)
{
    EXPECT_FALSE(mgr.isDragging());
}

TEST_F(DragFixture, StartSessionSetsDragging)
{
    int data = 42;
    mgr.startSession(typeid(int), &data, {10, 20});
    EXPECT_TRUE(mgr.isDragging());
    EXPECT_EQ(mgr.sessionType(), std::type_index(typeid(int)));
    EXPECT_EQ(mgr.sessionData(), &data);
    EXPECT_FLOAT_EQ(mgr.sessionPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(mgr.sessionPosition().y, 20.0f);
}

TEST_F(DragFixture, EndSessionClearsDragging)
{
    int data = 1;
    mgr.startSession(typeid(int), &data, {0, 0});
    mgr.endSession();
    EXPECT_FALSE(mgr.isDragging());
}

TEST_F(DragFixture, UpdatePositionChangesSessionPosition)
{
    int data = 0;
    mgr.startSession(typeid(int), &data, {0, 0});
    mgr.updatePosition({50, 75});
    EXPECT_FLOAT_EQ(mgr.sessionPosition().x, 50.0f);
    EXPECT_FLOAT_EQ(mgr.sessionPosition().y, 75.0f);
}

TEST_F(DragFixture, UpdatePositionNoOpWhenNotDragging)
{
    // Should not crash
    EXPECT_NO_THROW(mgr.updatePosition({100, 100}));
}

// -----------------------------------------------------------------------
// Target registration
// -----------------------------------------------------------------------

TEST_F(DragFixture, RegisterAndUnregisterTarget)
{
    int id_token = 0;
    void* id = &id_token;

    mgr.registerTarget(id,
        []() { return true; },
        []() {},
        []() {},
        []() {});

    // Give the target some bounds that contain the pointer
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(0, 0, 100, 100));

    int data = 1;
    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50});
    bool accepted = mgr.endSession();
    EXPECT_TRUE(accepted);

    // After unregister, same drag is not accepted
    mgr.registerTarget(id,
        []() { return true; },
        []() {},
        []() {},
        []() {});
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(0, 0, 100, 100));
    mgr.unregisterTarget(id);

    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50});
    accepted = mgr.endSession();
    EXPECT_FALSE(accepted);
}

// -----------------------------------------------------------------------
// Enter / exit callbacks
// -----------------------------------------------------------------------

TEST_F(DragFixture, EnterCalledWhenPointerEntersTarget)
{
    int id_token = 0;
    void* id = &id_token;
    int enters = 0, exits = 0;

    mgr.registerTarget(id,
        []() { return true; },
        [&]() { ++enters; },
        [&]() { ++exits; },
        []()  {});
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(50, 50, 100, 100));

    int data = 1;
    mgr.startSession(typeid(int), &data, {0, 0});

    // Move outside target — no enter yet
    mgr.updatePosition({10, 10});
    EXPECT_EQ(enters, 0);

    // Move inside target — enter fires
    mgr.updatePosition({75, 75});
    EXPECT_EQ(enters, 1);

    // Move inside again — no duplicate enter
    mgr.updatePosition({80, 80});
    EXPECT_EQ(enters, 1);

    // Move outside — exit fires
    mgr.updatePosition({10, 10});
    EXPECT_EQ(exits, 1);

    mgr.endSession();
}

TEST_F(DragFixture, ExitCalledOnEndSession)
{
    int id_token = 0;
    void* id = &id_token;
    int exits = 0;

    mgr.registerTarget(id,
        []() { return true; },
        []() {},
        [&]() { ++exits; },
        []()  {});
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(0, 0, 200, 200));

    int data = 1;
    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50}); // enter
    mgr.endSession();             // exit + accept

    EXPECT_EQ(exits, 1);
}

// -----------------------------------------------------------------------
// Accept callback and type checking
// -----------------------------------------------------------------------

TEST_F(DragFixture, AcceptCalledOnDrop)
{
    int id_token = 0;
    void* id = &id_token;
    bool accepted = false;

    mgr.registerTarget(id,
        []()  { return true; },
        []()  {},
        []()  {},
        [&]() { accepted = true; });
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(0, 0, 200, 200));

    int data = 1;
    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50});
    mgr.endSession();

    EXPECT_TRUE(accepted);
}

TEST_F(DragFixture, WillAcceptFalseBlocksAccept)
{
    int id_token = 0;
    void* id = &id_token;
    bool accepted = false;

    mgr.registerTarget(id,
        []()  { return false; }, // reject all
        []()  {},
        []()  {},
        [&]() { accepted = true; });
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(0, 0, 200, 200));

    int data = 1;
    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50});
    bool result = mgr.endSession();

    EXPECT_FALSE(result);
    EXPECT_FALSE(accepted);
}

TEST_F(DragFixture, PointerOutsideBoundsNotAccepted)
{
    int id_token = 0;
    void* id = &id_token;
    bool accepted = false;

    mgr.registerTarget(id,
        []()  { return true; },
        []()  {},
        []()  {},
        [&]() { accepted = true; });
    // Target at [200, 200, 100×100]
    mgr.updateTargetBounds(id, cw::Rect::fromLTWH(200, 200, 100, 100));

    int data = 1;
    mgr.startSession(typeid(int), &data, {50, 50});
    mgr.updatePosition({50, 50}); // outside the target
    mgr.endSession();

    EXPECT_FALSE(accepted);
}

// -----------------------------------------------------------------------
// Global accessor
// -----------------------------------------------------------------------

TEST_F(DragFixture, GlobalAccessorReturnsSetInstance)
{
    EXPECT_EQ(cw::DragManager::active(), &mgr);
}

TEST(DragManager, GlobalAccessorNullAfterClear)
{
    cw::DragManager::setActive(nullptr);
    EXPECT_EQ(cw::DragManager::active(), nullptr);
}
