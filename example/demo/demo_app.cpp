#include "demo_app.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// Fonts are registered under these IDs. Clay's debug view uses ID 0.
namespace font {
constexpr uint16_t small = 0;
constexpr uint16_t large = 1;
constexpr uint16_t title = 2;
constexpr uint16_t japanese = 3;
}  // namespace font

namespace color {
constexpr Clay_Color background{ 24, 26, 33, 255 };
constexpr Clay_Color surface{ 38, 42, 54, 255 };
constexpr Clay_Color surfaceHover{ 56, 62, 80, 255 };
constexpr Clay_Color accent{ 86, 156, 214, 255 };
constexpr Clay_Color accentDark{ 48, 98, 150, 255 };
constexpr Clay_Color danger{ 214, 92, 86, 255 };
constexpr Clay_Color ok{ 106, 180, 98, 255 };
constexpr Clay_Color text{ 230, 232, 238, 255 };
constexpr Clay_Color textDim{ 150, 156, 170, 255 };
constexpr Clay_Color focus{ 250, 200, 60, 255 };
}  // namespace color

const char *const itemNames[] = {
  "Kitchen", "Living", "Bedroom", "Study", "Hallway", "Garage", "Garden", "Porch", "Attic", "Cellar", "Bath", "Closet",
};
constexpr int itemCount = sizeof(itemNames) / sizeof(itemNames[0]);

// Clay expects strings with an explicit length. This wraps a C string that
// outlives the frame, such as a literal or a member buffer of DemoApp.
Clay_String clayString(const char *s) {
  Clay_String r{};
  r.isStaticallyAllocated = false;
  r.length = static_cast<int32_t>(strlen(s));
  r.chars = s;
  return r;
}

// A click is a release over the element, as with buttons on a desktop. Acting
// on release lets a finger that started a drag-scroll slide off harmlessly.
bool clickedThisFrame() {
  return Clay_Hovered() && Clay_GetPointerState().state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME;
}

Clay_TextElementConfig textStyle(uint16_t font, Clay_Color color) {
  Clay_TextElementConfig c{};
  c.textColor = color;
  c.fontId = font;
  c.wrapMode = CLAY_TEXT_WRAP_NONE;
  return c;
}

}  // namespace

DemoApp::DemoApp(lgfx::LGFX_Device &display)
  : display(display), renderer(fonts), presenter(display, renderer), buttons(clay_lgfx::GpioButtons::Pins{}) {
  // Fonts built into LovyanGFX, in two different formats: GFXfont for the
  // Latin text and U8g2 for Japanese.
  fonts.set(font::small, &lgfx::fonts::DejaVu12);
  fonts.set(font::large, &lgfx::fonts::DejaVu18);
  fonts.set(font::title, &lgfx::fonts::FreeSansBold9pt7b);
  fonts.set(font::japanese, &lgfx::fonts::lgfxJapanGothic_12);
  renderer.setCustomDrawHandler(&DemoApp::drawLamp, this);

  // A small RGB565 gradient to show off image elements.
  for (int y = 0; y < imageSize; ++y) {
    for (int x = 0; x < imageSize; ++x) {
      const uint16_t r = static_cast<uint16_t>(x * 31 / (imageSize - 1));
      const uint16_t g = static_cast<uint16_t>(y * 63 / (imageSize - 1));
      const uint16_t b = static_cast<uint16_t>(31 - r);
      imagePixels[y * imageSize + x] = static_cast<uint16_t>((r << 11) | (g << 5) | b);
    }
  }
  image.width = imageSize;
  image.height = imageSize;
  image.pixels = imagePixels;
}

DemoApp::~DemoApp() { free(clayMemory); }

bool DemoApp::begin(const InputConfig &config) {
  // Clay's defaults are sized for desktop applications and would need
  // megabytes. A screen of a few hundred pixels needs far fewer elements.
  Clay_SetMaxElementCount(80);
  Clay_SetMaxMeasureTextCacheWordCount(128);
  const uint32_t size = Clay_MinMemorySize();
  clayMemory = malloc(size);
  if (!clayMemory) {
    printf("demo: could not allocate %u bytes for Clay\n", static_cast<unsigned>(size));
    return false;
  }
  const Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(size, clayMemory);
  const Clay_Dimensions screen{ static_cast<float>(display.width()), static_cast<float>(display.height()) };
  clay = Clay_Initialize(arena, screen, Clay_ErrorHandler{ &DemoApp::handleClayError, this });
  fonts.attach();

  // 40 rows of 16 bit pixels at 320 pixels wide is 25 KB, small enough for
  // boards without PSRAM.
  if (!presenter.begin(40)) {
    printf("demo: could not allocate the band buffer\n");
    return false;
  }
  presenter.setClearColor(color::background);

  buttons = clay_lgfx::GpioButtons(config.buttonPins);
  buttons.begin();
  const bool anyButton = config.buttonPins.prev >= 0 || config.buttonPins.select >= 0 || config.buttonPins.next >= 0;
  input.setButtons(anyButton ? &clay_lgfx::GpioButtons::reader : nullptr, &buttons);
  input.setTouchDisplay(config.useTouch ? &display : nullptr);
  input.setTouchButtonZone(config.touchButtonZone);

  printf("demo: %ux%u, Clay uses %u bytes, touch %s, buttons %s\n",
         static_cast<unsigned>(display.width()),
         static_cast<unsigned>(display.height()),
         static_cast<unsigned>(size),
         input.hasTouch() ? "yes" : "no",
         input.hasButtons() ? "yes" : "no");
  lastMillis = lgfx::millis();
  return true;
}

