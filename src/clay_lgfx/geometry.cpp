#include "geometry.hpp"

#include <math.h>

namespace clay_lgfx {

namespace {

int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
float positive(float v) { return v > 0 ? v : 0.0f; }

// Horizontal distance between the edge of the bounding box and the edge of a
// corner arc of radius `r`, measured on a row whose centre lies `d` pixels
// away from the horizontal edge the corner touches.
float cornerInset(float r, float d) {
  if (r <= 0 || d >= r) return 0;
  const float dy = r - d;
  return r - sqrtf(r * r - dy * dy);
}

}  // namespace

Rect intersect(const Rect &a, const Rect &b) {
  const int32_t x0 = max32(a.x, b.x);
  const int32_t y0 = max32(a.y, b.y);
  const int32_t x1 = min32(a.right(), b.right());
  const int32_t y1 = min32(a.bottom(), b.bottom());
  return Rect{ x0, y0, x1 - x0, y1 - y0 };
}

CornerRadii clampRadii(const CornerRadii &radii, int32_t width, int32_t height) {
  CornerRadii r{ positive(radii.topLeft),
                 positive(radii.topRight),
                 positive(radii.bottomLeft),
                 positive(radii.bottomRight) };
  if (width <= 0 || height <= 0) return CornerRadii{};
  const float w = static_cast<float>(width);
  const float h = static_cast<float>(height);
  float scale = 1.0f;
  auto limit = [&scale](float sum, float available) {
    if (sum > available) {
      const float s = available / sum;
      if (s < scale) scale = s;
    }
  };
  limit(r.topLeft + r.topRight, w);
  limit(r.bottomLeft + r.bottomRight, w);
  limit(r.topLeft + r.bottomLeft, h);
  limit(r.topRight + r.bottomRight, h);
  if (scale < 1.0f) {
    r.topLeft *= scale;
    r.topRight *= scale;
    r.bottomLeft *= scale;
    r.bottomRight *= scale;
  }
  return r;
}

Span roundedRowSpan(const Rect &rect, const CornerRadii &radii, int32_t row) {
  if (rect.empty() || row < rect.y || row >= rect.bottom()) return Span{ 0, -1 };
  const float fromTop = static_cast<float>(row - rect.y) + 0.5f;
  const float fromBottom = static_cast<float>(rect.bottom() - row) - 0.5f;
  const float leftInset = cornerInset(radii.topLeft, fromTop) + cornerInset(radii.bottomLeft, fromBottom);
  const float rightInset = cornerInset(radii.topRight, fromTop) + cornerInset(radii.bottomRight, fromBottom);
  // Round so that a pixel is covered when its centre is inside the arc.
  const int32_t l = static_cast<int32_t>(floorf(leftInset + 0.5f));
  const int32_t r = static_cast<int32_t>(floorf(rightInset + 0.5f));
  return Span{ rect.x + l, rect.right() - r };
}

}  // namespace clay_lgfx
