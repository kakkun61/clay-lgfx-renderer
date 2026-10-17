/**
 * @file geometry.hpp
 * @brief Integer geometry helpers used to rasterise Clay's rounded shapes.
 *
 * LovyanGFX can only draw rounded rectangles whose four corners share one
 * radius, and it has no notion of borders with a different width on each
 * side. Clay allows both, so the renderer rasterises those shapes itself, one
 * horizontal span per row. The math lives here, free of any LovyanGFX
 * dependency, so that it can be unit tested on the host.
 */
#pragma once

#include <stdint.h>

namespace clay_lgfx {

/** @brief An axis-aligned rectangle in whole pixels. */
struct Rect {
  int32_t x = 0;  ///< Left edge.
  int32_t y = 0;  ///< Top edge.
  int32_t w = 0;  ///< Width. A rectangle with a width of zero or less is empty.
  int32_t h = 0;  ///< Height. A rectangle with a height of zero or less is empty.

  /** @brief Returns true when the rectangle covers no pixels at all. */
  bool empty() const { return w <= 0 || h <= 0; }
  /** @brief One past the rightmost column. */
  int32_t right() const { return x + w; }
  /** @brief One past the bottom row. */
  int32_t bottom() const { return y + h; }
};

/** @brief Radius of each corner of a rounded rectangle, in pixels. */
struct CornerRadii {
  float topLeft = 0;      ///< Radius of the top left corner.
  float topRight = 0;     ///< Radius of the top right corner.
  float bottomLeft = 0;   ///< Radius of the bottom left corner.
  float bottomRight = 0;  ///< Radius of the bottom right corner.

  /** @brief Returns true when every corner is square. */
  bool isZero() const { return topLeft <= 0 && topRight <= 0 && bottomLeft <= 0 && bottomRight <= 0; }
  /** @brief Returns true when all four corners share the same radius. */
  bool isUniform() const { return topLeft == topRight && topLeft == bottomLeft && topLeft == bottomRight; }
};

/**
 * @brief A run of pixels on one row, covering the columns `[left, right)`.
 *
 * A span whose @ref right is not greater than its @ref left is empty.
 */
struct Span {
  int32_t left = 0;   ///< First column covered by the span.
  int32_t right = 0;  ///< One past the last column covered by the span.

