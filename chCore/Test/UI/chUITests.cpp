/************************************************************************/
/**
 * @file chUITests.cpp
 * @author Swampertor
 * @date 2026/11/19
 * @brief Tests of the UI geometry, widget tree, viewport and draw list.
 */
/************************************************************************/
#include "chAngle.h"
#include "chConfigFile.h"
#include "chDisplayEvent.h"
#include "chLinearColor.h"
#include "chSpriteDraw.h"
#include "chUIButton.h"
#include "chUICanvasPanel.h"
#include "chUICanvasPanelSlot.h"
#include "chUIDrawList.h"
#include "chUIImage.h"
#include "chUIPanelWidget.h"
#include "chUIRect.h"
#include "chUITextBlock.h"
#include "chUITypes.h"
#include "chUIUserWidget.h"
#include "chUIViewport.h"
#include "chUIWidget.h"
#include "chUIWidgetTree.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chCoreTestHelpers.h"
#include "chTextTestHelpers.h"

using namespace chEngineSDK;

namespace {
constexpr float kTolerance = 0.0001f;
} // namespace

TEST_CASE("chCore - UI geometry", "[UI]")
{
  SECTION("UIRect contains its edges")
  {
    const UIRect rect{{10.0f, 20.0f}, {100.0f, 50.0f}};
    REQUIRE(rect.contains({10.0f, 20.0f}));
    REQUIRE(rect.contains({110.0f, 70.0f}));
    REQUIRE_FALSE(rect.contains({9.9f, 20.0f}));
    REQUIRE_FALSE(rect.contains({10.0f, 70.1f}));
  }

  SECTION("UIRect overlap and center")
  {
    const UIRect a{{0.0f, 0.0f}, {10.0f, 10.0f}};
    const UIRect b{{5.0f, 5.0f}, {10.0f, 10.0f}};
    const UIRect touching{{10.0f, 0.0f}, {10.0f, 10.0f}};
    REQUIRE(a.overlaps(b));
    REQUIRE_FALSE(a.overlaps(touching));
    REQUIRE(a.getCenter() == Vector2(5.0f, 5.0f));
    REQUIRE(a.getMax() == Vector2(10.0f, 10.0f));
  }

  SECTION("UIMargin insets and clamps")
  {
    const UIRect rect{{10.0f, 20.0f}, {100.0f, 50.0f}};
    const UIRect inset = rect.inset({5.0f, 4.0f, 3.0f, 2.0f});
    REQUIRE(inset.position == Vector2(15.0f, 24.0f));
    REQUIRE(inset.size == Vector2(92.0f, 44.0f));

    const UIRect clamped = rect.inset({200.0f, 200.0f, 200.0f, 200.0f});
    REQUIRE(clamped.size == Vector2::ZERO);
  }

  SECTION("UIGeometry keeps the arranged rect")
  {
    UIGeometry geometry;
    geometry.position = {3.0f, 4.0f};
    geometry.size = {10.0f, 20.0f};
    REQUIRE(geometry.toRect().position == Vector2(3.0f, 4.0f));
    REQUIRE(geometry.toRect().size == Vector2(10.0f, 20.0f));
    REQUIRE(geometry.getCenter() == Vector2(8.0f, 14.0f));
    REQUIRE_FALSE(geometry.bClipped);
  }
}

TEST_CASE("chCore - UI enums and navigation", "[UI]")
{
  // Saved in UI documents, so the values are fixed.
  STATIC_REQUIRE(static_cast<uint32>(UIVisibility::Visible) == 0);
  STATIC_REQUIRE(static_cast<uint32>(UIVisibility::SelfHitTestInvisible) == 4);
  STATIC_REQUIRE(static_cast<uint32>(UINavigation::Up) == 0);
  STATIC_REQUIRE(static_cast<uint32>(UINavigation::Previous) == 5);
  STATIC_REQUIRE(static_cast<uint32>(UINavigationRule::Escape) == 0);
  STATIC_REQUIRE(static_cast<uint32>(UINavigationRule::Custom) == 4);

  const UINavigationLink link;
  REQUIRE(link.rule == UINavigationRule::Escape);
  REQUIRE(link.target == nullptr);

  const UIRenderTransform transform;
  REQUIRE(transform.translation == Vector2::ZERO);
  REQUIRE(transform.scale == Vector2(1.0f, 1.0f));
  REQUIRE(transform.pivot == Vector2(0.5f, 0.5f));
  REQUIRE(transform.angle == 0.0f);

  REQUIRE(StringView(getUIVisibilityName(UIVisibility::Collapsed)) == "Collapsed");
  REQUIRE(StringView(getUINavigationName(UINavigation::Left)) == "Left");
  REQUIRE(StringView(getUINavigationRuleName(UINavigationRule::Wrap)) == "Wrap");
}

namespace {

struct UIHookCounters {
  int32 added = 0;
  int32 removed = 0;
  int32 constructed = 0;
  int32 destructed = 0;
  int32 ticks = 0;
};

class UIHookWidget final : public UIWidget
{
 public:
  explicit UIHookWidget(UIHookCounters* counters) : m_counters(counters) {}

  void
  onAddedToTree() override
  {
    if (m_counters) {
      ++m_counters->added;
    }
  }

  void
  onRemovedFromTree() override
  {
    if (m_counters) {
      ++m_counters->removed;
    }
  }

