# Chimera UI Port — Design and Plan

Status: **point 4 (basic widgets) implemented and tested.** Next: point 5 (input).

Verified: `chCoreTest` builds and passes — 271,259 assertions in 114 test cases; the eleven
UI cases hold 243 assertions.

This is the shared memory of the port. It records the decisions, the class model, and
the order the work is done in, so the design does not have to be rebuilt from chat.

## Goal

Port the runtime UI system that exists in `SFMX/FrameworkGame` to Chimera, using a
UMG/Unreal-style widget model instead of the SFMX global `UIManager` singleton.

What is ported from SFMX:

- Anchor/pivot/offset layout math (`UISlot::computePosition`).
- Hit-testing, blocking/fall-through, selection and directional navigation scoring.
- The behaviour of the nine widgets.
- The depth-first UI document format and the widget registry.

What is replaced:

- SFML vectors/rects/colours → `Vector2`, `UIRect`, `LinearColor`.
- `sf::RenderTarget` + `sf::RenderStates` → `UIDrawList` over `SpriteDraw`.
- `sf::Window` events → `InputManager`/`InputState` + a new `TextInputEvent`.
- Memory pools → `UIWidgetTree` ownership.
- `UIManager` → `UIViewport` + `UIUserWidget` + `UIWidgetTree`.

## Decisions

- **Widget model: UMG-style.**
  - `UIWidget` is the base.
  - `UIPanelWidget` has many children, each with a `UIPanelSlot`.
  - `UIContentWidget` has one child (button, border, size box).
  - `UIWidgetTree` owns the widget hierarchy of one document (the root, transitively).
  - `UIUserWidget` owns a `UIWidgetTree` and has the UMG lifecycle
    (`onInitialized`, `onConstruct`, `onTick`, `onDestruct`).
  - `UIViewport` owns the user widgets added to the screen and routes input.
- **Ownership:** `UIViewport` owns `UIUserWidget`s; `UIUserWidget` owns its
  `UIWidgetTree`; the tree owns the root; every panel owns its child widgets and their
  slots. Parent/child, slot, focus and navigation references are non-owning `UIWidget*`.
- **Layout:** the SFMX `UISlot` becomes `UICanvasPanelSlot`; the SFMX boxes become
  `UIVerticalBoxSlot` / `UIHorizontalBoxSlot`; `UIScrollView` becomes `UIScrollBox`.
- **Documents:** a new `UIDocumentAsset` (`.chAss`) stores the serialized widget tree.
  `createUserWidget()` builds an independent tree from the saved bytes.
- **Rendering:** `UIRenderer` owns an orthographic screen-space `Camera` and a
  `SpriteDraw`; `UIDrawList` converts UI top-left coordinates into the renderer's YZ
  plane. The overlay pass loads the back buffer instead of clearing it.
- **Input:** pointer and wheel come from `InputState`; navigation/submit/cancel use
  `InputActionId`. Text editing needs a new `TextInputEvent` plus
  `SDL_StartTextInput`/`SDL_StopTextInput`.
- **Scope:** screen-space UI first. World-space `UIWidgetComponent` later.

## Class map

| SFMX | UMG | Chimera |
|---|---|---|
| `UIManager` | game viewport + `AddToViewport` | `UIViewport` |
| root list | `UUserWidget` instances | `UIViewport` owns `UIUserWidget`s |
| `UIWidget` tree | `UWidgetTree` | `UIWidgetTree` |
| `UISlot` | `UCanvasPanelSlot` | `UICanvasPanelSlot` |
| `UIHorizontalBox` | `UHorizontalBox` | `UIHorizontalBox` |
| `UIVerticalBox` | `UVerticalBox` | `UIVerticalBox` |
| `UIScrollView` | `UScrollBox` | `UIScrollBox` |
| `UILabel` | `UTextBlock` | `UITextBlock` |
| `UIImage` | `UImage` | `UIImage` |
| `UIButton` | `UButton` | `UIButton` |
| `UICheckbox` | `UCheckBox` | `UICheckBox` |
| `UISlider` | `USlider` | `UISlider` |
| `UITextBox` | `UEditableTextBox` | `UIEditableTextBox` |
| `saveUI`/`loadUI` | Widget Blueprint | `UIDocumentAsset` |
| `Game::initUI()` | `UUserWidget` subclass | `UIUserWidget` subclass |

Class tree:

```
UIWidget
├── UIPanelWidget                 multiple children, slot per child
│   ├── UICanvasPanel             free placement + anchors
│   ├── UIVerticalBox
│   ├── UIHorizontalBox
│   ├── UIOverlay                 stacking
│   └── UIScrollBox
├── UIContentWidget               one child
│   ├── UIButton
│   ├── UIBorder
│   └── UISizeBox
├── UITextBlock
├── UIImage
├── UICheckBox
├── UISlider
├── UIEditableTextBox
└── UIUserWidget                  owns a UIWidgetTree
```

