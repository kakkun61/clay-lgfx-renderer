#include "renderer.hpp"

#include <math.h>

namespace clay_lgfx {

namespace {

uint8_t channel(float v) {
  if (!(v > 0)) return 0;  // Also catches NaN.
  if (v >= 255) return 255;
  return static_cast<uint8_t>(v + 0.5f);
}

int32_t roundToInt(float v) { return static_cast<int32_t>(floorf(v + 0.5f)); }

// Rounds both edges rather than the size, so that adjacent boxes stay
// adjacent after rounding instead of leaving gaps or overlaps.
Rect toRect(const Clay_BoundingBox &box, int32_t originX, int32_t originY) {
  const int32_t x0 = roundToInt(box.x);
  const int32_t y0 = roundToInt(box.y);
  const int32_t x1 = roundToInt(box.x + box.width);
  const int32_t y1 = roundToInt(box.y + box.height);
  return Rect{ x0 - originX, y0 - originY, x1 - x0, y1 - y0 };
}

CornerRadii toRadii(const Clay_CornerRadius &r) {
  return CornerRadii{ r.topLeft, r.topRight, r.bottomLeft, r.bottomRight };
}

}  // namespace

lgfx::rgb888_t toLgfxColor(Clay_Color color) {
  return lgfx::rgb888_t(channel(color.r), channel(color.g), channel(color.b));
}

// Everything that only lives for the duration of one render() call.
struct Renderer::Pass {
  static constexpr int maxClipDepth = 16;

  const Renderer &renderer;
  lgfx::LovyanGFX &target;
  int32_t originX;
  int32_t originY;

  Rect baseClip;
  Rect clipStack[maxClipDepth];
  int clipDepth = 0;

  bool overlayActive = false;
  Clay_Color overlay{ 0, 0, 0, 0 };

  Pass(const Renderer &r, lgfx::LovyanGFX &t, int32_t ox, int32_t oy)
    : renderer(r), target(t), originX(ox), originY(oy) {
    int32_t x, y, w, h;
    target.getClipRect(&x, &y, &w, &h);
    baseClip = Rect{ x, y, w, h };
  }

  ~Pass() { target.setClipRect(baseClip.x, baseClip.y, baseClip.w, baseClip.h); }

  const Rect &currentClip() const { return clipDepth > 0 ? clipStack[clipDepth - 1] : baseClip; }

  void applyClip() {
    const Rect &c = currentClip();
    if (c.empty()) {
      // LovyanGFX treats a zero sized clip rectangle as "no clipping",
      // so an empty clip is handled by skipping draws in visible().
      return;
    }
    target.setClipRect(c.x, c.y, c.w, c.h);
  }

  bool visible(const Rect &r) const {
    const Rect &c = currentClip();
    return !c.empty() && !intersect(r, c).empty();
  }

  lgfx::rgb888_t color(Clay_Color c) const {
    if (overlayActive) {
      const float t = overlay.a / 255.0f;
      c.r += (overlay.r - c.r) * t;
      c.g += (overlay.g - c.g) * t;
      c.b += (overlay.b - c.b) * t;
    }
    return toLgfxColor(c);
  }

  void pushClip(const Rect &r) {
    if (clipDepth >= maxClipDepth) {
      // Deeper nesting than we track: keep clipping to the innermost
      // known rectangle, which errs on the side of drawing less.
      ++clipDepth;
      return;
    }
    clipStack[clipDepth] = intersect(r, currentClip());
    ++clipDepth;
    applyClip();
  }

  void popClip() {
    if (clipDepth == 0) return;
    --clipDepth;
    if (clipDepth >= maxClipDepth) return;
    applyClip();
  }

  void fillRect(const Rect &r, const CornerRadii &radii, Clay_Color c) {
    if (c.a <= 0 || !visible(r)) return;
    const lgfx::rgb888_t col = color(c);
    if (radii.isZero()) {
      target.fillRect(r.x, r.y, r.w, r.h, col);
      return;
    }
    forEachRoundedRectStrip(r, radii, [&](int32_t x, int32_t y, int32_t w, int32_t h) {
      target.fillRect(x, y, w, h, col);
    });
  }