  void
  onConstruct() override
  {
    if (m_counters) {
      ++m_counters->constructed;
    }
  }

  void
  onDestruct() override
  {
    if (m_counters) {
      ++m_counters->destructed;
    }
  }

  void
  onTick(float deltaTime) override
  {
    CH_PARAMETER_UNUSED(deltaTime);
    if (m_counters) {
      ++m_counters->ticks;
    }
  }

 private:
  UIHookCounters* m_counters = nullptr;
};

/**
 * Places a child at an absolute rectangle. The real canvas slot arrives in point 4; this
 * gives the widget-layer tests deterministic positions without it.
 */
class UIFixedSlot final : public UIPanelSlot
{
 public:
  explicit UIFixedSlot(const UIRect& rect) : m_rect(rect) {}

  void
  arrange(const UIGeometry& parentGeometry, UIGeometry& outChildGeometry) const override
  {
    outChildGeometry = parentGeometry;
    outChildGeometry.position = m_rect.position;
    outChildGeometry.size = m_rect.size;
  }

 private:
  UIRect m_rect;
};

struct UIPointerCounters {
  int32 enters = 0;
  int32 exits = 0;
  int32 downs = 0;
  int32 ups = 0;
  int32 clicks = 0;
  int32 scrolls = 0;
  int32 submits = 0;
  int32 cancels = 0;
  int32 focusReceived = 0;
  int32 focusLost = 0;
  float lastScroll = 0.0f;
  Vector2 lastLocal = Vector2::ZERO;
};

class UIPointerWidget final : public UIWidget
{
 public:
  explicit UIPointerWidget(UIPointerCounters* counters) : m_counters(counters) {}

  void
  onPointerEnter(const Vector2& localPosition) override
  {
    ++m_counters->enters;
    m_counters->lastLocal = localPosition;
  }

  void
  onPointerExit(const Vector2& localPosition) override
  {
    CH_PARAMETER_UNUSED(localPosition);
    ++m_counters->exits;
  }

  void
  onPointerDown(const Vector2& localPosition) override
  {
    CH_PARAMETER_UNUSED(localPosition);
    ++m_counters->downs;
  }

  void
  onPointerUp(const Vector2& localPosition) override
  {
    CH_PARAMETER_UNUSED(localPosition);
    ++m_counters->ups;
  }

  void
  onPointerClick(const Vector2& localPosition) override
  {
    CH_PARAMETER_UNUSED(localPosition);
    ++m_counters->clicks;
  }

  void
  onScroll(float delta) override
  {
    ++m_counters->scrolls;
    m_counters->lastScroll = delta;
  }

  void
  onFocusReceived() override
  {
    ++m_counters->focusReceived;
  }

  void
  onFocusLost() override
  {
    ++m_counters->focusLost;
  }

  void
  onSubmit() override
  {
    ++m_counters->submits;
  }

  void
  onCancel() override
  {
    ++m_counters->cancels;
  }

 private:
  UIPointerCounters* m_counters = nullptr;
};

class UIScrollPanel final : public UIPanelWidget
{
 public:
  explicit UIScrollPanel(UIPointerCounters* counters) : m_counters(counters) {}

  void
  onScroll(float delta) override
  {
    ++m_counters->scrolls;
    m_counters->lastScroll = delta;
  }

 private:
  UIPointerCounters* m_counters = nullptr;
};

class UITestUserWidget final : public UIUserWidget
{
 public:
  explicit UITestUserWidget(UIHookCounters* counters) : m_counters(counters) {}

  NODISCARD UIPanelWidget*
  getPanel() const noexcept
  {
    return m_panel;
  }

  UIWidget*
  addChildAt(StringView name, const UIRect& rect)
  {
    if (m_panel == nullptr) {
      return nullptr;
    }

    UniquePtr<UIWidget> child = chMakeUnique<UIWidget>();
    child->setName(name);
    return m_panel->addChild(std::move(child), chMakeUnique<UIFixedSlot>(rect));
  }

  void
  onInitialized() override
  {
    m_panel = getMutableWidgetTree().createRoot<UIPanelWidget>();
    m_panel->setName("root");
  }

  void
  onConstruct() override
  {
    if (m_counters) {
      ++m_counters->constructed;
    }
  }

  void
  onTick(float deltaTime) override
  {
    CH_PARAMETER_UNUSED(deltaTime);
    if (m_counters) {
      ++m_counters->ticks;
    }
  }

  void
  onDestruct() override
  {
    if (m_counters) {
      ++m_counters->destructed;
    }
  }

 private:
  UIHookCounters* m_counters = nullptr;
  UIPanelWidget* m_panel = nullptr;
};

} // namespace