void DemoApp::handleClayError(Clay_ErrorData error) {
  printf("clay: %.*s\n", static_cast<int>(error.errorText.length), error.errorText.chars);
}

void DemoApp::frame() {
  const uint32_t now = lgfx::millis();
  const float dt = static_cast<float>(now - lastMillis) / 1000.0f;
  lastMillis = now;

  Clay_SetCurrentContext(clay);
  input.update(dt);
  Clay_BeginLayout();
  layout();
  const Clay_RenderCommandArray commands = Clay_EndLayout(dt);
  presenter.present(commands);
}

void DemoApp::layout() {
  CLAY(CLAY_ID("Root"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM },
         .backgroundColor = color::background }) {
    layoutHeader();
    CLAY(CLAY_ID("Body"),
         { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                       .padding = { 4, 4, 4, 4 },
                       .childGap = 4 } }) {
      layoutList();
      layoutDetails();
    }
    layoutFooter();
  }
}

void DemoApp::layoutHeader() {
  CLAY(CLAY_ID("Header"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(26) },
                     .padding = { 8, 8, 0, 0 },
                     .childGap = 8,
                     .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
         .backgroundColor = color::accentDark }) {
    CLAY(
      CLAY_ID("Logo"),
      { .layout = { .sizing = { CLAY_SIZING_FIXED(16), CLAY_SIZING_FIXED(16) } }, .image = { .imageData = &image } }) {}
    CLAY_TEXT(CLAY_STRING("Clay x LovyanGFX"), CLAY_TEXT_CONFIG(textStyle(font::title, color::text)));
    CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } } }) {}
    if (input.hasTouch()) {
      CLAY_AUTO_ID({ .layout = { .padding = { 4, 4, 1, 1 } },
                     .backgroundColor = color::accent,
                     .cornerRadius = CLAY_CORNER_RADIUS(4) }) {
        CLAY_TEXT(CLAY_STRING("TOUCH"), CLAY_TEXT_CONFIG(textStyle(font::small, color::text)));
      }
    }
    if (input.hasButtons()) {
      CLAY_AUTO_ID({ .layout = { .padding = { 4, 4, 1, 1 } },
                     .backgroundColor = color::accent,
                     .cornerRadius = CLAY_CORNER_RADIUS(4) }) {
        CLAY_TEXT(CLAY_STRING("KEYS"), CLAY_TEXT_CONFIG(textStyle(font::small, color::text)));
      }
    }
  }
}

void DemoApp::layoutList() {
  const Clay_ElementId listId = CLAY_ID("List");
  CLAY(listId,
       { .layout = { .sizing = { CLAY_SIZING_FIXED(display.width() / 3.0f), CLAY_SIZING_GROW(0) },
                     .padding = { 4, 4, 4, 4 },
                     .childGap = 3,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM },
         .backgroundColor = color::surface,
         .cornerRadius = CLAY_CORNER_RADIUS(6),
         .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() } }) {
    for (int i = 0; i < itemCount; ++i) {
      const Clay_ElementId id = CLAY_IDI("ListItem", i);
      const bool focused = input.focusable(id, listId);
      const bool hovered = Clay_PointerOver(id);
      const bool current = i == selected;
      const Clay_Color fill = current ? color::accent : (hovered ? color::surfaceHover : color::surface);
      CLAY(
        id,
        { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22) },
                      .padding = { 6, 6, 0, 0 },
                      .childGap = 6,
                      .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
          .backgroundColor = fill,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = { .color = color::focus, .width = CLAY_BORDER_OUTSIDE(static_cast<uint16_t>(focused ? 2 : 0)) } }) {
        if (clickedThisFrame()) selected = i;
        CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_FIXED(6), CLAY_SIZING_FIXED(6) } },
                       .backgroundColor = lampOn[i] ? color::ok : color::textDim,
                       .cornerRadius = CLAY_CORNER_RADIUS(3) }) {}
        CLAY_TEXT(clayString(itemNames[i]), CLAY_TEXT_CONFIG(textStyle(font::small, color::text)));
      }
    }
  }
}