  void drawBorder(const Rect &r, const Clay_BorderRenderData &b) {
    if (b.color.a <= 0 || !visible(r)) return;
    const lgfx::rgb888_t col = color(b.color);
    forEachBorderStrip(r,
                       toRadii(b.cornerRadius),
                       b.width.left,
                       b.width.right,
                       b.width.top,
                       b.width.bottom,
                       [&](int32_t x, int32_t y, int32_t w, int32_t h) { target.fillRect(x, y, w, h, col); });
  }

  void drawText(const Rect &r, const Clay_TextRenderData &t) {
    if (t.textColor.a <= 0 || !visible(r)) return;
    const FontSlot *slot = renderer.fonts.get(t.fontId);
    if (!slot) return;
    lgfx::FontMetrics metrics;
    slot->font->getDefaultMetric(&metrics);
    const int32_t scale = scaleFor(metrics, t.fontSize);
    const int32_t height = metrics.height * scale;
    const Rect &clip = currentClip();

    // Clay makes the box as tall as the line height, which may exceed the font's.
    const int32_t top = r.y + (r.h - height) / 2;
    if (top >= clip.bottom() || top + height <= clip.y) return;

    // The glyphs themselves are drawn by the font, positioned the way
    // LGFXBase::drawString() does for text whose top left corner is given.
    const lgfx::rgb888_t c = color(t.textColor);
    lgfx::TextStyle style;
    style.fore_rgb888 = style.back_rgb888 =
      (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
    style.size_x = style.size_y = static_cast<float>(scale);
    const int32_t y = top - metrics.y_offset * scale;

    int32_t pen = r.x;
    int32_t i = 0;
    const char *chars = t.stringContents.chars;
    const int32_t length = t.stringContents.length;
    while (i < length && pen < clip.right()) {
      int32_t used;
      const uint32_t cp = decodeUtf8(chars + i, length - i, &used);
      i += used;
      uint16_t glyph;
      if (!loadGlyph(*slot, cp, &metrics, &glyph)) continue;
      const int32_t advance = metrics.x_advance * scale;
      if (pen + advance > clip.x) {
        int32_t filled = 0;
        slot->font->drawChar(&target, pen, y, glyph, &style, &metrics, filled);
      }
      pen += advance + t.letterSpacing;
    }
  }

  void drawImage(const Rect &r, const Clay_ImageRenderData &data) {
    const Image *image = static_cast<const Image *>(data.imageData);
    if (!image || !image->pixels || image->width == 0 || image->height == 0 || !visible(r)) return;
    const lgfx::rgb565_t *pixels = reinterpret_cast<const lgfx::rgb565_t *>(image->pixels);
    if (r.w == image->width && r.h == image->height) {
      target.pushImage(r.x, r.y, image->width, image->height, pixels);
    } else {
      target.pushImageRotateZoom(static_cast<float>(r.x),
                                 static_cast<float>(r.y),
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 static_cast<float>(r.w) / image->width,
                                 static_cast<float>(r.h) / image->height,
                                 image->width,
                                 image->height,
                                 pixels);
    }
  }

  void drawCustom(const Rect &r, const Clay_RenderCommand &command) {
    const Clay_CustomRenderData &data = command.renderData.custom;
    // Like other elements, a custom element may carry a background.
    fillRect(r, toRadii(data.cornerRadius), data.backgroundColor);
    if (renderer.customHandler && visible(r)) {
      renderer.customHandler(target, command, r, renderer.customUserData);
      applyClip();  // The handler may have changed the clip rectangle.
    }
  }

  void run(const Clay_RenderCommandArray &commands) {
    for (int32_t i = 0; i < commands.length; ++i) {
      const Clay_RenderCommand &command = commands.internalArray[i];
      const Rect r = toRect(command.boundingBox, originX, originY);
      switch (command.commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
          fillRect(r, toRadii(command.renderData.rectangle.cornerRadius), command.renderData.rectangle.backgroundColor);
          break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
          drawBorder(r, command.renderData.border);
          break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
          drawText(r, command.renderData.text);
          break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
          drawImage(r, command.renderData.image);
          break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
          pushClip(r);
          break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
          popClip();
          break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
          overlayActive = true;
          overlay = command.renderData.overlayColor.color;
          break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
          overlayActive = false;
          break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
          drawCustom(r, command);
          break;
        case CLAY_RENDER_COMMAND_TYPE_NONE:
        default:
          break;
      }
    }
  }
};

void Renderer::render(lgfx::LovyanGFX &target, const Clay_RenderCommandArray &commands, int32_t originX,
                      int32_t originY) const {
  Pass pass(*this, target, originX, originY);
  target.startWrite();
  pass.run(commands);
  target.endWrite();
}

bool BandedPresenter::begin(int32_t bandHeight, lgfx::color_depth_t depth) {
  const int32_t height = display.height();
  if (bandHeight > height) bandHeight = height;
  if (bandHeight < 1) bandHeight = 1;
  band.setColorDepth(depth);
  hasLastHash = false;
  return band.createSprite(display.width(), bandHeight) != nullptr;
}

bool BandedPresenter::present(const Clay_RenderCommandArray &commands) {
  const uint32_t hash = hashRenderCommands(commands);
  if (hasLastHash && hash == lastHash) return false;
  if (band.width() == 0 || band.height() == 0) return false;

  const lgfx::rgb888_t clear = toLgfxColor(clearColor);
  const int32_t bandHeight = band.height();
  const int32_t height = display.height();
  display.startWrite();
  for (int32_t y = 0; y < height; y += bandHeight) {
    band.fillScreen(clear);
    renderer.render(band, commands, 0, y);
    // The last band may stick out below the display; LovyanGFX clips it.
    band.pushSprite(&display, 0, y);
  }
  display.endWrite();

  lastHash = hash;
  hasLastHash = true;
  return true;
}

namespace {

// 32 bit FNV-1a. Fast, tiny and good enough to tell frames apart.
struct Fnv1a {
  uint32_t h = 2166136261u;
  void bytes(const void *data, size_t size) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
      h ^= p[i];
      h *= 16777619u;
    }
  }
  template <typename T>
  void value(const T &v) {
    bytes(&v, sizeof(v));
  }
  void color(const Clay_Color &c) {
    value(c.r);
    value(c.g);
    value(c.b);
    value(c.a);
  }
  void radius(const Clay_CornerRadius &r) {
    value(r.topLeft);
    value(r.topRight);
    value(r.bottomLeft);
    value(r.bottomRight);
  }
};

}  // namespace