TEST_CASE("chCore - UI widget tree ownership and lifecycle", "[UI]")
{
  UIHookCounters counters;
  UIWidgetTree tree;

  UIPanelWidget* root = tree.createRoot<UIPanelWidget>();
  root->setName("root");

  UIHookWidget* child =
      static_cast<UIHookWidget*>(root->addChild(chMakeUnique<UIHookWidget>(&counters)));
  child->setName("child");

  REQUIRE(tree.getRoot() == root);
  REQUIRE(tree.getWidgetCount() == 2);
  REQUIRE(tree.findWidgetByName("root") == root);
  REQUIRE(tree.findWidgetByName("child") == child);
  REQUIRE(counters.added == 1);

  UIGeometry geometry;
  geometry.size = {800.0f, 600.0f};
  tree.arrange(geometry);
  REQUIRE(root->getGeometry().size == Vector2(800.0f, 600.0f));
  REQUIRE(child->getGeometry().size == Vector2(800.0f, 600.0f));

  tree.construct();
  REQUIRE(tree.isConstructed());
  REQUIRE(counters.constructed == 1);

  tree.tick(0.016f);
  REQUIRE(counters.ticks == 1);

  tree.destruct();
  REQUIRE_FALSE(tree.isConstructed());
  REQUIRE(counters.destructed == 1);

  REQUIRE(root->removeChild(child));
  REQUIRE(counters.removed == 1);
  REQUIRE(tree.getWidgetCount() == 1);
  REQUIRE(root->getChildCount() == 0);

  tree.clear();
  REQUIRE(tree.getRoot() == nullptr);
  REQUIRE(tree.getWidgetCount() == 0);
}

TEST_CASE("chCore - UI widget hit testing and visibility", "[UI]")
{
  UIPanelWidget root;
  root.setName("root");

  UIGeometry geometry;
  geometry.size = {100.0f, 100.0f};
  root.arrange(geometry);

  UIWidget* a = root.addChild(chMakeUnique<UIWidget>());
  UIWidget* b = root.addChild(chMakeUnique<UIWidget>());
  a->setName("a");
  b->setName("b");
  root.arrange(geometry);

  SECTION("The last child is on top")
  {
    REQUIRE(root.hitTest({50.0f, 50.0f}) == b);
    REQUIRE(root.hitTest({150.0f, 50.0f}) == nullptr);
  }

  SECTION("Visibility decides what is hit")
  {
    b->setVisibility(UIVisibility::HitTestInvisible);
    REQUIRE(root.hitTest({50.0f, 50.0f}) == a);

    b->setVisibility(UIVisibility::SelfHitTestInvisible);
    REQUIRE(root.hitTest({50.0f, 50.0f}) == a);

    b->setVisibility(UIVisibility::Visible);
    a->setVisibility(UIVisibility::Hidden);
    b->setVisibility(UIVisibility::Hidden);
    REQUIRE(root.hitTest({50.0f, 50.0f}) == &root);

    a->setVisibility(UIVisibility::Visible);
    b->setVisibility(UIVisibility::Visible);
    root.setVisibility(UIVisibility::Collapsed);
    REQUIRE(root.hitTest({50.0f, 50.0f}) == nullptr);
  }

  SECTION("The render transform moves the hit area")
  {
    UIRenderTransform transform;
    transform.translation = {200.0f, 0.0f};
    a->setRenderTransform(transform);
    a->arrange(geometry);

    REQUIRE(a->containsPoint({250.0f, 50.0f}));
    REQUIRE_FALSE(a->containsPoint({50.0f, 50.0f}));
  }
}

TEST_CASE("chCore - UI viewport focus and navigation", "[UI]")
{
  UIHookCounters counters;
  UIViewport viewport;
  viewport.setViewportSize({800.0f, 600.0f});

  UniquePtr<UITestUserWidget> user = chMakeUnique<UITestUserWidget>(&counters);
  UITestUserWidget* rawUser = user.get();
  REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);
  REQUIRE(viewport.getUserWidgetCount() == 1);

  UIPanelWidget* panel = rawUser->getPanel();
  UIWidget* topLeft = rawUser->addChildAt("topLeft", {{0.0f, 0.0f}, {100.0f, 100.0f}});
  UIWidget* topRight = rawUser->addChildAt("topRight", {{200.0f, 0.0f}, {100.0f, 100.0f}});
  UIWidget* bottomLeft = rawUser->addChildAt("bottomLeft", {{0.0f, 200.0f}, {100.0f, 100.0f}});
  UIWidget* bottomRight =
      rawUser->addChildAt("bottomRight", {{200.0f, 200.0f}, {100.0f, 100.0f}});
  rawUser->refreshLayout();

  REQUIRE(panel != nullptr);
  REQUIRE(viewport.findWidgetByName("topLeft") == topLeft);
  REQUIRE_FALSE(viewport.setFocusedWidget(panel));

  REQUIRE(viewport.focusFirst());
  REQUIRE(viewport.getFocusedWidget() == topLeft);

  REQUIRE(viewport.moveFocus(UINavigation::Right));
  REQUIRE(viewport.getFocusedWidget() == topRight);

  REQUIRE(viewport.moveFocus(UINavigation::Down));
  REQUIRE(viewport.getFocusedWidget() == bottomRight);

  REQUIRE(viewport.moveFocus(UINavigation::Left));
  REQUIRE(viewport.getFocusedWidget() == bottomLeft);

  REQUIRE(viewport.moveFocus(UINavigation::Up));
  REQUIRE(viewport.getFocusedWidget() == topLeft);

  SECTION("Next and Previous follow document order")
  {
    REQUIRE(viewport.moveFocus(UINavigation::Next));
    REQUIRE(viewport.getFocusedWidget() == topRight);

    REQUIRE(viewport.moveFocus(UINavigation::Previous));
    REQUIRE(viewport.getFocusedWidget() == topLeft);
  }

  SECTION("Explicit and Stop links override the search")
  {
    UINavigationLink explicitLink;
    explicitLink.rule = UINavigationRule::Explicit;
    explicitLink.target = bottomRight;
    topLeft->setNavigation(UINavigation::Down, explicitLink);

    REQUIRE(viewport.moveFocus(UINavigation::Down));
    REQUIRE(viewport.getFocusedWidget() == bottomRight);

    UINavigationLink stopLink;
    stopLink.rule = UINavigationRule::Stop;
    bottomRight->setNavigation(UINavigation::Right, stopLink);

    REQUIRE_FALSE(viewport.moveFocus(UINavigation::Right));
    REQUIRE(viewport.getFocusedWidget() == bottomRight);
  }

  SECTION("Wrap falls back to the opposite edge")
  {
    UINavigationLink wrapLink;
    wrapLink.rule = UINavigationRule::Wrap;
    topLeft->setNavigation(UINavigation::Left, wrapLink);

    REQUIRE(viewport.setFocusedWidget(topLeft));
    REQUIRE(viewport.moveFocus(UINavigation::Left));
    REQUIRE(viewport.getFocusedWidget() == topRight);
  }

  SECTION("Removing the document clears focus")
  {
    REQUIRE(viewport.getFocusedWidget() == topLeft);
    REQUIRE(viewport.removeUserWidget(rawUser));
    REQUIRE(viewport.getFocusedWidget() == nullptr);
    REQUIRE(viewport.getUserWidgetCount() == 0);
    REQUIRE(counters.destructed == 1);
  }
}