bool DemoApp::button(Clay_ElementId id, Clay_String label, Clay_Color color) {
  const bool focused = input.focusable(id);
  bool clicked = false;
  const bool pressed = Clay_PointerOver(id) && Clay_GetPointerState().state == CLAY_POINTER_DATA_PRESSED;
  CLAY(id,
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30) },
                     .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } },
         .backgroundColor = pressed ? color::accentDark : color,
         .cornerRadius = { 8, 2, 2, 8 },
         .border = { .color = color::focus, .width = CLAY_BORDER_OUTSIDE(static_cast<uint16_t>(focused ? 2 : 0)) } }) {
    clicked = clickedThisFrame();
    CLAY_TEXT(label, CLAY_TEXT_CONFIG(textStyle(font::large, color::text)));
  }
  return clicked;
}

void DemoApp::layoutDetails() {
  CLAY(CLAY_ID("Details"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                     .padding = { 8, 8, 6, 6 },
                     .childGap = 6,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM },
         .backgroundColor = color::surface,
         .cornerRadius = CLAY_CORNER_RADIUS(6) }) {
    CLAY(CLAY_ID("Title"),
         { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                       .childGap = 8,
                       .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
      CLAY_TEXT(clayString(itemNames[selected]), CLAY_TEXT_CONFIG(textStyle(font::large, color::text)));
      CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } } }) {}
      // A custom element: the renderer calls drawLamp() for it.
      CLAY(CLAY_ID("Lamp"),
           { .layout = { .sizing = { CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20) } },
             .custom = { .customData = &lampOn[selected] } }) {}
    }

    // Count with a bar that grows with it.
    int &count = counts[selected];
    snprintf(countText, sizeof(countText), "%d", count);
    CLAY(CLAY_ID("CountRow"),
         { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                       .childGap = 8,
                       .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
      CLAY_TEXT(CLAY_STRING("Level"), CLAY_TEXT_CONFIG(textStyle(font::small, color::textDim)));
      CLAY(CLAY_ID("Bar"),
           { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(10) } },
             .backgroundColor = color::background,
             .cornerRadius = CLAY_CORNER_RADIUS(5) }) {
        CLAY(CLAY_ID("BarFill"),
             { .layout = { .sizing = { CLAY_SIZING_PERCENT(count / 10.0f), CLAY_SIZING_GROW(0) } },
               .backgroundColor = color::ok,
               .cornerRadius = CLAY_CORNER_RADIUS(5) }) {}
      }
      CLAY_TEXT(clayString(countText), CLAY_TEXT_CONFIG(textStyle(font::large, color::text)));
    }

    CLAY(CLAY_ID("Buttons"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = 6 } }) {
      if (button(CLAY_ID("Minus"), CLAY_STRING("-"), color::surfaceHover) && count > 0) --count;
      if (button(CLAY_ID("Plus"), CLAY_STRING("+"), color::surfaceHover) && count < 10) ++count;
    }

    bool &on = lampOn[selected];
    if (button(CLAY_ID("Toggle"),
               on ? CLAY_STRING("Turn off") : CLAY_STRING("Turn on"),
               on ? color::danger : color::accent)) {
      on = !on;
    }

    CLAY_TEXT(CLAY_STRING("日本語も表示できます"), CLAY_TEXT_CONFIG(textStyle(font::japanese, color::text)));

    // Text scaled up from the small font: fontSize 26 doubles DejaVu12, whose lines are 13 pixels.
    CLAY_TEXT(
      CLAY_STRING("Scale x2"),
      CLAY_TEXT_CONFIG({ .textColor = color::textDim, .fontId = font::small, .fontSize = 26, .letterSpacing = 1 }));
  }
}

void DemoApp::layoutFooter() {
  if (input.hasButtons() && input.hasTouch()) {
    snprintf(statusText, sizeof(statusText), "Tap, or A/C to move and B to press");
  } else if (input.hasButtons()) {
    snprintf(statusText, sizeof(statusText), "A/C: move focus   B: press");
  } else if (input.hasTouch()) {
    snprintf(statusText, sizeof(statusText), "Tap to press, drag the list to scroll");
  } else {
    snprintf(statusText, sizeof(statusText), "No input device");
  }
  CLAY(CLAY_ID("Footer"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(16) },
                     .padding = { 6, 6, 0, 0 },
                     .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
         .backgroundColor = color::surface }) {
    CLAY_TEXT(clayString(statusText), CLAY_TEXT_CONFIG(textStyle(font::small, color::textDim)));
  }
}

void DemoApp::drawLamp(lgfx::LovyanGFX &target, const Clay_RenderCommand &command, const clay_lgfx::Rect &bounds,
                       void *userData) {
  (void) userData;
  const bool on = *static_cast<const bool *>(command.renderData.custom.customData);
  const int32_t r = (bounds.w < bounds.h ? bounds.w : bounds.h) / 2;
  const int32_t cx = bounds.x + bounds.w / 2;
  const int32_t cy = bounds.y + bounds.h / 2;
  target.fillCircle(cx, cy, r, on ? clay_lgfx::toLgfxColor(color::focus) : clay_lgfx::toLgfxColor(color::background));
  target.drawCircle(cx, cy, r, clay_lgfx::toLgfxColor(color::textDim));
}