## Widget layer as implemented

- `UIWidget` is concrete, so tests and simple documents can use a bare widget. Derived
  widgets override `getTypeName`, `arrangeChildren`, `paint` and the input hooks.
- Geometry is absolute in viewport space. A panel's slot computes each child's absolute
  rectangle; `UIWidget::arrange` stores it and calls `arrangeChildren`. The base slot
  fills the parent, so a plain panel stacks its children.
- `UIPanelWidget` owns `Vector<UniquePtr<UIWidget>>` and a parallel
  `Vector<UniquePtr<UIPanelSlot>>`. `UIContentWidget` owns one content and one slot.
  `UIWidgetTree` owns the root, which transitively owns the rest.
- Panels and content wrappers default to `setFocusable(false)`; navigation lands on the
  controls they contain.
- `UIWidget` provides `onAddedToTree`, `onRemovedFromTree`, `onConstruct`, `onDestruct`,
  `onTick`, `paint` and the pointer/focus hooks. Events (`Event<>`) were not added yet;
  point 10 can either subscribe through subclasses or add widget events.
- `UIUserWidget` runs `initialize` → `construct` → `tick` → `destruct`.
  `onInitialized` builds the tree; `refreshLayout` arranges it to the viewport.
- `UIViewport` owns user widgets, arranges them on resize, ticks/paints them, hit-tests
  last-to-first and owns focus. Focus navigation implements `Escape`, `Explicit`, `Wrap`,
  `Stop`, `Custom`, plus `Next`/`Previous` document order. Pointer dispatch is point 5.
- `paint(UIDrawList&)` is implemented; widgets draw through the draw list and `UIRenderer`
  adds the overlay pass.

## Basic widgets as implemented

- `UICanvasPanel` gives new children a `UICanvasPanelSlot`; `addChildAt` places one at an
  absolute rectangle. The slot keeps the SFMX anchor formula and carries the parent's
  scale and clip to the child.
- `UITextBlock` builds and caches a `TextLayout` with a top-left pivot, rebuilt when the
  text, font, size, color, margin or wrapping width changes. It uses `Font::getDefault()`
  when no font is set and is not focusable.
- `UIImage` draws a `TextureAsset` region with a filter, flips, tint and blend mode; a null
  texture draws the tint alone. It is not focusable.
- `UIButton` derives from `UIContentWidget`, is focusable, paints normal/hovered/pressed/
  disabled colors and calls `onClicked` on click or submit. Pointer dispatch is point 5.

## Layout model

- UI space is top-left origin, +X right, +Y down.
- `UIRect` is `position + size`; `UIMargin` is left/top/right/bottom.
- `UICanvasPanelSlot` keeps the SFMX formula exactly:

```
refMin = anchorMin * parentSize
refMax = anchorMax * parentSize
ref    = lerp(refMin, refMax, alignment)
pos    = ref + position - alignment * size
```

- `alignment` is UMG's name for the SFMX `pivot`.
- Box slots own padding, fill/auto size and child alignment; the box itself only
  stacks its children.
- `UIScrollBox` will need per-clip-group scissor support; phase 1 culls fully
  outside children and defers real clipping.

## Rendering model

- `UIRenderer` camera: orthographic, at `(-10, 0, height/2)`, looking along `+X`,
  `orthographicSize = height/2`, viewport `width x height`.
- UI point `(x, y)` becomes world `(x, height - y)`.
- A widget quad is a `SpriteQuad` with `NO_TEXTURE` for solid rectangles, or a
  texture's bindless flags for images.
- Text uses `TextLayout` + `SpriteDraw::text`; labels cache their layout and rebuild
  when the text, font or size changes.
- Draw order: one `order` per drawable, `depth` decreasing so later draws cover
  earlier ones.
- `SpritePass::addPass` gains a `LoadOp` parameter and an encoded-output pipeline
  variant. The scene path writes linear values into an HDR target and encodes through
  the tonemap pass; the UI pass writes the UNORM back buffer, so it must encode.

## Input model

- Pointer: `InputState::getMousePosition`, `isDown/wasPressed/wasReleased(MouseLeft)`,
  `getMouseWheel`.
- Focus: `UIViewport::setFocusedWidget` fires `onFocusLost`/`onFocusReceived`.
- Navigation: `UINavigationRule::Escape` uses the SFMX scored search;
  `Explicit` uses the link target; `Wrap` and `Stop` are small additions.
- Visibility replaces the SFMX `blocksInput` flag:
  - `Visible` is hit-tested and blocks widgets below.
  - `SelfHitTestInvisible` is not hit itself but its children are.
  - `HitTestInvisible` blocks neither itself nor its children.
  - `Hidden`/`Collapsed` are not hit-tested.