TEST_CASE("chCore - UI user widget and viewport", "[UI]")
{
  UIHookCounters counters;
  UIViewport viewport;
  viewport.setViewportSize({640.0f, 480.0f});

  UniquePtr<UITestUserWidget> user = chMakeUnique<UITestUserWidget>(&counters);
  UITestUserWidget* rawUser = user.get();
  REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);

  REQUIRE(rawUser->isInitialized());
  REQUIRE(rawUser->isConstructed());
  REQUIRE(counters.constructed == 1);
  REQUIRE(rawUser->getRootWidget()->getGeometry().size == Vector2(640.0f, 480.0f));

  viewport.setViewportSize({800.0f, 600.0f});
  REQUIRE(rawUser->getRootWidget()->getGeometry().size == Vector2(800.0f, 600.0f));

  viewport.tick(0.016f);
  REQUIRE(counters.ticks == 1);

  REQUIRE(viewport.ownsWidget(rawUser->getRootWidget()));
  REQUIRE_FALSE(viewport.ownsWidget(nullptr));

  UIViewport other;
  other.setViewportSize({100.0f, 100.0f});
  UniquePtr<UITestUserWidget> otherUser = chMakeUnique<UITestUserWidget>(nullptr);
  UITestUserWidget* rawOther =
      static_cast<UITestUserWidget*>(other.addUserWidget(std::move(otherUser)));
  UIWidget* otherWidget = rawOther->addChildAt("other", {{0.0f, 0.0f}, {10.0f, 10.0f}});
  rawOther->refreshLayout();

  REQUIRE_FALSE(viewport.ownsWidget(otherWidget));
  REQUIRE_FALSE(viewport.setFocusedWidget(otherWidget));

  REQUIRE(viewport.removeUserWidget(rawUser));
  REQUIRE(counters.destructed == 1);
  REQUIRE(viewport.getUserWidgetCount() == 0);
}

