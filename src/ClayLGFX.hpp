/**
 * @file ClayLGFX.hpp
 * @brief Convenience header that pulls in the whole Clay renderer for LovyanGFX.
 *
 * Include this after setting up LovyanGFX. It brings in the renderer, the font
 * table and the input controller. Text is drawn with LovyanGFX's own fonts,
 * so no font data comes with the library.
 *
 * Clay itself is included as well, but its implementation is not: exactly one
 * source file of the application has to define `CLAY_IMPLEMENTATION` before
 * including `clay.h`, as Clay's documentation describes.
 */
#pragma once

#include "clay_lgfx/font.hpp"
#include "clay_lgfx/font_table.hpp"
#include "clay_lgfx/geometry.hpp"
#include "clay_lgfx/input.hpp"
#include "clay_lgfx/renderer.hpp"