uint32_t hashRenderCommands(const Clay_RenderCommandArray &commands) {
  // Fields are hashed one by one instead of hashing whole structs, because
  // padding bytes and the unused parts of the render data union may hold
  // leftovers from earlier frames.
  Fnv1a f;
  f.value(commands.length);
  for (int32_t i = 0; i < commands.length; ++i) {
    const Clay_RenderCommand &c = commands.internalArray[i];
    const uint8_t type = static_cast<uint8_t>(c.commandType);
    f.value(type);
    f.value(c.boundingBox.x);
    f.value(c.boundingBox.y);
    f.value(c.boundingBox.width);
    f.value(c.boundingBox.height);
    const Clay_RenderData &d = c.renderData;
    switch (c.commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
        f.color(d.rectangle.backgroundColor);
        f.radius(d.rectangle.cornerRadius);
        break;
      case CLAY_RENDER_COMMAND_TYPE_BORDER:
        f.color(d.border.color);
        f.radius(d.border.cornerRadius);
        f.value(d.border.width.left);
        f.value(d.border.width.right);
        f.value(d.border.width.top);
        f.value(d.border.width.bottom);
        break;
      case CLAY_RENDER_COMMAND_TYPE_TEXT:
        f.color(d.text.textColor);
        f.value(d.text.fontId);
        f.value(d.text.fontSize);
        f.value(d.text.letterSpacing);
        f.value(d.text.stringContents.length);
        f.bytes(d.text.stringContents.chars, static_cast<size_t>(d.text.stringContents.length));
        break;
      case CLAY_RENDER_COMMAND_TYPE_IMAGE:
        f.color(d.image.backgroundColor);
        f.radius(d.image.cornerRadius);
        f.value(d.image.imageData);
        break;
      case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
        f.color(d.custom.backgroundColor);
        f.radius(d.custom.cornerRadius);
        f.value(d.custom.customData);
        break;
      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
        f.value(d.clip.horizontal);
        f.value(d.clip.vertical);
        break;
      case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
      case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
        f.color(d.overlayColor.color);
        break;
      default:
        break;
    }
  }
  return f.h;
}

}  // namespace clay_lgfx
