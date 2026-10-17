/**
 * @file renderer.hpp
 * @brief Draws Clay render commands onto any LovyanGFX surface.
 */
#pragma once

#include <clay.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

#include "font_table.hpp"
#include "geometry.hpp"

namespace clay_lgfx {

/**
 * @brief A bitmap that can be shown by a Clay image element.
 *
 * Pass a pointer to one of these as the `imageData` of an element's
 * `.image` configuration. The renderer stretches the image to fill the
 * element's bounding box, so giving the element the same size as the image
 * draws it pixel for pixel.
 */
struct Image {
  uint16_t width = 0;                ///< Width in pixels.
  uint16_t height = 0;               ///< Height in pixels.
  const uint16_t *pixels = nullptr;  ///< `width * height` RGB565 pixels in native byte order, row by row.
};

/**
 * @brief Converts a Clay colour to the colour type LovyanGFX draws with.
 *
 * Clay colours use floats in the range 0 to 255. Out of range values are clamped.
 */
lgfx::rgb888_t toLgfxColor(Clay_Color color);

/**
 * @brief Draws custom elements on behalf of the renderer.
 *
 * @param target    The surface being drawn on.
 * @param command   The CLAY_RENDER_COMMAND_TYPE_CUSTOM command to draw.
 * @param bounds    The command's bounding box translated into @p target's coordinates.
 * @param userData  The pointer given to Renderer::setCustomDrawHandler().
 *
 * The clip rectangle of @p target is already set up when the handler runs.
 */
using CustomDrawHandler = void (*)(lgfx::LovyanGFX &target, const Clay_RenderCommand &command, const Rect &bounds,
                                   void *userData);

/**
 * @brief Draws a Clay_RenderCommandArray using LovyanGFX primitives.
 *
 * The renderer handles every command type Clay produces:
 *
 * - Rectangles, with an individual radius for every corner.
 * - Borders, with an individual width for every side and rounded corners.
 * - Text, using the monospaced bitmap fonts of a FontTable.
 * - Images described by an Image struct.
 * - Scissor commands, which become LovyanGFX clip rectangles. Nested scissors
 *   are intersected with each other.
 * - Overlay colours, which are blended into every colour drawn while they are active.
 * - Custom elements, which are forwarded to a CustomDrawHandler.
 *
 * Colours are drawn opaque. LovyanGFX cannot read back pixels from most
 * panels, so true alpha blending is not possible in general. Commands whose
 * colour has an alpha of zero are skipped, as Clay uses that to mean
 * "nothing to draw".
 *
 * The renderer keeps no state between calls to render(), so a single instance
 * can draw to several surfaces, and it can be reused across frames freely.
 */
class Renderer {
 public:
  /**
     * @brief Creates a renderer that draws text with @p fonts.
     *
     * The font table is referenced, not copied, and must outlive the renderer.
     */
  explicit Renderer(const FontTable &fonts) : fonts(fonts) {}

  /** @brief Installs the function that draws custom elements. Pass nullptr to ignore them. */
  void setCustomDrawHandler(CustomDrawHandler handler, void *userData) {
    customHandler = handler;
    customUserData = userData;
  }

  /**
     * @brief Draws every command of @p commands onto @p target.
     *
     * @param target   A display, a sprite, or anything else derived from lgfx::LovyanGFX.
     * @param commands The array returned by Clay_EndLayout().
     * @param originX  Layout x coordinate that maps to column 0 of @p target.
     * @param originY  Layout y coordinate that maps to row 0 of @p target.
     *
     * The origin makes it possible to render the layout in pieces, for
     * example into a sprite that only holds a horizontal band of the screen,
     * which is what BandedPresenter does. The clip rectangle of @p target is
     * respected and restored before returning.
     */
  void render(lgfx::LovyanGFX &target, const Clay_RenderCommandArray &commands, int32_t originX = 0,
              int32_t originY = 0) const;

 private:
  struct Pass;

  const FontTable &fonts;
  CustomDrawHandler customHandler = nullptr;
  void *customUserData = nullptr;
};

/**
 * @brief Pushes a layout to a display through a small off-screen buffer.
 *
 * Clay rebuilds the whole frame every time. Drawing that straight to a panel
 * makes it flicker, while a sprite covering the whole screen takes 150 KB at
 * 320x240 in 16 bit colour, more than a board without PSRAM can comfortably
 * spare. This class allocates a sprite that holds only a horizontal band of
 * the screen, renders the layout into it one band at a time and pushes each
 * band to the display. Every pixel is written exactly once per frame, so
 * there is no flicker, at the cost of walking the command list once per band.
 *
 * It also remembers a hash of the last frame it drew and skips frames whose
 * commands did not change, which keeps an idle UI from keeping the SPI bus busy.
 */
class BandedPresenter {
 public:
  /** @brief Creates a presenter. Nothing is allocated until begin() is called. */
  BandedPresenter(lgfx::LGFX_Device &display, const Renderer &renderer) : display(display), renderer(renderer) {}

  /**
     * @brief Allocates the band buffer.
     *
     * @param bandHeight  Rows per band. Larger bands mean fewer passes over the
     *                    command list but more memory. A value larger than the
     *                    display is reduced to the display height.
     * @param depth       Colour depth of the buffer.
     * @return false when the buffer could not be allocated.
     */
  bool begin(int32_t bandHeight, lgfx::color_depth_t depth = lgfx::rgb565_2Byte);

  /** @brief Sets the colour every band is cleared to before rendering. */
  void setClearColor(Clay_Color color) { clearColor = color; }

  /**
     * @brief Draws @p commands unless they are identical to the previous frame.
     * @return true when something was drawn.
     */
  bool present(const Clay_RenderCommandArray &commands);

  /**
     * @brief Forces the next call to present() to draw.
     *
     * Use this when something the hash cannot see has changed, such as the
     * contents of an Image or the output of a custom draw handler.
     */
  void invalidate() { hasLastHash = false; }

 private:
  lgfx::LGFX_Device &display;
  const Renderer &renderer;
  lgfx::LGFX_Sprite band;
  Clay_Color clearColor{ 0, 0, 0, 255 };
  uint32_t lastHash = 0;
  bool hasLastHash = false;
};

/**
 * @brief Computes a hash of everything in @p commands that affects the picture.
 *
 * Used by BandedPresenter to detect frames that need no redraw.
 */
uint32_t hashRenderCommands(const Clay_RenderCommandArray &commands);

}  // namespace clay_lgfx
