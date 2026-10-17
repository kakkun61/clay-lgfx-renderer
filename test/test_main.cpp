// Host side unit tests for the parts of the renderer that do not need a
// display: the rasterisation math, text measurement, and text drawing, which
// is compared with LovyanGFX's own drawString() on off-screen sprites.

#define CLAY_IMPLEMENTATION
#include <clay.h>
#include <stdio.h>
#include <string.h>

#include <LovyanGFX.hpp>

#include "clay_lgfx/font_table.hpp"
#include "clay_lgfx/geometry.hpp"
#include "clay_lgfx/renderer.hpp"

using namespace clay_lgfx;

namespace {

int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                                                          \
  do {                                                                                                          \
    const long long va = static_cast<long long>(a), vb = static_cast<long long>(b);                             \
    if (va != vb) {                                                                                             \
      fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, va, vb); \
      ++failures;                                                                                               \
    }                                                                                                           \
  } while (0)

Clay_StringSlice slice(const char *s) {
  Clay_StringSlice r{};
  r.chars = s;
  r.length = static_cast<int32_t>(strlen(s));
  return r;
}

// Rasterises a callback based shape into a small bitmap, failing the test on
// any pixel that is painted twice.
struct Canvas {
  static constexpr int size = 64;
  int pixels[size][size] = {};
  void paint(int32_t x, int32_t y, int32_t w, int32_t h) {
    for (int32_t yy = y; yy < y + h; ++yy)
      for (int32_t xx = x; xx < x + w; ++xx) {
        CHECK(xx >= 0 && xx < size && yy >= 0 && yy < size);
        if (xx >= 0 && xx < size && yy >= 0 && yy < size) {
          CHECK_EQ(pixels[yy][xx], 0);
          pixels[yy][xx] = 1;
        }
      }
  }
  int count() const {
    int n = 0;
    for (auto &row : pixels)
      for (int p : row) n += p;
    return n;
  }
};

void testIntersect() {
  const Rect r = intersect(Rect{ 0, 0, 10, 10 }, Rect{ 5, 6, 10, 10 });
  CHECK_EQ(r.x, 5);
  CHECK_EQ(r.y, 6);
  CHECK_EQ(r.w, 5);
  CHECK_EQ(r.h, 4);
  CHECK(intersect(Rect{ 0, 0, 4, 4 }, Rect{ 10, 10, 4, 4 }).empty());
}

void testClampRadii() {
  const CornerRadii r = clampRadii(CornerRadii{ 20, 20, 20, 20 }, 10, 30);
  CHECK(r.topLeft <= 5.0f && r.topRight <= 5.0f);
  CHECK(r.isUniform());
  const CornerRadii n = clampRadii(CornerRadii{ -3, 2, 0, 0 }, 10, 10);
  CHECK(n.topLeft == 0 && n.topRight == 2);
}

void testSquareRectIsOneStrip() {
  int calls = 0;
  forEachRoundedRectStrip(Rect{ 1, 2, 10, 5 }, CornerRadii{}, [&](int32_t x, int32_t y, int32_t w, int32_t h) {
    ++calls;
    CHECK_EQ(x, 1);
    CHECK_EQ(y, 2);
    CHECK_EQ(w, 10);
    CHECK_EQ(h, 5);
  });
  CHECK_EQ(calls, 1);
}

void testRoundedRectIsSymmetric() {
  Canvas c;
  forEachRoundedRectStrip(Rect{ 0, 0, 20, 16 },
                          CornerRadii{ 6, 6, 6, 6 },
                          [&](int32_t x, int32_t y, int32_t w, int32_t h) { c.paint(x, y, w, h); });
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 20; ++x) {
      CHECK_EQ(c.pixels[y][x], c.pixels[y][19 - x]);
      CHECK_EQ(c.pixels[y][x], c.pixels[15 - y][x]);
    }
  CHECK_EQ(c.pixels[0][0], 0);  // The corner is cut off...
  CHECK_EQ(c.pixels[8][0], 1);  // ...but the middle of the edge is not.
  CHECK_EQ(c.pixels[0][10], 1);
  CHECK(c.count() < 20 * 16);
}

void testBorderMatchesWidths() {
  Canvas c;
  forEachBorderStrip(Rect{ 0, 0, 20, 20 }, CornerRadii{}, 1, 2, 3, 4, [&](int32_t x, int32_t y, int32_t w, int32_t h) {
    c.paint(x, y, w, h);
  });
  CHECK_EQ(c.count(), 20 * 20 - (20 - 1 - 2) * (20 - 3 - 4));
  CHECK_EQ(c.pixels[10][0], 1);
  CHECK_EQ(c.pixels[10][1], 0);
  CHECK_EQ(c.pixels[10][18], 1);
  CHECK_EQ(c.pixels[10][17], 0);
  CHECK_EQ(c.pixels[2][10], 1);
  CHECK_EQ(c.pixels[3][10], 0);
  CHECK_EQ(c.pixels[16][10], 1);
  CHECK_EQ(c.pixels[15][10], 0);
}

