/************************************************************************/
/**
 * @file chScriptTextBindings.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  Fonts, the texts scripts draw each frame and the Text component of a GameObject.
 */
/************************************************************************/
#include "chScriptBindings.h"

#include "chAssetManager.h"
#include "chFont.h"
#include "chFontAsset.h"
#include "chGameObject.h"
#include "chLinearColor.h"
#include "chNumberFormat.h"
#include "chPath.h"
#include "chScene.h"
#include "chScriptManager.h"
#include "chScriptMathTypes.h"
#include "chScriptObjectTypes.h"
#include "chSpriteDraw.h"
#include "chStringUtils.h"
#include "chTextComponent.h"
#include "chTextLayout.h"

namespace chEngineSDK {
CH_SCRIPT_HANDLE_TYPE(TextComponent, "Text");

/**
 * The text of a GameObject: a handle kept with the object's like the other parts, which
 * finds the component each time it is used, so a removed text is an error, never a dangling
 * pointer.
 */
template<>
struct ScriptTraits<TextComponent>
{
  NODISCARD static TextComponent&
  check(lua_State* state, const int32 index)
  {
    GameObject& gameObject =
        ScriptPartHandle::checkOwner(state, index, ScriptHandleType<TextComponent>::NAME);
    TextComponent* text = gameObject.getComponent<TextComponent>();
    if (!text) {
      luaL_error(state, "GameObject '%s' has no Text", gameObject.getName().c_str());
    }
    return *text;
  }
};

namespace {
constexpr const ANSICHAR* kAlignNames[] = {"Left", "Center", "Right"};
static_assert(sizeof(kAlignNames) / sizeof(kAlignNames[0]) ==
              static_cast<SIZE_T>(TextAlign::COUNT));
constexpr const ANSICHAR* kBlendNames[] = {"Alpha", "Additive"};
static_assert(sizeof(kBlendNames) / sizeof(kBlendNames[0]) ==
              static_cast<SIZE_T>(SpriteBlendMode::COUNT));
constexpr const ANSICHAR* kNotationNames[] = {"Plain", "Suffix", "Scientific", "Engineering"};
static_assert(sizeof(kNotationNames) / sizeof(kNotationNames[0]) ==
              static_cast<SIZE_T>(NumberNotation::COUNT));

/*
 * The font asset of a Font value, or null when it no longer exists (or is the default).
 */
SPtr<FontAsset>
findFontAsset(const UUID& id)
{
  if (id.isNull() || !AssetManager::isStarted()) {
    return nullptr;
  }
  const SPtr<IAsset> asset = AssetManager::instance().getAsset(id);
  return asset ? asset->as<FontAsset>() : nullptr;
}

/*
 * The font at index, loaded: a Font value or a path or name. Raises when there is none. A
 * path or name walks every asset, so scripts keep the value Font.find gives.
 */
SPtr<FontAsset>
checkFontAsset(lua_State* state, const int32 index, const ANSICHAR* function)
{
  const auto* value = static_cast<const ScriptFont*>(
      luaL_testudata(state, index, ScriptValueType<ScriptFont>::NAME));
  // The default font needs no asset.
  if (value && value->font.isNull()) {
    return nullptr;
  }
  if (!AssetManager::isStarted()) {
    luaL_error(state, "%s: there are no assets to search", function);
  }
  AssetManager& assetManager = AssetManager::instance();
  SPtr<FontAsset> font;
  if (value) {
    font = findFontAsset(value->font);
    if (!font) {
      luaL_error(state, "%s: the font was deleted", function);
    }
  }
  else {
    const StringView text = ScriptTraits<StringView>::check(state, index);
    const SPtr<IAsset> asset =
        assetManager.findAsset(AssetTypeTraits<FontAsset>::getTypeId(), text);
    if (!asset) {
      luaL_error(state, "%s: there is no font %s", function, String(text).c_str());
    }
    font = asset->as<FontAsset>();
  }
  if (!font->isLoaded() && !assetManager.syncLoadAsset(font)) {
    luaL_error(state, "%s: font %s could not be loaded", function, font->getName());
  }
  return font;
}

/*
 * The Font a style uses at a draw: its asset's font when it is loaded, else the default.
 * One hash lookup.
 */
Font*
resolveFont(const UUID& id)
{
  const SPtr<FontAsset> asset = findFontAsset(id);
  if (asset && asset->isLoaded() && asset->getFont()) {
    return asset->getFont().get();
  }
  return Font::getDefault().get();
}

template<SIZE_T Count>
uint8
checkName(lua_State* state,
          const int32 index,
          const ANSICHAR* function,
          const ANSICHAR* option,
          const ANSICHAR* const (&names)[Count])
{
  const StringView text = ScriptTraits<StringView>::check(state, index);
  for (SIZE_T i = 0; i < Count; ++i) {
    if (StringUtils::equalsIgnoreCase(text, names[i])) {
      return static_cast<uint8>(i);
    }
  }
  String allowed;
  for (SIZE_T i = 0; i < Count; ++i) {
    allowed += i == 0 ? "" : (i + 1 == Count ? " or " : ", ");
    allowed += names[i];
  }
  luaL_error(state, "%s: %s must be %s, not '%s'", function, option, allowed.c_str(),
             String(text).c_str());
  return 0;
}

NODISCARD int16
checkInt16(lua_State* state, const int32 index, const ANSICHAR* option)
{
  const lua_Integer value = luaL_checkinteger(state, index);
  if (value < INT16_MIN || value > INT16_MAX) {
    luaL_error(state, "%s must be between %d and %d", option, INT16_MIN, INT16_MAX);
  }
  return static_cast<int16>(value);
}

NODISCARD int8
checkDecimals(lua_State* state, const int32 index)
{
  const lua_Integer value = luaL_checkinteger(state, index);
  if (value < 0 || value > NumberFormat::MAX_DECIMALS) {
    luaL_error(state, "decimals must be between 0 and %d", NumberFormat::MAX_DECIMALS);
  }
  return static_cast<int8>(value);
}

void
setColor(float (&outValues)[4], const LinearColor& color)
{
  outValues[0] = color.r;
  outValues[1] = color.g;
  outValues[2] = color.b;
  outValues[3] = color.a;
}

NODISCARD LinearColor
toColor(const float (&values)[4])
{
  return LinearColor(values[0], values[1], values[2], values[3]);
}

ScriptTextStyle
makeDefaultStyle()
{
  ScriptTextStyle style{};
  style.font = UUID::null();
  style.size = 1.0f;
  setColor(style.color, LinearColor::WHITE);
  style.pivot[0] = 0.5f;
  style.pivot[1] = 0.5f;
  style.lineSpacing = 1.0f;
  setColor(style.outlineColor, LinearColor::BLACK);
  setColor(style.shadowColor, LinearColor::TRANSPARENT_BLACK);
  style.shadowOffset[0] = 0.05f;
  style.shadowOffset[1] = -0.05f;
  const NumberFormatDesc number{};
  style.plainBelow = number.plainBelow;
  style.align = static_cast<uint8>(TextAlign::Left);
  style.blendMode = static_cast<uint8>(SpriteBlendMode::Alpha);
  style.flags = ScriptTextStyle::RICH_TEXT_BIT |
                (number.bTrimZeros ? ScriptTextStyle::TRIM_ZEROS_BIT : 0u);
  style.notation = static_cast<uint8>(number.notation);
  style.decimals = static_cast<int8>(number.decimals);
  style.plainDecimals = static_cast<int8>(number.plainDecimals);
  return style;
}

NODISCARD NumberFormatDesc
getNumberFormat(const ScriptTextStyle& style)
{
  return {.notation = static_cast<NumberNotation>(style.notation),
          .decimals = style.decimals,
          .bTrimZeros = (style.flags & ScriptTextStyle::TRIM_ZEROS_BIT) != 0,
          .plainBelow = style.plainBelow,
          .plainDecimals = style.plainDecimals};
}

/*
 * The style at index: a TextStyle, a Font (the default style with that font) or nil (the
 * default style).
 */
ScriptTextStyle
checkStyle(lua_State* state, const int32 index, const ANSICHAR* function)
{
  if (const auto* style = static_cast<const ScriptTextStyle*>(
          luaL_testudata(state, index, ScriptValueType<ScriptTextStyle>::NAME))) {
    return *style;
  }
  ScriptTextStyle style = makeDefaultStyle();
  if (lua_isnoneornil(state, index)) {
    return style;
  }
  if (luaL_testudata(state, index, ScriptValueType<ScriptFont>::NAME) == nullptr) {
    luaL_typeerror(state, index, "TextStyle, Font or nil");
  }
  const SPtr<FontAsset> font = checkFontAsset(state, index, function);
  style.font = font ? font->getUUID() : UUID::null();
  return style;
}

/*
 * The text at index: a string, or a number written with the style's number format into
 * buffer, so a counter drawn every frame makes no Lua string. The view is valid during the
 * call.
 */
StringView
checkText(lua_State* state,
          const int32 index,
          const ScriptTextStyle& style,
          ANSICHAR (&buffer)[NumberFormat::MAX_CHARS])
{
  if (lua_type(state, index) == LUA_TNUMBER) {
    return NumberFormat::format(buffer, lua_tonumber(state, index), getNumberFormat(style));
  }
  return ScriptTraits<StringView>::check(state, index);
}

NODISCARD TextLayoutDesc
makeLayoutDesc(const ScriptTextStyle& style)
{
  return {.size = style.size,
          .wrapWidth = style.wrapWidth,
          .align = static_cast<TextAlign>(style.align),
          .lineSpacing = style.lineSpacing,
          .letterSpacing = style.letterSpacing,
          .pivot = Vector2(style.pivot[0], style.pivot[1]),
          .bMonospaceDigits = (style.flags & ScriptTextStyle::MONOSPACE_DIGITS_BIT) != 0,
          .bRichText = (style.flags & ScriptTextStyle::RICH_TEXT_BIT) != 0,
          .color = toColor(style.color)};
}

/*
 * Kept between calls so its memory is reused: drawing a long text allocates nothing once it
 * grew. Main thread only, like every script call.
 */
TextLayout&
getScratchLayout()
{
  static TextLayout s_layout;
  return s_layout;
}

/*
 * The list of the running script's scene for the phase it runs in. Null without a scene.
 */
SpriteDraw*
getSpriteDraw()
{
  const ScriptManager& scriptManager = ScriptManager::instance();
  Scene* const scene = scriptManager.getCurrentScene();
  if (!scene) {
    return nullptr;
  }
  return scriptManager.isInFixedStep() ? &scene->getFixedSpriteDraw()
                                       : &scene->getSpriteDraw();
}

/*
 * Text.makeStyle(options): a TextStyle. Options (all optional): font (Font, path or name;
 * the default font without one), size (1), color, align ("Left", "Center", "Right"),
 * wrapWidth (0: never), lineSpacing (1), letterSpacing, pivot (Vector2, (0, 0) bottom left;
 * center by default), monospaceDigits, richText (true), layer, order, blend, outlineWidth,
 * outlineColor, shadowColor, shadowOffset (Vector2), depth (world X), notation ("Plain",
 * "Suffix", "Scientific", "Engineering"), decimals, trimZeros, plainBelow, plainDecimals.
 * A misspelt option raises.
 */
int32
makeStyle(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Text.makeStyle";
  ScriptTextStyle style = makeDefaultStyle();
  if (lua_isnoneornil(state, 1)) {
    ScriptTraits<ScriptTextStyle>::push(state, style);
    return 1;
  }
  luaL_checktype(state, 1, LUA_TTABLE);

  const auto setFlag = [&style](uint8 bit, bool bOn) {
    style.flags = static_cast<uint8>(bOn ? (style.flags | bit) : (style.flags & ~bit));
  };
  lua_pushnil(state);
  while (lua_next(state, 1) != 0) {
    const int32 value = lua_gettop(state);
    if (lua_type(state, -2) != LUA_TSTRING) {
      return luaL_error(state, "%s: options are named fields", kFunction);
    }
    const StringView key = lua_tostring(state, -2);
    if (key == "font") {
      const SPtr<FontAsset> font = checkFontAsset(state, value, kFunction);
      style.font = font ? font->getUUID() : UUID::null();
    }
    else if (key == "size") {
      style.size = static_cast<float>(luaL_checknumber(state, value));
    }
    else if (key == "color") {
      setColor(style.color, ScriptTraits<LinearColor>::check(state, value));
    }
    else if (key == "align") {
      style.align = checkName(state, value, kFunction, "align", kAlignNames);
    }
    else if (key == "wrapWidth") {
      style.wrapWidth = Math::max(static_cast<float>(luaL_checknumber(state, value)), 0.0f);
    }
    else if (key == "lineSpacing") {
      style.lineSpacing = static_cast<float>(luaL_checknumber(state, value));
    }
    else if (key == "letterSpacing") {
      style.letterSpacing = static_cast<float>(luaL_checknumber(state, value));
    }
    else if (key == "pivot") {
      const Vector2 pivot = ScriptTraits<Vector2>::check(state, value);
      style.pivot[0] = pivot.x;
      style.pivot[1] = pivot.y;
    }
    else if (key == "monospaceDigits") {
      setFlag(ScriptTextStyle::MONOSPACE_DIGITS_BIT, lua_toboolean(state, value) != 0);
    }
    else if (key == "richText") {
      setFlag(ScriptTextStyle::RICH_TEXT_BIT, lua_toboolean(state, value) != 0);
    }
    else if (key == "layer") {
      style.sortingLayer = checkInt16(state, value, "layer");
    }
    else if (key == "order") {
      style.order = checkInt16(state, value, "order");
    }
    else if (key == "blend") {
      style.blendMode = checkName(state, value, kFunction, "blend", kBlendNames);
    }
    else if (key == "outlineWidth") {
      style.outlineWidth = Math::max(static_cast<float>(luaL_checknumber(state, value)), 0.0f);
    }
    else if (key == "outlineColor") {
      setColor(style.outlineColor, ScriptTraits<LinearColor>::check(state, value));
    }
    else if (key == "shadowColor") {
      setColor(style.shadowColor, ScriptTraits<LinearColor>::check(state, value));
    }
    else if (key == "shadowOffset") {
      const Vector2 offset = ScriptTraits<Vector2>::check(state, value);
      style.shadowOffset[0] = offset.x;
      style.shadowOffset[1] = offset.y;
    }
    else if (key == "depth") {
      style.depth = static_cast<float>(luaL_checknumber(state, value));
    }
    else if (key == "notation") {
      style.notation = checkName(state, value, kFunction, "notation", kNotationNames);
    }
    else if (key == "decimals") {
      style.decimals = checkDecimals(state, value);
    }
    else if (key == "trimZeros") {
      setFlag(ScriptTextStyle::TRIM_ZEROS_BIT, lua_toboolean(state, value) != 0);
    }
    else if (key == "plainBelow") {
      style.plainBelow = luaL_checknumber(state, value);
    }
    else if (key == "plainDecimals") {
      style.plainDecimals = checkDecimals(state, value);
    }
    else {
      return luaL_error(state, "%s: unknown option '%s'", kFunction, lua_tostring(state, -2));
    }
    lua_pop(state, 1);
  }
  ScriptTraits<ScriptTextStyle>::push(state, style);
  return 1;
}

/*
 * Text.draw(style, text, x, y, rotation?, scale?, color?): text (a string, or a number
 * written with the style's number format) this frame with its pivot at (x, y) in the YZ
 * plane, turned counterclockwise by rotation degrees, its size times scale. style is a
 * TextStyle, a Font or nil. A text that fits one glyph run (at most 16 characters, no line
 * break, no color tags, inside the wrap width) is one instance; a longer one is laid out
 * and drawn glyph by glyph.
 */
int32
drawText(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Text.draw";
  const ScriptTextStyle style = checkStyle(state, 1, kFunction);
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  const StringView text = checkText(state, 2, style, buffer);
  const Vector2 position(static_cast<float>(luaL_checknumber(state, 3)),
                         static_cast<float>(luaL_checknumber(state, 4)));
  const Radian rotation(Degree(static_cast<float>(luaL_optnumber(state, 5, 0.0))));
  const float scale = static_cast<float>(luaL_optnumber(state, 6, 1.0));
  const LinearColor color =
      lua_isnoneornil(state, 7) ? toColor(style.color) : ScriptTraits<LinearColor>::check(state, 7);

  SpriteDraw* spriteDraw = getSpriteDraw();
  Font* font = resolveFont(style.font);
  if (!spriteDraw || !font || text.empty()) {
    return 0;
  }

  const bool bRichText = (style.flags & ScriptTextStyle::RICH_TEXT_BIT) != 0;
  if (SpriteDraw::fitsGlyphRun(text, bRichText)) {
    const GlyphRunDesc desc{
        .position = position,
        .depth = style.depth,
        .rotation = rotation,
        .size = style.size * scale,
        .pivot = Vector2(style.pivot[0], style.pivot[1]),
        .color = color,
        .bMonospaceDigits = (style.flags & ScriptTextStyle::MONOSPACE_DIGITS_BIT) != 0,
        .letterSpacing = style.letterSpacing * scale,
        .sortingLayer = style.sortingLayer,
        .order = style.order,
        .blendMode = static_cast<SpriteBlendMode>(style.blendMode),
        .outlineWidth = style.outlineWidth,
        .outlineColor = toColor(style.outlineColor),
        .shadowColor = toColor(style.shadowColor),
        .shadowOffset = Vector2(style.shadowOffset[0], style.shadowOffset[1])};
    GlyphRunItem items[SpriteDraw::MAX_RUNS_PER_TEXT];
    const uint32 count = SpriteDraw::makeGlyphRuns(*font, text, desc, items);
    const float wrapWidth = style.wrapWidth * scale;
    if (count > 0 && (wrapWidth <= 0.0f || items[count - 1].width <= wrapWidth)) {
      for (uint32 i = 0; i < count; ++i) {
        spriteDraw->glyphRun(items[i]);
      }
      return 0;
    }
  }

  TextLayout& layout = getScratchLayout();
  TextLayoutDesc layoutDesc = makeLayoutDesc(style);
  layoutDesc.color = color;
  layout.build(*font, text, layoutDesc);
  spriteDraw->text(*font,
                   layout,
                   {.position = position,
                    .depth = style.depth,
                    .rotation = rotation,
                    .scale = scale,
                    .sortingLayer = style.sortingLayer,
                    .order = style.order,
                    .blendMode = static_cast<SpriteBlendMode>(style.blendMode),
                    .outlineWidth = style.outlineWidth,
                    .outlineColor = toColor(style.outlineColor),
                    .shadowColor = toColor(style.shadowColor),
                    .shadowOffset = Vector2(style.shadowOffset[0], style.shadowOffset[1])});
  return 0;
}

/*
 * Text.measure(style, text): width and height of text laid out with style, in world units.
 */
int32
measureText(lua_State* state)
{
  const ScriptTextStyle style = checkStyle(state, 1, "Text.measure");
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  const StringView text = checkText(state, 2, style, buffer);
  Font* font = resolveFont(style.font);
  if (!font) {
    lua_pushnumber(state, 0.0);
    lua_pushnumber(state, 0.0);
    return 2;
  }
  TextLayout& layout = getScratchLayout();
  layout.build(*font, text, makeLayoutDesc(style));
  lua_pushnumber(state, layout.getSize().x);
  lua_pushnumber(state, layout.getSize().y);
  return 2;
}

/*
 * Text.formatNumber(value, notation?, decimals?): the text NumberFormat writes ("Suffix"
 * with 2 decimals by default).
 */
int32
formatNumber(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Text.formatNumber";
  const double value = luaL_checknumber(state, 1);
  NumberFormatDesc desc{};
  if (!lua_isnoneornil(state, 2)) {
    desc.notation =
        static_cast<NumberNotation>(checkName(state, 2, kFunction, "notation", kNotationNames));
  }
  if (!lua_isnoneornil(state, 3)) {
    desc.decimals = checkDecimals(state, 3);
  }
  ANSICHAR buffer[NumberFormat::MAX_CHARS];
  const StringView text = NumberFormat::format(buffer, value, desc);
  lua_pushlstring(state, text.data(), text.size());
  return 1;
}

int32
setStyleColor(lua_State* state)
{
  auto& style = ScriptTraits<ScriptTextStyle>::check(state, 1);
  setColor(style.color, ScriptTraits<LinearColor>::check(state, 2));
  return 0;
}

int32
setStyleSize(lua_State* state)
{
  auto& style = ScriptTraits<ScriptTextStyle>::check(state, 1);
  style.size = static_cast<float>(luaL_checknumber(state, 2));
  return 0;
}

int32
getStyleSize(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<ScriptTextStyle>::check(state, 1).size);
  return 1;
}

int32
setStyleSorting(lua_State* state)
{
  auto& style = ScriptTraits<ScriptTextStyle>::check(state, 1);
  style.sortingLayer = checkInt16(state, 2, "layer");
  style.order = lua_isnoneornil(state, 3) ? style.order : checkInt16(state, 3, "order");
  return 0;
}

int32
setStyleFont(lua_State* state)
{
  auto& style = ScriptTraits<ScriptTextStyle>::check(state, 1);
  const SPtr<FontAsset> font =
      lua_isnoneornil(state, 2) ? nullptr : checkFontAsset(state, 2, "TextStyle.setFont");
  style.font = font ? font->getUUID() : UUID::null();
  return 0;
}

int32
copyStyle(lua_State* state)
{
  const ScriptTextStyle style = ScriptTraits<ScriptTextStyle>::check(state, 1);
  ScriptTraits<ScriptTextStyle>::push(state, style);
  return 1;
}

/*
 * Font.find(pathOrName): a Font value, nil when there is none. Walks every asset, so call it
 * once (in onStart).
 */
int32
findFont(lua_State* state)
{
  const StringView text = ScriptTraits<StringView>::check(state, 1);
  if (!AssetManager::isStarted()) {
    return luaL_error(state, "Font.find: there are no assets to search");
  }
  const SPtr<IAsset> asset =
      AssetManager::instance().findAsset(AssetTypeTraits<FontAsset>::getTypeId(), text);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  ScriptTraits<ScriptFont>::push(state, ScriptFont{asset->getUUID()});
  return 1;
}

/*
 * Font.getDefault(): the engine's font, the one texts without a font use.
 */
int32
getDefaultFont(lua_State* state)
{
  ScriptTraits<ScriptFont>::push(state, ScriptFont{UUID::null()});
  return 1;
}

int32
isFontValid(lua_State* state)
{
  const auto& font = ScriptTraits<ScriptFont>::check(state, 1);
  lua_pushboolean(state, (font.font.isNull() || findFontAsset(font.font)) ? 1 : 0);
  return 1;
}

int32
getFontName(lua_State* state)
{
  const auto& font = ScriptTraits<ScriptFont>::check(state, 1);
  if (font.font.isNull()) {
    lua_pushstring(state, "Default");
    return 1;
  }
  const SPtr<FontAsset> asset = findFontAsset(font.font);
  if (asset) {
    lua_pushstring(state, asset->getName());
  }
  else {
    lua_pushnil(state);
  }
  return 1;
}

/*
 * The virtual path, such as /Game/Fonts/Title; nil for the default font or a deleted one.
 */
int32
getFontPath(lua_State* state)
{
  const auto& font = ScriptTraits<ScriptFont>::check(state, 1);
  const SPtr<FontAsset> asset = findFontAsset(font.font);
  if (!asset) {
    lua_pushnil(state);
    return 1;
  }
  const String path = Path(asset->getAssetPath()).join(Path(asset->getName())).toString();
  lua_pushlstring(state, path.data(), path.size());
  return 1;
}

int32
fontToString(lua_State* state)
{
  const auto& font = ScriptTraits<ScriptFont>::check(state, 1);
  if (font.font.isNull()) {
    lua_pushstring(state, "Font(Default)");
    return 1;
  }
  const SPtr<FontAsset> asset = findFontAsset(font.font);
  lua_pushfstring(state, "Font(%s)", asset ? asset->getName() : "missing");
  return 1;
}

int32
getTextGameObject(lua_State* state)
{
  ScriptTraits<GameObject>::push(
      state, ScriptPartHandle::checkOwner(state, 1, ScriptHandleType<TextComponent>::NAME));
  return 1;
}

/*
 * text:setText(value): a string, or a number written with the text's number format (no Lua
 * string is made, and the text is laid out again only when it changed).
 */
int32
setTextValue(lua_State* state)
{
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  if (lua_type(state, 2) == LUA_TNUMBER) {
    text.setNumber(lua_tonumber(state, 2));
  }
  else {
    text.setText(ScriptTraits<StringView>::check(state, 2));
  }
  return 0;
}

int32
getTextValue(lua_State* state)
{
  const String& value = ScriptTraits<TextComponent>::check(state, 1).getText();
  lua_pushlstring(state, value.data(), value.size());
  return 1;
}

/*
 * text:setNumberFormat(notation, decimals?): how setText writes numbers.
 */
int32
setTextNumberFormat(lua_State* state)
{
  constexpr const ANSICHAR* kFunction = "Text.setNumberFormat";
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  NumberFormatDesc desc = text.getNumberFormat();
  desc.notation =
      static_cast<NumberNotation>(checkName(state, 2, kFunction, "notation", kNotationNames));
  if (!lua_isnoneornil(state, 3)) {
    desc.decimals = checkDecimals(state, 3);
  }
  text.setNumberFormat(desc);
  return 0;
}

int32
setTextFont(lua_State* state)
{
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  text.setFont(lua_isnoneornil(state, 2) ? nullptr : checkFontAsset(state, 2, "Text.setFont"));
  return 0;
}

int32
getTextFont(lua_State* state)
{
  const TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  ScriptTraits<ScriptFont>::push(state, ScriptFont{text.getFontId()});
  return 1;
}

int32
setTextColor(lua_State* state)
{
  ScriptTraits<TextComponent>::check(state, 1).setColor(
      ScriptTraits<LinearColor>::check(state, 2));
  return 0;
}

int32
getTextColor(lua_State* state)
{
  ScriptTraits<LinearColor>::push(state, ScriptTraits<TextComponent>::check(state, 1).getColor());
  return 1;
}

int32
setTextSize(lua_State* state)
{
  ScriptTraits<TextComponent>::check(state, 1).setSize(
      static_cast<float>(luaL_checknumber(state, 2)));
  return 0;
}

int32
getTextSize(lua_State* state)
{
  lua_pushnumber(state, ScriptTraits<TextComponent>::check(state, 1).getSize());
  return 1;
}

int32
setTextSorting(lua_State* state)
{
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  const int16 layer = checkInt16(state, 2, "layer");
  const int16 order = lua_isnoneornil(state, 3) ? text.getOrder() : checkInt16(state, 3, "order");
  text.setSorting(layer, order);
  return 0;
}

int32
getTextSorting(lua_State* state)
{
  const TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  lua_pushinteger(state, text.getSortingLayer());
  lua_pushinteger(state, text.getOrder());
  return 2;
}

/*
 * text:setOutline(width, color?): color keeps its value when not given.
 */
int32
setTextOutline(lua_State* state)
{
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  const float width = static_cast<float>(luaL_checknumber(state, 2));
  text.setOutline(width,
                  lua_isnoneornil(state, 3) ? text.getOutlineColor()
                                            : ScriptTraits<LinearColor>::check(state, 3));
  return 0;
}

/*
 * text:setShadow(color?, offset?): nil color removes the shadow; offset keeps its value when
 * not given.
 */
int32
setTextShadow(lua_State* state)
{
  TextComponent& text = ScriptTraits<TextComponent>::check(state, 1);
  const LinearColor color = lua_isnoneornil(state, 2)
                                ? LinearColor::TRANSPARENT_BLACK
                                : ScriptTraits<LinearColor>::check(state, 2);
  text.setShadow(color,
                 lua_isnoneornil(state, 3) ? text.getShadowOffset()
                                           : ScriptTraits<Vector2>::check(state, 3));
  return 0;
}

int32
setTextVisible(lua_State* state)
{
  ScriptTraits<TextComponent>::check(state, 1).setEnabled(lua_toboolean(state, 2) != 0);
  return 0;
}

int32
isTextVisible(lua_State* state)
{
  lua_pushboolean(state, ScriptTraits<TextComponent>::check(state, 1).isEnabled() ? 1 : 0);
  return 1;
}

/*
 * Width and height of the shown text in world units before the object's scale, as of the
 * last update.
 */
int32
measureTextComponent(lua_State* state)
{
  const Vector2& size = ScriptTraits<TextComponent>::check(state, 1).getTextSize();
  lua_pushnumber(state, size.x);
  lua_pushnumber(state, size.y);
  return 2;
}

/*
 * gameObject:getText(): the object's Text, nil without one.
 */
int32
getObjectText(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  if (!gameObject.getComponent<TextComponent>()) {
    lua_pushnil(state);
    return 1;
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<TextComponent>::NAME);
  return 1;
}

/*
 * gameObject:addText(text?): adds a Text with the default look and gives it; an object that
 * has one already gets that one, with text set when given.
 */
int32
addObjectText(lua_State* state)
{
  GameObject& gameObject = ScriptTraits<GameObject>::check(state, 1);
  TextComponent* text = gameObject.getComponent<TextComponent>();
  if (!text) {
    text = &gameObject.addComponent<TextComponent>();
  }
  if (!lua_isnoneornil(state, 2)) {
    if (lua_type(state, 2) == LUA_TNUMBER) {
      text->setNumber(lua_tonumber(state, 2));
    }
    else {
      text->setText(ScriptTraits<StringView>::check(state, 2));
    }
  }
  ScriptPartHandle::push(state, gameObject, ScriptHandleType<TextComponent>::NAME);
  return 1;
}
} // namespace