  /** @brief Returns true when the span covers no pixels. */
  bool empty() const { return right <= left; }
};

/**
 * @brief Returns the intersection of two rectangles.
 *
 * The result is empty, though not necessarily zero sized, when the rectangles
 * do not overlap.
 */
Rect intersect(const Rect &a, const Rect &b);

/**
 * @brief Shrinks the radii so that they fit inside a rectangle.
 *
 * Clay does not stop a layout from asking for a corner radius larger than the
 * element itself. Like CSS, the radii are scaled down together until adjacent
 * corners no longer overlap. Negative radii are treated as zero.
 */
CornerRadii clampRadii(const CornerRadii &radii, int32_t width, int32_t height);

/**
 * @brief Computes which pixels of a row belong to a rounded rectangle.
 *
 * @param rect   The rectangle being filled.
 * @param radii  Corner radii, which are expected to have gone through clampRadii().
 * @param row    The absolute y coordinate of the row.
 * @return The covered columns, or an empty span when the row is outside @p rect.
 *
 * A pixel is considered inside when its centre lies inside the shape, which
 * gives symmetric corners without any antialiasing.
 */
Span roundedRowSpan(const Rect &rect, const CornerRadii &radii, int32_t row);

/**
 * @brief Calls @p emit with the rectangles that together cover a rounded rectangle.
 *
 * Consecutive rows that produce the same span are merged, so a rectangle with
 * square corners costs a single call and a rounded one costs roughly two
 * calls per corner row. Drawing whole rectangles instead of single lines
 * matters a lot on SPI panels, where every call carries a fixed overhead.
 *
 * @tparam Emit A callable taking `(int32_t x, int32_t y, int32_t w, int32_t h)`.
 */
template <typename Emit>
void forEachRoundedRectStrip(const Rect &rect, const CornerRadii &radii, Emit &&emit) {
  if (rect.empty()) return;
  const CornerRadii r = clampRadii(radii, rect.w, rect.h);
  int32_t runStart = rect.y;
  Span runSpan = roundedRowSpan(rect, r, rect.y);
  for (int32_t row = rect.y + 1; row <= rect.bottom(); ++row) {
    const Span span = row < rect.bottom() ? roundedRowSpan(rect, r, row) : Span{ 0, -1 };
    if (span.left == runSpan.left && span.right == runSpan.right) continue;
    if (!runSpan.empty()) emit(runSpan.left, runStart, runSpan.right - runSpan.left, row - runStart);
    runStart = row;
    runSpan = span;
  }
}

/**
 * @brief Calls @p emit with the rectangles that make up a rounded border.
 *
 * The border is the area between the outer rounded rectangle @p outer and an
 * inner one obtained by moving each edge inwards by the corresponding width.
 * The inner corners get correspondingly smaller radii, so a border of
 * constant width keeps a constant thickness around its corners.
 *
 * @param outer   Outer edge of the border.
 * @param radii   Radii of the outer corners.
 * @param left    Width of the left edge.
 * @param right   Width of the right edge.
 * @param top     Width of the top edge.
 * @param bottom  Width of the bottom edge.
 * @param emit    A callable taking `(int32_t x, int32_t y, int32_t w, int32_t h)`.
 */
template <typename Emit>
void forEachBorderStrip(const Rect &outer, const CornerRadii &radii, int32_t left, int32_t right, int32_t top,
                        int32_t bottom, Emit &&emit) {
  if (outer.empty()) return;
  const CornerRadii ro = clampRadii(radii, outer.w, outer.h);
  const Rect inner{ outer.x + left, outer.y + top, outer.w - left - right, outer.h - top - bottom };
  auto shrink = [](float r, int32_t a, int32_t b) {
    const float v = r - static_cast<float>(a > b ? a : b);
    return v > 0 ? v : 0.0f;
  };
  const CornerRadii ri = clampRadii(CornerRadii{ shrink(ro.topLeft, left, top),
                                                 shrink(ro.topRight, right, top),
                                                 shrink(ro.bottomLeft, left, bottom),
                                                 shrink(ro.bottomRight, right, bottom) },
                                    inner.w,
                                    inner.h);

  // A row of a border consists of at most two segments. Runs of rows with
  // identical segments are merged exactly like forEachRoundedRectStrip does.
  struct Segments {
    Span a, b;
    bool operator==(const Segments &o) const {
      return a.left == o.a.left && a.right == o.a.right && b.left == o.b.left && b.right == o.b.right;
    }
  };
  auto segmentsAt = [&](int32_t row) {
    Segments s;
    const Span o = roundedRowSpan(outer, ro, row);
    const Span i = inner.empty() ? Span{ 0, -1 } : roundedRowSpan(inner, ri, row);
    if (i.empty()) {
      s.a = o;
      s.b = Span{ 0, -1 };
    } else {
      s.a = Span{ o.left, i.left };
      s.b = Span{ i.right, o.right };
    }
    return s;
  };
  auto flush = [&](const Segments &s, int32_t y, int32_t h) {
    if (!s.a.empty()) emit(s.a.left, y, s.a.right - s.a.left, h);
    if (!s.b.empty()) emit(s.b.left, y, s.b.right - s.b.left, h);
  };

  int32_t runStart = outer.y;
  Segments run = segmentsAt(outer.y);
  for (int32_t row = outer.y + 1; row < outer.bottom(); ++row) {
    const Segments s = segmentsAt(row);
    if (s == run) continue;
    flush(run, runStart, row - runStart);
    runStart = row;
    run = s;
  }
  flush(run, runStart, outer.bottom() - runStart);
}

}  // namespace clay_lgfx