void testRoundedBorderStaysInsideFill() {
  Canvas border, fill;
  const Rect r{ 2, 2, 30, 24 };
  const CornerRadii radii{ 8, 8, 8, 8 };
  forEachBorderStrip(r, radii, 2, 2, 2, 2, [&](int32_t x, int32_t y, int32_t w, int32_t h) {
    border.paint(x, y, w, h);
  });
  forEachRoundedRectStrip(r, radii, [&](int32_t x, int32_t y, int32_t w, int32_t h) { fill.paint(x, y, w, h); });
  for (int y = 0; y < Canvas::size; ++y)
    for (int x = 0; x < Canvas::size; ++x)
      if (border.pixels[y][x]) CHECK_EQ(fill.pixels[y][x], 1);
  CHECK(border.count() > 0);
}

void testUtf8() {
  int32_t used;
  CHECK_EQ(decodeUtf8("A", 1, &used), 'A');
  CHECK_EQ(used, 1);
  CHECK_EQ(decodeUtf8("\xC3\xA9", 2, &used), 0xE9);
  CHECK_EQ(used, 2);
  CHECK_EQ(decodeUtf8("\xE3\x81\x82", 3, &used), 0x3042);
  CHECK_EQ(used, 3);
  CHECK_EQ(decodeUtf8("\xF0\x9F\x98\x80", 4, &used), 0x1F600);
  CHECK_EQ(used, 4);
  // Truncated and malformed sequences consume one byte at a time.
  CHECK_EQ(decodeUtf8("\xE3\x81", 2, &used), 0xFFFD);
  CHECK_EQ(used, 1);
  CHECK_EQ(decodeUtf8("\xC0\xAF", 2, &used), 0xFFFD);
  CHECK_EQ(countCodepoints("a\xE3\x81\x82z", 5), 3);
}

// A font with made up metrics, to pin down the measuring rules. Every glyph
// is 6 pixels wide, except 'W', which is 9, and U+3042, which is 12. It has
// no glyph for anything else, so the fallback rules come into play.
struct FakeFont : lgfx::IFont {
  void getDefaultMetric(lgfx::FontMetrics *metrics) const override {
    *metrics = lgfx::FontMetrics{};
    metrics->height = 10;
    metrics->y_advance = 12;
    metrics->baseline = 8;
    metrics->y_offset = -8;
  }
  bool updateFontMetric(lgfx::FontMetrics *metrics, uint16_t c) const override {
    int16_t advance;
    if (c == 'W') {
      advance = 9;
    } else if (c == 0x3042) {
      advance = 12;
    } else if (c >= 0x20 && c < 0x7F) {
      advance = 6;
    } else {
      return false;
    }
    metrics->width = metrics->x_advance = advance;
    metrics->x_offset = 0;
    return true;
  }
  size_t drawChar(lgfx::LGFXBase *, int32_t, int32_t, uint16_t, const lgfx::TextStyle *, lgfx::FontMetrics *,
                  int32_t &) const override {
    return 0;
  }
};

const FakeFont fakeFont;

void testFontTable() {
  FontTable fonts;
  CHECK(fonts.get(0) == nullptr);
  CHECK(fonts.set(0, &fakeFont));
  CHECK(!fonts.set(FontTable::capacity, &fakeFont));
  CHECK(fonts.get(5) != nullptr && fonts.get(5)->font == &fakeFont);  // Falls back to font 0.

  Clay_Dimensions d = fonts.measure(slice("Hello"), 0, 0, 0);
  CHECK_EQ(d.width, 30);
  CHECK_EQ(d.height, 10);
  d = fonts.measure(slice("Hi"), 0, 20, 1);  // fontSize 20 doubles a 10 pixel font.
  CHECK_EQ(d.width, 2 * (12 + 1));
  CHECK_EQ(d.height, 20);
  CHECK_EQ(fonts.measure(slice("W"), 0, 0, 0).width, 9);
  // Measuring word by word adds up to the whole line.
  const float whole = fonts.measure(slice("ab cd"), 0, 0, 2).width;
  const float parts = fonts.measure(slice("ab "), 0, 0, 2).width + fonts.measure(slice("cd"), 0, 0, 2).width;
  CHECK_EQ(whole, parts);
}

void testScale() {
  lgfx::FontMetrics m{};
  m.height = 13;
  CHECK_EQ(scaleFor(m, 0), 1);
  CHECK_EQ(scaleFor(m, 12), 1);
  CHECK_EQ(scaleFor(m, 26), 2);
  CHECK_EQ(scaleFor(m, 38), 2);
}