TEST_CASE("chCore - UI draw list", "[UI]")
{
  UIDrawList drawList({800.0f, 600.0f});

  SECTION("UI points become the sprite plane")
  {
    REQUIRE(drawList.toWorldPosition({0.0f, 0.0f}) == Vector2(0.0f, 600.0f));
    REQUIRE(drawList.toWorldPosition({10.0f, 20.0f}) == Vector2(10.0f, 580.0f));
    REQUIRE(drawList.toUIPosition({10.0f, 580.0f}) == Vector2(10.0f, 20.0f));
    REQUIRE(drawList.getViewportSize() == Vector2(800.0f, 600.0f));
  }

  SECTION("A rectangle is a sprite anchored at its top-left")
  {
    drawList.addRect({{10.0f, 20.0f}, {100.0f, 50.0f}}, LinearColor::WHITE);

    REQUIRE(drawList.getSprites().size() == 1);
    const SpriteItem& sprite = drawList.getSprites()[0];
    REQUIRE(sprite.instance.center == Vector3(0.0f, 60.0f, 555.0f));
    REQUIRE(sprite.instance.halfAxisU == Vector3(0.0f, 50.0f, 0.0f));
    REQUIRE(sprite.instance.halfAxisV == Vector3(0.0f, 0.0f, 25.0f));
    REQUIRE(sprite.instance.textureAndFlags == GpuSpriteInstance::NO_TEXTURE);
    REQUIRE(sprite.radius == 75.0f);
    REQUIRE(sprite.sortingLayer == 0);
    REQUIRE(sprite.order == 0);
    REQUIRE(sprite.bInUse);
    REQUIRE_FALSE(drawList.isEmpty());
  }

  SECTION("Draw order follows the paint order")
  {
    drawList.addRect({{0.0f, 0.0f}, {10.0f, 10.0f}}, LinearColor::RED);
    drawList.addRect({{0.0f, 0.0f}, {10.0f, 10.0f}}, LinearColor::GREEN);

    REQUIRE(drawList.getSprites().size() == 2);
    REQUIRE(drawList.getSprites()[0].sortingLayer == 0);
    REQUIRE(drawList.getSprites()[1].sortingLayer == 0);
    REQUIRE(drawList.getSprites()[0].order == 0);
    REQUIRE(drawList.getSprites()[1].order == 1);
  }

  SECTION("A rotated rectangle turns around its pivot")
  {
    drawList.addRect({{0.0f, 0.0f}, {100.0f, 50.0f}}, LinearColor::WHITE,
                     Radian(Degree(90.0f)), {0.5f, 0.5f});

    REQUIRE(drawList.getSprites().size() == 1);
    const SpriteItem& sprite = drawList.getSprites()[0];
    REQUIRE(sprite.instance.center.nearEqual(Vector3(0.0f, 50.0f, 575.0f), kTolerance));
    REQUIRE(sprite.instance.halfAxisU.nearEqual(Vector3(0.0f, 0.0f, 50.0f), kTolerance));
    REQUIRE(sprite.instance.halfAxisV.nearEqual(Vector3(0.0f, -25.0f, 0.0f), kTolerance));
  }

  SECTION("Empty rectangles are dropped")
  {
    drawList.addRect({{0.0f, 0.0f}, {0.0f, 10.0f}}, LinearColor::WHITE);
    drawList.addRect({{0.0f, 0.0f}, {10.0f, -1.0f}}, LinearColor::WHITE);
    REQUIRE(drawList.isEmpty());
  }

  SECTION("Images keep their texture flags and UV rectangle")
  {
    const float uvRect[4] = {0.25f, 0.5f, 0.125f, 0.25f};
    drawList.addImage({{0.0f, 0.0f}, {10.0f, 10.0f}}, GpuSpriteInstance::NO_TEXTURE, uvRect,
                      LinearColor::WHITE);

    REQUIRE(drawList.getSprites().size() == 1);
    const GpuSpriteInstance& instance = drawList.getSprites()[0].instance;
    REQUIRE(instance.uvRect[0] == 0.25f);
    REQUIRE(instance.uvRect[1] == 0.5f);
    REQUIRE(instance.uvRect[2] == 0.125f);
    REQUIRE(instance.uvRect[3] == 0.25f);
  }

  SECTION("An image without a texture draws its tint alone")
  {
    drawList.addImage({{0.0f, 0.0f}, {10.0f, 10.0f}}, nullptr, SpriteFilter::Point,
                      LinearColor::RED);

    REQUIRE(drawList.getSprites().size() == 1);
    const GpuSpriteInstance& instance = drawList.getSprites()[0].instance;
    // A null texture still carries the filter bit, but the index mask says "no texture".
    REQUIRE((instance.textureAndFlags & GpuSpriteInstance::TEXTURE_INDEX_MASK)
            == GpuSpriteInstance::NO_TEXTURE);
    REQUIRE((instance.textureAndFlags & GpuSpriteInstance::POINT_FILTER_BIT) != 0);
    REQUIRE(instance.uvRect[0] == 0.0f);
    REQUIRE(instance.uvRect[2] == 1.0f);
  }

  SECTION("Clear restarts the frame and its order")
  {
    drawList.addRect({{0.0f, 0.0f}, {10.0f, 10.0f}}, LinearColor::WHITE);
    REQUIRE_FALSE(drawList.isEmpty());

    drawList.clear();
    REQUIRE(drawList.isEmpty());
    REQUIRE(drawList.getSprites().empty());

    drawList.addRect({{0.0f, 0.0f}, {10.0f, 10.0f}}, LinearColor::WHITE);
    REQUIRE(drawList.getSprites()[0].order == 0);
  }
}

TEST_CASE("chCore - UI canvas panel", "[UI]")
{
  SECTION("The slot keeps the SFMX anchor formula")
  {
    UIGeometry parent;
    parent.size = {800.0f, 600.0f};

    UICanvasPanelSlot slot;
    slot.setPosition({10.0f, 20.0f});
    slot.setSize({100.0f, 50.0f});

    UIGeometry child;
    slot.arrange(parent, child);
    REQUIRE(child.position == Vector2(10.0f, 20.0f));
    REQUIRE(child.size == Vector2(100.0f, 50.0f));

    // Centered in the parent, with its own center on the anchor point.
    slot.setAnchors({0.5f, 0.5f}, {0.5f, 0.5f});
    slot.setAlignment({0.5f, 0.5f});
    slot.setPosition(Vector2::ZERO);
    slot.arrange(parent, child);
    REQUIRE(child.position == Vector2(350.0f, 275.0f));

    // The parent's scale and clip reach the child.
    parent.scale = 2.0f;
    parent.bClipped = true;
    parent.clipRect = {{0.0f, 0.0f}, {400.0f, 300.0f}};
    slot.arrange(parent, child);
    REQUIRE(child.scale == 2.0f);
    REQUIRE(child.bClipped);
    REQUIRE(child.clipRect.size == Vector2(400.0f, 300.0f));
  }

  SECTION("The panel places its children")
  {
    UICanvasPanel panel;
    UIGeometry geometry;
    geometry.size = {800.0f, 600.0f};
    panel.arrange(geometry);

    UIWidget* child =
        panel.addChildAt(chMakeUnique<UIWidget>(), {{10.0f, 20.0f}, {100.0f, 50.0f}});
    REQUIRE(child != nullptr);
    REQUIRE(StringView(child->getSlot()->getTypeName()) == "UICanvasPanelSlot");

    panel.arrange(geometry);
    REQUIRE(child->getPosition() == Vector2(10.0f, 20.0f));
    REQUIRE(child->getSize() == Vector2(100.0f, 50.0f));

    UIWidget* defaultChild = panel.addChild(chMakeUnique<UIWidget>());
    REQUIRE(defaultChild != nullptr);
    REQUIRE(StringView(defaultChild->getSlot()->getTypeName()) == "UICanvasPanelSlot");
    panel.arrange(geometry);
    REQUIRE(defaultChild->getSize() == Vector2(100.0f, 100.0f));
  }
}

