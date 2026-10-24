// Entry point of the demo on real hardware, built with arduino-cli.
//
// The display is detected at run time by LovyanGFX, so the same binary runs
// on any ESP32 board LovyanGFX can identify. The input devices are chosen from
// the detected board below; boards that are not listed get the touch panel if
// they have one and no buttons.

#define LGFX_AUTODETECT
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

#include "demo_app.hpp"

// These two objects are file-scope statics because Arduino calls setup() and
// loop() without arguments and gives them no other way to share state. They
// are confined to this file, and everything below them, DemoApp included,
// receives what it needs through constructors and parameters instead. The
// SDL entry point in sim_main.cpp, which owns its main loop, keeps the very
// same objects on the stack.
static LGFX display;
static DemoApp app(display);

static DemoApp::InputConfig inputConfigFor(lgfx::board_t board) {
  DemoApp::InputConfig config;
  config.useTouch = display.touch() != nullptr;
  switch (board) {
    case lgfx::board_t::board_M5Stack:
      // Three buttons under the screen, no touch panel.
      config.buttonPins.prev = 39;
      config.buttonPins.select = 38;
      config.buttonPins.next = 37;
      break;
    case lgfx::board_t::board_M5StackCore2:
      // The buttons are marks printed below the screen, read by the touch panel.
      config.touchButtonZone = true;
      break;
    case lgfx::board_t::board_M5StickC:
    case lgfx::board_t::board_M5StickCPlus:
      // Button A on the front selects, button B on the side moves on.
      config.buttonPins.select = 37;
      config.buttonPins.next = 39;
      break;
    case lgfx::board_t::board_M5StickCPlus2:
      config.buttonPins.prev = 35;
      config.buttonPins.select = 37;
      config.buttonPins.next = 39;
      break;
    default:
      // CoreS3 and other touch boards need nothing beyond the default.
      break;
  }
  return config;
}

void setup() {
  Serial.begin(115200);
  display.init();
  if (display.width() < display.height()) display.setRotation(display.getRotation() ^ 1);
  if (!app.begin(inputConfigFor(display.getBoard()))) {
    display.fillScreen(TFT_RED);
    display.drawString("Out of memory", 4, 4);
    display.setCursor(4, 24);
    display.printf("Clay needs %u bytes\n", static_cast<unsigned>(Clay_MinMemorySize()));
    display.setCursor(4, display.getCursorY());
    display.printf("free %u, largest block %u\n",
                   static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                   static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    for (;;) delay(1000);
  }
}

void loop() {
  app.frame();
  delay(5);
}
