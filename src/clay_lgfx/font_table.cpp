#include "font_table.hpp"

namespace clay_lgfx {

bool FontTable::set(uint16_t fontId, const Font *font, uint32_t fallback) {
  if (fontId >= capacity) return false;
  slots[fontId] = FontSlot{ font, fallback };
  return true;
}

const FontSlot *FontTable::get(uint16_t fontId) const {
  if (fontId < capacity && slots[fontId].font) return &slots[fontId];
  return slots[0].font ? &slots[0] : nullptr;
}

Clay_Dimensions FontTable::measure(Clay_StringSlice text, uint16_t fontId, uint16_t fontSize,
                                   uint16_t letterSpacing) const {
  const FontSlot *slot = get(fontId);
  if (!slot) return Clay_Dimensions{ 0, 0 };
  lgfx::FontMetrics metrics;
  slot->font->getDefaultMetric(&metrics);
  const int32_t scale = scaleFor(metrics, fontSize);
  return Clay_Dimensions{ static_cast<float>(measureWidth(*slot, text.chars, text.length, scale, letterSpacing)),
                          static_cast<float>(metrics.height * scale) };
}

void FontTable::attach() const {
  // Clay's callback takes a mutable pointer but the table is never modified through it.
  Clay_SetMeasureTextFunction(&FontTable::measureCallback, const_cast<FontTable *>(this));
}

Clay_Dimensions FontTable::measureCallback(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
  const FontTable *self = static_cast<const FontTable *>(userData);
  return self->measure(text, config->fontId, config->fontSize, config->letterSpacing);
}

}  // namespace clay_lgfx