- Text: new `TextInputEvent` in `chDisplayEvent.h` with a fixed UTF-8 buffer so it
  stays trivially copyable; `EventDispatcherManager::OnTextInput`; SDL
  `SDL_EVENT_TEXT_INPUT` mapping. While an editable text box is focused, disable the
  gameplay input context so typing does not fire actions.

## Documents and assets

- `UIDocumentAsset : IAsset` stores the serialized tree as bytes plus the referenced
  font/texture UUIDs.
- `createUserWidget()` returns a `UniquePtr<UIUserWidget>` with a fresh tree.
- Registered in `AssetManager::initialize()` next to the other asset creators.
- `collectReferences` walks the tree so packaging follows UI documents.
- `GameApplication` loads the HUD document from `Game.ini`.

## Milestones

1. [x] **Core types** — `chUIRect.h` (`UIMargin`, `UIRect`, `UIGeometry`) and
   `chUITypes.h` (`UIVisibility`, `UINavigation`, `UINavigationRule`,
   `UINavigationLink`, `UIRenderTransform`).
2. [x] **Widget layer** — `UIWidget`, `UIWidgetTree`, `UIUserWidget`,
   `UIPanelWidget`, `UIPanelSlot`, `UIContentWidget`, `UIViewport`.
3. [x] **Renderer** — `UIDrawList`, `UIRenderer`, `SpritePass` load/encode.
4. [x] **Basic widgets** — `UICanvasPanel` + `UICanvasPanelSlot`, `UITextBlock`,
   `UIImage`, `UIButton`.
5. [ ] **Input** — pointer, hover/press/click, wheel, focus, navigation.
6. [ ] **HUD sample** — `UIHudWidget : UIUserWidget`, added to `UIViewport` from
   `GameApplication`.
7. [ ] **Tests** — slot math, tree ownership, hit-test/visibility, focus/navigation,
   button click.
8. [ ] **Remaining widgets** — boxes, overlay, border, size box, check box, slider,
   editable text box, scroll box.
9. [ ] **Serialization + asset** — `UIDocument`, `UIDocumentAsset`, registry,
   references, `Game.ini` load.
10. [ ] **Scripting** — `UI.get`, show/hide, text, value, click callbacks.

## Files

Current:

- `chCore/Source/UI/chUIRect.h`
- `chCore/Source/UI/chUITypes.h`
- `chCore/Source/UI/chUIWidget.h/.cpp`
- `chCore/Source/UI/chUIWidgetTree.h/.cpp`
- `chCore/Source/UI/chUIUserWidget.h/.cpp`
- `chCore/Source/UI/chUIViewport.h/.cpp`
- `chCore/Source/UI/chUIPanelWidget.h/.cpp`
- `chCore/Source/UI/chUIPanelSlot.h/.cpp`
- `chCore/Source/UI/chUIContentWidget.h/.cpp`
- `chCore/Source/UI/chUIDrawList.h/.cpp`
- `chCore/Source/UI/chUIRenderer.h/.cpp`
- `chCore/Source/UI/chUICanvasPanel.h/.cpp`
- `chCore/Source/UI/chUICanvasPanelSlot.h/.cpp`
- `chCore/Source/UI/chUITextBlock.h/.cpp`
- `chCore/Source/UI/chUIImage.h/.cpp`
- `chCore/Source/UI/chUIButton.h/.cpp`
- `chCore/Test/UI/chUITests.cpp`

Planned:

- `chCore/Source/UI/chUIDocument.h/.cpp`
- `chCore/Source/UI/chUIDocumentAsset.h/.cpp`

`ch_collect_sources` globs `Source/**/*.cpp|h`, so new files need no `CMakeLists.txt`
edit. `Private/` folders are not on the public include path.

## Open points

- Widget events: `UIWidget` uses virtual hooks only for now. If scripting or game code
  needs subscriptions without subclassing, add `Event<>` members in point 10.
- Render transforms: hit-testing applies them, painting text, images and button
  backgrounds does not yet.
- Button padding: the content fills the button; add a content margin when UMG-style
  padding is needed.
- Auto size: no slot sizes a widget from its desired size yet; `UITextBlock::getDesiredSize`
  is ready for the boxes and size box in point 8.
- Clipping: group sprites by clip rect in `SpriteBatcher`, or a clip rect per
  instance. Not needed until `UIScrollBox`.
- Colour space: the encoded-output variant is the chosen path; confirm with the
  renderer owner if the back buffer format ever becomes sRGB.
- World-space UI: `UIWidgetComponent` after screen-space UI works.
- Editor: document preview/inspector after the runtime is complete.