TEST_CASE("chCore - UI text block", "[UI]")
{
  const SPtr<Font> font = makeTestFont();

  UITextBlock block;
  block.setFont(font.get());
  block.setText("Hello");
  block.setFontSize(24.0f);

  REQUIRE(StringView(block.getTypeName()) == "UITextBlock");
  REQUIRE_FALSE(block.isFocusable());

  UIGeometry blockGeometry;
  blockGeometry.size = {200.0f, 50.0f};
  block.arrange(blockGeometry);
  REQUIRE(block.getDesiredSize().x > 0.0f);
  REQUIRE(block.getDesiredSize().y > 0.0f);

  UIDrawList drawList({800.0f, 600.0f});
  block.paint(drawList);

  const SIZE_T count = drawList.getSprites().size();
  REQUIRE(count > 0);

  // Every glyph stays inside the block, allowing a pixel of bearing around its image.
  for (const SpriteItem& item : drawList.getSprites()) {
    const Vector2 uiPosition =
        drawList.toUIPosition({item.instance.center.y, item.instance.center.z});
    REQUIRE(uiPosition.x >= -1.0f);
    REQUIRE(uiPosition.x <= 201.0f);
    REQUIRE(uiPosition.y >= -1.0f);
    REQUIRE(uiPosition.y <= 51.0f);
  }

  drawList.clear();
  block.setText("Hi");
  block.paint(drawList);
  REQUIRE(drawList.getSprites().size() < count);

  // A color change rebuilds the layout, so the glyph colors follow.
  block.setTextColor(LinearColor::RED);
  drawList.clear();
  block.paint(drawList);
  REQUIRE_FALSE(drawList.isEmpty());
}

TEST_CASE("chCore - UI image", "[UI]")
{
  UIImage image;
  REQUIRE(StringView(image.getTypeName()) == "UIImage");
  REQUIRE_FALSE(image.isFocusable());

  UIGeometry imageGeometry;
  imageGeometry.position = {10.0f, 20.0f};
  imageGeometry.size = {30.0f, 40.0f};
  image.arrange(imageGeometry);
  REQUIRE(image.getFilter() == SpriteFilter::Linear);
  REQUIRE(image.getBlendMode() == SpriteBlendMode::Alpha);

  UIDrawList drawList({100.0f, 100.0f});
  image.paint(drawList);
  REQUIRE(drawList.getSprites().size() == 1);

  const GpuSpriteInstance& instance = drawList.getSprites()[0].instance;
  REQUIRE((instance.textureAndFlags & GpuSpriteInstance::TEXTURE_INDEX_MASK)
          == GpuSpriteInstance::NO_TEXTURE);
  // UI (10, 20) is world (10, 80); the quad's center is half its size below and right.
  REQUIRE(instance.center.nearEqual(Vector3(0.0f, 25.0f, 60.0f), kTolerance));
}

TEST_CASE("chCore - UI button", "[UI]")
{
  UIButton button;
  REQUIRE(StringView(button.getTypeName()) == "UIButton");
  REQUIRE(button.isFocusable());

  int32 clicks = 0;
  button.setOnClicked([&clicks]() { ++clicks; });

  UIGeometry buttonGeometry;
  buttonGeometry.size = {120.0f, 40.0f};
  button.arrange(buttonGeometry);
  UIDrawList drawList({200.0f, 200.0f});
  button.paint(drawList);
  REQUIRE(drawList.getSprites().size() == 1);

  button.onPointerDown({10.0f, 10.0f});
  REQUIRE(button.isPressed());
  button.onPointerUp({10.0f, 10.0f});
  REQUIRE_FALSE(button.isPressed());

  button.onPointerClick({10.0f, 10.0f});
  REQUIRE(clicks == 1);

  button.onSubmit();
  REQUIRE(clicks == 2);

  button.setEnabled(false);
  button.onPointerClick({10.0f, 10.0f});
  button.onSubmit();
  REQUIRE(clicks == 2);

  // Hover is only tracked while the button can be used.
  button.setEnabled(true);
  button.onPointerEnter({10.0f, 10.0f});
  REQUIRE(button.isHovered());
  button.onPointerExit({10.0f, 10.0f});
  REQUIRE_FALSE(button.isHovered());
}