/*
 */
void
ScriptBindings::registerText(lua_State* state)
{
  ScriptClass<ScriptFont>(state)
      .rawStaticFunction("find", &findFont, "fun(pathOrName: string): Font?")
      .rawStaticFunction("getDefault", &getDefaultFont, "fun(): Font")
      .rawMethod("isValid", &isFontValid, "fun(): boolean")
      .rawMethod("getName", &getFontName, "fun(): string?")
      .rawMethod("getPath", &getFontPath, "fun(): string?")
      .operators<ScriptOperator::Equal>();
  luaL_getmetatable(state, ScriptValueType<ScriptFont>::NAME);
  lua_pushcfunction(state, &fontToString);
  lua_setfield(state, -2, "__tostring");
  lua_pop(state, 1);

  ScriptClass<ScriptTextStyle>(state)
      .rawMethod("setColor", &setStyleColor, "fun(color: Color)")
      .rawMethod("setSize", &setStyleSize, "fun(size: number)")
      .rawMethod("getSize", &getStyleSize, "fun(): number")
      .rawMethod("setSorting", &setStyleSorting, "fun(layer: integer, order?: integer)")
      .rawMethod("setFont", &setStyleFont, "fun(font?: Font|string)")
      .rawMethod("copy", &copyStyle, "fun(): TextStyle");

  ScriptClass<TextComponent>(state)
      .rawMethod("getGameObject", &getTextGameObject, "fun(): GameObject")
      .rawMethod("setText", &setTextValue, "fun(value: string|number)")
      .rawMethod("getText", &getTextValue, "fun(): string")
      .rawMethod("setNumberFormat", &setTextNumberFormat,
                 "fun(notation: NumberNotation, decimals?: integer)")
      .rawMethod("setFont", &setTextFont, "fun(font?: Font|string)")
      .rawMethod("getFont", &getTextFont, "fun(): Font")
      .rawMethod("setColor", &setTextColor, "fun(color: Color)")
      .rawMethod("getColor", &getTextColor, "fun(): Color")
      .rawMethod("setSize", &setTextSize, "fun(size: number)")
      .rawMethod("getSize", &getTextSize, "fun(): number")
      .rawMethod("setSorting", &setTextSorting, "fun(layer: integer, order?: integer)")
      .rawMethod("getSorting", &getTextSorting, "fun(): integer, integer")
      .rawMethod("setOutline", &setTextOutline, "fun(width: number, color?: Color)")
      .rawMethod("setShadow", &setTextShadow, "fun(color?: Color, offset?: Vector2)")
      .rawMethod("setVisible", &setTextVisible, "fun(visible: boolean)")
      .rawMethod("isVisible", &isTextVisible, "fun(): boolean")
      .rawMethod("measure", &measureTextComponent, "fun(): number, number");

  ScriptClass<GameObject>(state)
      .rawMethod("getText", &getObjectText, "fun(): Text?")
      .rawMethod("addText", &addObjectText, "fun(text?: string|number): Text");

  if (ScriptApiDescription* api = ScriptApiDescription::find(state)) {
    api->addAlias("NumberNotation", "\"Plain\"|\"Suffix\"|\"Scientific\"|\"Engineering\"");
    api->addAlias("TextAlign", "\"Left\"|\"Center\"|\"Right\"");
  }

  ScriptTable(state, "Text",
              "Texts drawn this frame without GameObjects, in the YZ plane (x is right, y up), "
              "and number formatting. Short texts (up to 16 characters, one line, no color "
              "tags) are one instance each, so many counters are cheap.")
      .rawFunction("makeStyle", &makeStyle,
                   "fun(options?: {font?: Font|string, size?: number, color?: Color, align?: "
                   "TextAlign, wrapWidth?: number, lineSpacing?: number, letterSpacing?: "
                   "number, pivot?: Vector2, monospaceDigits?: boolean, richText?: boolean, "
                   "layer?: integer, order?: integer, blend?: \"Alpha\"|\"Additive\", "
                   "outlineWidth?: number, outlineColor?: Color, shadowColor?: Color, "
                   "shadowOffset?: Vector2, depth?: number, notation?: NumberNotation, "
                   "decimals?: integer, trimZeros?: boolean, plainBelow?: number, "
                   "plainDecimals?: integer}): TextStyle")
      .rawFunction("draw", &drawText,
                   "fun(style: TextStyle|Font|nil, text: string|number, x: number, y: number, "
                   "rotation?: number, scale?: number, color?: Color)")
      .rawFunction("measure", &measureText,
                   "fun(style: TextStyle|Font|nil, text: string|number): number, number")
      .rawFunction("formatNumber", &formatNumber,
                   "fun(value: number, notation?: NumberNotation, decimals?: integer): string");
}

} // namespace chEngineSDK
