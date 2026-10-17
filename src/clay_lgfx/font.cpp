#include "font.hpp"

namespace clay_lgfx {

namespace {

constexpr uint32_t replacementCharacter = 0xFFFD;

// Characters LovyanGFX itself never draws when printing strings.
bool invisible(uint32_t codepoint) { return codepoint < 0x20 || (codepoint >= 0xFE00 && codepoint < 0xFE10); }

bool tryLoad(const Font &font, uint32_t codepoint, lgfx::FontMetrics *metrics) {
  return codepoint <= 0xFFFF && font.updateFontMetric(metrics, static_cast<uint16_t>(codepoint));
}

}  // namespace

uint32_t decodeUtf8(const char *text, int32_t length, int32_t *consumed) {
  const uint8_t *s = reinterpret_cast<const uint8_t *>(text);
  const uint8_t lead = s[0];
  *consumed = 1;
  if (lead < 0x80) return lead;

  int32_t extra;
  uint32_t cp;
  uint32_t minimum;
  if ((lead & 0xE0) == 0xC0) {
    extra = 1;
    cp = lead & 0x1F;
    minimum = 0x80;
  } else if ((lead & 0xF0) == 0xE0) {
    extra = 2;
    cp = lead & 0x0F;
    minimum = 0x800;
  } else if ((lead & 0xF8) == 0xF0) {
    extra = 3;
    cp = lead & 0x07;
    minimum = 0x10000;
  } else {
    return replacementCharacter;
  }
  if (extra >= length) return replacementCharacter;
  for (int32_t i = 1; i <= extra; ++i) {
    if ((s[i] & 0xC0) != 0x80) return replacementCharacter;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return replacementCharacter;
  *consumed = extra + 1;
  return cp;
}

int32_t countCodepoints(const char *text, int32_t length) {
  int32_t count = 0;
  int32_t i = 0;
  while (i < length) {
    int32_t used;
    decodeUtf8(text + i, length - i, &used);
    i += used;
    ++count;
  }
  return count;
}

int32_t scaleFor(const lgfx::FontMetrics &metrics, uint16_t fontSize) {
  if (fontSize == 0 || metrics.height <= 0) return 1;
  const int32_t scale = fontSize / metrics.height;
  return scale < 1 ? 1 : scale;
}

bool loadGlyph(const FontSlot &slot, uint32_t codepoint, lgfx::FontMetrics *metrics, uint16_t *glyph) {
  if (!invisible(codepoint)) {
    if (tryLoad(*slot.font, codepoint, metrics)) {
      *glyph = static_cast<uint16_t>(codepoint);
      return true;
    }
    if (slot.fallback != 0 && tryLoad(*slot.font, slot.fallback, metrics)) {
      *glyph = static_cast<uint16_t>(slot.fallback);
      return true;
    }
  }
  metrics->x_advance = 0;
  metrics->width = 0;
  metrics->x_offset = 0;
  return false;
}

int32_t measureWidth(const FontSlot &slot, const char *text, int32_t length, int32_t scale, int32_t letterSpacing) {
  lgfx::FontMetrics metrics;
  slot.font->getDefaultMetric(&metrics);
  int32_t width = 0;
  int32_t i = 0;
  while (i < length) {
    int32_t used;
    const uint32_t cp = decodeUtf8(text + i, length - i, &used);
    i += used;
    uint16_t glyph;
    if (!loadGlyph(slot, cp, &metrics, &glyph)) continue;
    width += metrics.x_advance * scale + letterSpacing;
  }
  return width;
}

}  // namespace clay_lgfx