TEST_CASE("chCore - UI viewport pointer input", "[UI]")
{
  UIViewport viewport;
  viewport.setViewportSize({400.0f, 300.0f});

  UniquePtr<UITestUserWidget> user = chMakeUnique<UITestUserWidget>(nullptr);
  UITestUserWidget* rawUser = user.get();
  REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);

  UIPanelWidget* root = rawUser->getPanel();
  REQUIRE(root != nullptr);

  UIPointerCounters leftCounters;
  UIPointerCounters rightCounters;
  UIPointerCounters scrollChildCounters;
  UIPointerCounters scrollPanelCounters;

  UIPointerWidget* left = static_cast<UIPointerWidget*>(root->addChild(
      chMakeUnique<UIPointerWidget>(&leftCounters),
      chMakeUnique<UIFixedSlot>(UIRect{{0.0f, 0.0f}, {100.0f, 100.0f}})));
  UIPointerWidget* right = static_cast<UIPointerWidget*>(root->addChild(
      chMakeUnique<UIPointerWidget>(&rightCounters),
      chMakeUnique<UIFixedSlot>(UIRect{{200.0f, 0.0f}, {100.0f, 100.0f}})));
  UIScrollPanel* scrollPanel = static_cast<UIScrollPanel*>(root->addChild(
      chMakeUnique<UIScrollPanel>(&scrollPanelCounters),
      chMakeUnique<UIFixedSlot>(UIRect{{0.0f, 200.0f}, {100.0f, 100.0f}})));
  UIPointerWidget* scrollChild = static_cast<UIPointerWidget*>(
      scrollPanel->addChild(chMakeUnique<UIPointerWidget>(&scrollChildCounters)));

  rawUser->refreshLayout();

  REQUIRE(viewport.hitTest({50.0f, 50.0f}) == left);
  REQUIRE(viewport.hitTest({250.0f, 50.0f}) == right);
  REQUIRE(viewport.hitTest({50.0f, 250.0f}) == scrollChild);

  SECTION("Hover enters and exits as the pointer moves")
  {
    viewport.processPointer({50.0f, 50.0f}, false, false, false, Vector2::ZERO);
    REQUIRE(viewport.getHoveredWidget() == left);
    REQUIRE(leftCounters.enters == 1);
    REQUIRE(leftCounters.lastLocal == Vector2(50.0f, 50.0f));

    // Moving inside the same widget does not fire the event again.
    viewport.processPointer({60.0f, 60.0f}, false, false, false, Vector2::ZERO);
    REQUIRE(leftCounters.enters == 1);

    viewport.processPointer({250.0f, 50.0f}, false, false, false, Vector2::ZERO);
    REQUIRE(viewport.getHoveredWidget() == right);
    REQUIRE(leftCounters.exits == 1);
    REQUIRE(rightCounters.enters == 1);
    REQUIRE(rightCounters.lastLocal == Vector2(50.0f, 50.0f));

    viewport.processPointer({350.0f, 250.0f}, false, false, false, Vector2::ZERO);
    REQUIRE(viewport.getHoveredWidget() == nullptr);
    REQUIRE(rightCounters.exits == 1);
  }

  SECTION("A press captures the pointer and the release clicks")
  {
    viewport.processPointer({50.0f, 50.0f}, true, true, false, Vector2::ZERO);
    REQUIRE(viewport.getCapturedWidget() == left);
    REQUIRE(leftCounters.downs == 1);
    REQUIRE(viewport.getFocusedWidget() == left);
    REQUIRE(leftCounters.focusReceived == 1);

    viewport.processPointer({50.0f, 50.0f}, true, false, false, Vector2::ZERO);
    REQUIRE(leftCounters.downs == 1);

    viewport.processPointer({50.0f, 50.0f}, false, false, true, Vector2::ZERO);
    REQUIRE(viewport.getCapturedWidget() == nullptr);
    REQUIRE(leftCounters.ups == 1);
    REQUIRE(leftCounters.clicks == 1);
  }

  SECTION("A release outside the widget cancels the click")
  {
    viewport.processPointer({50.0f, 50.0f}, true, true, false, Vector2::ZERO);
    viewport.processPointer({250.0f, 50.0f}, false, false, true, Vector2::ZERO);

    REQUIRE(leftCounters.ups == 1);
    REQUIRE(leftCounters.clicks == 0);
    // The pointer still moves, so hover follows it while the press is captured.
    REQUIRE(viewport.getHoveredWidget() == right);
    REQUIRE(rightCounters.enters == 1);
  }

  SECTION("The wheel bubbles from the hit widget to its ancestors")
  {
    viewport.processPointer({50.0f, 250.0f}, false, false, false, {0.0f, 1.0f});
    REQUIRE(scrollChildCounters.scrolls == 1);
    REQUIRE(scrollChildCounters.lastScroll == Approx(1.0f));
    REQUIRE(scrollPanelCounters.scrolls == 1);
    REQUIRE(scrollPanelCounters.lastScroll == Approx(1.0f));

    // A horizontal wheel without a vertical part still reaches onScroll.
    viewport.processPointer({50.0f, 250.0f}, false, false, false, {2.0f, 0.0f});
    REQUIRE(scrollChildCounters.scrolls == 2);
    REQUIRE(scrollChildCounters.lastScroll == Approx(2.0f));
  }

  SECTION("InputState drives the pointer")
  {
    InputState state;
    state.beginFrame();
    state.addMouseMove(50.0f, 50.0f, 50.0f, 50.0f);
    state.setKey(InputKey::MouseLeft, true);
    viewport.processInput(state);

    REQUIRE(viewport.getHoveredWidget() == left);
    REQUIRE(viewport.getCapturedWidget() == left);
    REQUIRE(leftCounters.downs == 1);

    state.beginFrame();
    state.setKey(InputKey::MouseLeft, false);
    viewport.processInput(state);

    REQUIRE(viewport.getCapturedWidget() == nullptr);
    REQUIRE(leftCounters.ups == 1);
    REQUIRE(leftCounters.clicks == 1);
  }

  SECTION("Removing the document clears hover and capture")
  {
    viewport.processPointer({50.0f, 50.0f}, true, true, false, Vector2::ZERO);
    REQUIRE(viewport.getHoveredWidget() == left);
    REQUIRE(viewport.getCapturedWidget() == left);

    REQUIRE(viewport.removeUserWidget(rawUser));
    REQUIRE(viewport.getHoveredWidget() == nullptr);
    REQUIRE(viewport.getCapturedWidget() == nullptr);
    REQUIRE(leftCounters.exits == 1);
  }
}

