/**
 * @file font_table.hpp
 * @brief Maps Clay font IDs to LovyanGFX fonts and measures text for Clay's layout.
 */
#pragma once

#include <clay.h>
#include <stdint.h>

#include "font.hpp"

namespace clay_lgfx {

/**
 * @brief The set of fonts available to a layout, indexed by Clay's `fontId`.
 *
 * Text whose font ID has no font registered is measured and drawn with font 0.
 * Clay's built in debug view always asks for font 0, so it is a good idea to
 * register something there.
 *
 * The table only stores pointers, so the fonts must outlive it. LovyanGFX's
 * built in fonts are constant data and live forever.
 */
class FontTable {
 public:
  /** @brief Number of distinct font IDs the table can hold. */
  static constexpr uint16_t capacity = 8;

  /**
   * @brief Registers @p font under @p fontId.
   *
   * @code
   * fonts.set(0, &fonts::DejaVu12);
   * fonts.set(1, &fonts::lgfxJapanGothic_12);
   * @endcode
   *
   * Clay caches text measurements. When replacing a font while Clay is
   * already running, call Clay_ResetMeasureTextCache() afterwards, or the
   * layout keeps using the widths of the old font.
   *
   * @param fontId    The ID Clay text configurations refer to.
   * @param font      The font, or nullptr to unregister.
   * @param fallback  Character drawn in place of characters the font lacks,
   *                  or 0 to draw nothing for them.
   * @return false when @p fontId is not below @ref capacity.
   */
  bool set(uint16_t fontId, const Font *font, uint32_t fallback = '?');

  /**
   * @brief Returns the font to use for @p fontId.
   *
   * Falls back to font 0 when nothing is registered under @p fontId, and
   * returns nullptr only when font 0 is missing too.
   */
  const FontSlot *get(uint16_t fontId) const;

  /**
   * @brief Measures a piece of text the same way the renderer will draw it.
   *
   * See measureWidth() and scaleFor() for how the width and the scale are
   * worked out. The height is the font's scaled text height.
   */
  Clay_Dimensions measure(Clay_StringSlice text, uint16_t fontId, uint16_t fontSize, uint16_t letterSpacing) const;

  /**
   * @brief Makes Clay measure text with this table.
   *
   * Call this after Clay_Initialize(). It affects whichever Clay context is
   * current at the time of the call. The table is passed to Clay as user
   * data rather than kept in a global, so several tables, and several Clay
   * contexts, can coexist.
   */
  void attach() const;

  /** @brief The function handed to Clay_SetMeasureTextFunction() by attach(). */
  static Clay_Dimensions measureCallback(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

 private:
  FontSlot slots[capacity] = {};
};

}  // namespace clay_lgfx
