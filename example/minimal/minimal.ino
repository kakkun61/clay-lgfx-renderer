// The smallest useful program: a counter and a button that increments it.
//
// It runs on any ESP32 board LovyanGFX detects. The button can be tapped on a
// touch panel, or, on an original M5Stack, selected with buttons A and C and
// pressed with button B.

#define LGFX_AUTODETECT
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

// Clay is a single header library: exactly one source file compiles its
// implementation, and this sketch is that file.
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <ClayLGFX.hpp>

// Arduino calls setup() and loop() without arguments, so the objects they
// share have to live at file scope.
static LGFX display;
static clay_lgfx::FontTable fontTable;
static clay_lgfx::Renderer renderer(fontTable);
static clay_lgfx::BandedPresenter presenter(display, renderer);
static clay_lgfx::InputController input;
static clay_lgfx::GpioButtons buttons({ 39, 38, 37 });  // Buttons A, B and C of an original M5Stack.
static uint32_t lastMillis = 0;
static int count = 0;
static char countText[24];

static void printClayError(Clay_ErrorData error) {
  Serial.printf("clay: %.*s\n", static_cast<int>(error.errorText.length), error.errorText.chars);
}

void setup() {
  Serial.begin(115200);
  display.init();

  // Any LovyanGFX font works. Font 0 is also used by Clay's debug view.
  fontTable.set(0, &lgfx::fonts::DejaVu18);

  // Clay's defaults are sized for desktops; a small screen needs far fewer elements.
  Clay_SetMaxElementCount(64);
  Clay_SetMaxMeasureTextCacheWordCount(128);
  const uint32_t size = Clay_MinMemorySize();
  const Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(size, malloc(size));
  Clay_Initialize(arena,
                  Clay_Dimensions{ static_cast<float>(display.width()), static_cast<float>(display.height()) },
                  Clay_ErrorHandler{ printClayError, nullptr });
  fontTable.attach();

  // Draw through a buffer of 40 rows, so the screen never flickers.
  presenter.begin(40);
  presenter.setClearColor(Clay_Color{ 20, 24, 32, 255 });

  // Use whatever input the board has: a touch panel, buttons, or both.
  if (display.touch()) input.setTouchDisplay(&display);
  if (display.getBoard() == lgfx::board_t::board_M5Stack) {
    buttons.begin();
    input.setButtons(&clay_lgfx::GpioButtons::reader, &buttons);
  }
  lastMillis = millis();
}

void loop() {
  const uint32_t now = millis();
  const float deltaTime = static_cast<float>(now - lastMillis) / 1000.0f;
  lastMillis = now;

  input.update(deltaTime);  // Always before Clay_BeginLayout().
  Clay_BeginLayout();

  snprintf(countText, sizeof(countText), "Count: %d", count);
  CLAY(CLAY_ID("Root"),
       { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                     .childGap = 16,
                     .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                     .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
    CLAY_TEXT((Clay_String{ false, static_cast<int32_t>(strlen(countText)), countText }),
              CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));

    // focusable() lets the buttons reach this element; it returns whether to show the focus.
    const bool focused = input.focusable(CLAY_ID("Button"));
    CLAY(CLAY_ID("Button"),
         { .layout = { .padding = { 24, 24, 10, 10 } },
           .backgroundColor = Clay_Hovered() ? Clay_Color{ 86, 156, 214, 255 } : Clay_Color{ 56, 62, 80, 255 },
           .cornerRadius = CLAY_CORNER_RADIUS(8),
           .border = { .color = { 250, 200, 60, 255 },
                       .width = CLAY_BORDER_OUTSIDE(static_cast<uint16_t>(focused ? 2 : 0)) } }) {
      // A click is a release over the element.
      if (Clay_Hovered() && Clay_GetPointerState().state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) ++count;
      CLAY_TEXT(CLAY_STRING("Press me"), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
    }
  }

  presenter.present(Clay_EndLayout(deltaTime));
  delay(5);
}