TEST_CASE("chCore - UI viewport actions", "[UI]")
{
  UIViewport viewport;
  viewport.setViewportSize({400.0f, 300.0f});

  UniquePtr<UITestUserWidget> user = chMakeUnique<UITestUserWidget>(nullptr);
  UITestUserWidget* rawUser = user.get();
  REQUIRE(viewport.addUserWidget(std::move(user)) == rawUser);

  UIPanelWidget* root = rawUser->getPanel();
  REQUIRE(root != nullptr);

  UIPointerCounters leftCounters;
  UIPointerCounters rightCounters;

  UIPointerWidget* left = static_cast<UIPointerWidget*>(root->addChild(
      chMakeUnique<UIPointerWidget>(&leftCounters),
      chMakeUnique<UIFixedSlot>(UIRect{{0.0f, 0.0f}, {100.0f, 100.0f}})));
  UIPointerWidget* right = static_cast<UIPointerWidget*>(root->addChild(
      chMakeUnique<UIPointerWidget>(&rightCounters),
      chMakeUnique<UIFixedSlot>(UIRect{{200.0f, 0.0f}, {100.0f, 100.0f}})));
  rawUser->refreshLayout();

  REQUIRE(viewport.setFocusedWidget(left));

  SECTION("Submit and cancel go to the focused widget")
  {
    UIViewport::ActionState actions;
    actions.bSubmit = true;
    viewport.processActions(actions);
    REQUIRE(leftCounters.submits == 1);
    REQUIRE(viewport.getFocusedWidget() == left);

    actions = UIViewport::ActionState();
    actions.bCancel = true;
    viewport.processActions(actions);
    REQUIRE(leftCounters.cancels == 1);
    REQUIRE(viewport.getFocusedWidget() == left);
  }

  SECTION("Navigation actions move focus")
  {
    UIViewport::ActionState actions;
    actions.bNavigateRight = true;
    viewport.processActions(actions);
    REQUIRE(viewport.getFocusedWidget() == right);

    actions = UIViewport::ActionState();
    actions.bNext = true;
    viewport.processActions(actions);
    REQUIRE(viewport.getFocusedWidget() == left);

    actions = UIViewport::ActionState();
    actions.bPrevious = true;
    viewport.processActions(actions);
    REQUIRE(viewport.getFocusedWidget() == right);
  }

  SECTION("InputManager action ids drive the viewport")
  {
    InputManager input;
    ConfigFile defaults;
    defaults.parse("[UISubmit]\nKeys=Space\n"
                   "[UICancel]\nKeys=Escape\n"
                   "[UIRight]\nKeys=D\n"
                   "[UINext]\nKeys=Tab\n");
    input.loadBindings(defaults, ConfigFile());

    UIViewport::ActionBindings bindings;
    bindings.submit = input.findAction("UISubmit");
    bindings.cancel = input.findAction("UICancel");
    bindings.navigateRight = input.findAction("UIRight");
    bindings.next = input.findAction("UINext");
    REQUIRE(bindings.submit != InputManager::INVALID_ACTION);
    REQUIRE(bindings.cancel != InputManager::INVALID_ACTION);
    REQUIRE(bindings.navigateRight != InputManager::INVALID_ACTION);
    REQUIRE(bindings.next != InputManager::INVALID_ACTION);
    viewport.setActionBindings(bindings);

    const auto frame = [&input, &viewport](InputKey key, bool bDown) {
      input.beginFrame();
      input.processEvent(DisplayEvent(KeyEvent{.key = key, .bDown = bDown, .modifiers = {}}));
      input.updateActions();
      viewport.processInput(input);
    };

    frame(InputKey::Space, true);
    REQUIRE(leftCounters.submits == 1);

    frame(InputKey::Space, false);
    frame(InputKey::Escape, true);
    REQUIRE(leftCounters.cancels == 1);

    frame(InputKey::Escape, false);
    frame(InputKey::D, true);
    REQUIRE(viewport.getFocusedWidget() == right);

    frame(InputKey::D, false);
    frame(InputKey::Tab, true);
    REQUIRE(viewport.getFocusedWidget() == left);
  }
}
