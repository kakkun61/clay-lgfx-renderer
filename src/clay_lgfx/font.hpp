/**
 * @file font.hpp
 * @brief Text measurement on top of LovyanGFX fonts.
 *
 * The renderer draws text with LovyanGFX's own fonts, anything derived from
 * `lgfx::IFont`: GFXfont fonts such as `fonts::FreeSans9pt7b`, U8g2 fonts
 * such as the Japanese `fonts::lgfxJapanGothic_12`, BDF fonts such as
 * `fonts::efont`, VLW fonts loaded at run time, and so on. Decoding the many
 * font formats is left to LovyanGFX; this file only works out how wide text
 * is, in exactly the way the renderer will draw it. A font of your own is an
 * `lgfx::IFont` subclass.
 */
#pragma once

#include <stdint.h>

#include <LovyanGFX.hpp>

namespace clay_lgfx {

/** @brief A font the renderer can use. Any LovyanGFX font will do. */
using Font = lgfx::IFont;

/**
 * @brief A font registered for one Clay font ID.
 */
struct FontSlot {
  /** The font. */
  const Font *font = nullptr;
  /**
   * Character drawn in place of characters the font lacks, or 0 to draw
   * nothing and advance by zero.
   */
  uint32_t fallback = '?';
};

/**
 * @brief Decodes the next code point from a UTF-8 string.
 *
 * Malformed sequences never stop decoding: each bad byte is consumed on its
 * own and reported as U+FFFD, which ends up drawn as the fallback character.
 *
 * @param text    Pointer to the bytes being decoded.
 * @param length  Number of bytes available at @p text. Must be greater than zero.
 * @param[out] consumed  Receives the number of bytes the code point took.
 * @return The decoded code point.
 */
uint32_t decodeUtf8(const char *text, int32_t length, int32_t *consumed);

/** @brief Counts the code points in a UTF-8 string using decodeUtf8(). */
int32_t countCodepoints(const char *text, int32_t length);

/**
 * @brief Returns the integer factor a font is scaled by to be drawn at @p fontSize.
 *
 * Bitmap fonts come in a single size, so Clay's `fontSize` is honoured by
 * scaling the glyphs up by a whole number: the largest factor whose text
 * height is not taller than @p fontSize, but never less than one. A
 * @p fontSize of zero draws the font at its native size.
 *
 * @param metrics  The font's metrics as returned by `getDefaultMetric()`.
 * @param fontSize Clay's `fontSize`.
 */
int32_t scaleFor(const lgfx::FontMetrics &metrics, uint16_t fontSize);

/**
 * @brief Loads the metrics of the glyph drawn for @p codepoint.
 *
 * Control characters and variation selectors are not drawn. Characters the
 * font lacks, including everything above U+FFFF, which LovyanGFX fonts cannot
 * hold, are replaced by the slot's fallback character.
 *
 * @param slot           The font and its fallback.
 * @param codepoint      The character.
 * @param[in,out] metrics Metrics from `getDefaultMetric()`; the per glyph
 *                       fields are updated for the glyph.
 * @param[out] glyph     Receives the character to hand to `drawChar()`.
 * @return true when something is to be drawn. When false, the character
 *         takes no space and `metrics->x_advance` is set to zero.
 */
bool loadGlyph(const FontSlot &slot, uint32_t codepoint, lgfx::FontMetrics *metrics, uint16_t *glyph);

/**
 * @brief Measures a piece of text the way the renderer draws it.
 *
 * Every code point advances by its scaled advance plus @p letterSpacing, the
 * last one included. Clay measures a line word by word and adds the results
 * up, and counting this way keeps that sum equal to the width of the whole
 * line. Unlike `LGFXBase::textWidth()`, the width does not stretch to cover
 * the ink of a last glyph that reaches beyond its advance, for the same
 * reason.
 *
 * @return The width in pixels.
 */
int32_t measureWidth(const FontSlot &slot, const char *text, int32_t length, int32_t scale, int32_t letterSpacing);

}  // namespace clay_lgfx