void testFallback() {
  const FontSlot withQuestionMark{ &fakeFont, '?' };
  const FontSlot withNothing{ &fakeFont, 0 };
  // U+3044 is missing and drawn as '?'; U+3042 is there.
  CHECK_EQ(measureWidth(withQuestionMark, "\xE3\x81\x84", 3, 1, 0), 6);
  CHECK_EQ(measureWidth(withQuestionMark, "\xE3\x81\x82", 3, 1, 0), 12);
  CHECK_EQ(measureWidth(withNothing, "a\xE3\x81\x84z", 5, 1, 0), 12);
  // Characters above U+FFFF never reach the font.
  CHECK_EQ(measureWidth(withQuestionMark, "\xF0\x9F\x98\x80", 4, 1, 0), 6);
  // Control characters and variation selectors take no space, not even letter spacing.
  CHECK_EQ(measureWidth(withQuestionMark, "a\tb\xEF\xB8\x8F", 6, 1, 3), 2 * (6 + 3));

  lgfx::FontMetrics m;
  fakeFont.getDefaultMetric(&m);
  uint16_t glyph = 0;
  CHECK(loadGlyph(withQuestionMark, 0x3044, &m, &glyph));
  CHECK_EQ(glyph, '?');
  CHECK(!loadGlyph(withNothing, 0x3044, &m, &glyph));
  CHECK_EQ(m.x_advance, 0);
}

// Width LovyanGFX advances by when it draws the string itself.
int32_t lovyanAdvance(const lgfx::IFont &font, const char *text, float size) {
  lgfx::LGFX_Sprite sprite;
  sprite.setColorDepth(16);
  sprite.createSprite(400, 60);
  sprite.setFont(&font);
  sprite.setTextSize(size);
  return static_cast<int32_t>(sprite.drawString(text, 0, 0));
}

void testMeasureMatchesLovyanGfx() {
  struct Case {
    const lgfx::IFont *font;
    const char *text;
  };
  const Case cases[] = {
    { &lgfx::fonts::DejaVu12, "Hello, world" },
    { &lgfx::fonts::FreeSans9pt7b, "Proportional text" },
    { &lgfx::fonts::lgfxJapanGothic_12, "日本語とASCII" },
  };
  for (const Case &c : cases) {
    const FontSlot slot{ c.font, '?' };
    const int32_t length = static_cast<int32_t>(strlen(c.text));
    CHECK_EQ(measureWidth(slot, c.text, length, 1, 0), lovyanAdvance(*c.font, c.text, 1));
    CHECK_EQ(measureWidth(slot, c.text, length, 2, 0), lovyanAdvance(*c.font, c.text, 2));
  }
}

// Draws one Clay text command with the renderer and the same text with
// LovyanGFX's drawString(), and compares the two pictures pixel by pixel.
void checkTextMatchesDrawString(const lgfx::IFont &font, const char *text, uint16_t fontSize) {
  FontTable fonts;
  fonts.set(0, &font);
  const Renderer renderer(fonts);
  lgfx::FontMetrics metrics;
  font.getDefaultMetric(&metrics);
  const int32_t scale = scaleFor(metrics, fontSize);

  Clay_RenderCommand command;
  memset(&command, 0, sizeof(command));
  command.commandType = CLAY_RENDER_COMMAND_TYPE_TEXT;
  command.boundingBox = Clay_BoundingBox{ 5, 7, 300, static_cast<float>(metrics.height * scale) };
  command.renderData.text.stringContents = slice(text);
  command.renderData.text.textColor = Clay_Color{ 255, 255, 255, 255 };
  command.renderData.text.fontSize = fontSize;
  Clay_RenderCommandArray commands{ 1, 1, &command };

  lgfx::LGFX_Sprite ours, theirs;
  for (lgfx::LGFX_Sprite *s : { &ours, &theirs }) {
    s->setColorDepth(16);
    s->createSprite(320, 80);
    s->fillScreen(TFT_BLACK);
  }
  renderer.render(ours, commands);
  theirs.setFont(&font);
  theirs.setTextSize(static_cast<float>(scale));
  theirs.setTextColor(TFT_WHITE);
  theirs.drawString(text, 5, 7);

  int differences = 0, lit = 0;
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 320; ++x) {
      differences += ours.readPixel(x, y) != theirs.readPixel(x, y);
      lit += ours.readPixel(x, y) != 0;
    }
  CHECK_EQ(differences, 0);
  CHECK(lit > 0);
}

void testDrawingMatchesLovyanGfx() {
  checkTextMatchesDrawString(lgfx::fonts::DejaVu12, "Hello, world", 0);
  checkTextMatchesDrawString(lgfx::fonts::FreeSans9pt7b, "Proportional", 0);
  checkTextMatchesDrawString(lgfx::fonts::FreeSans9pt7b, "Scaled", 40);
  checkTextMatchesDrawString(lgfx::fonts::lgfxJapanGothic_12, "日本語とASCII", 0);
}

}  // namespace

int main() {
  testIntersect();
  testClampRadii();
  testSquareRectIsOneStrip();
  testRoundedRectIsSymmetric();
  testBorderMatchesWidths();
  testRoundedBorderStaysInsideFill();
  testUtf8();
  testFontTable();
  testScale();
  testFallback();
  testMeasureMatchesLovyanGfx();
  testDrawingMatchesLovyanGfx();
  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
