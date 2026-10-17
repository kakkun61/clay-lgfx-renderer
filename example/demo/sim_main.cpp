// Entry point of the demo in the SDL simulator on a desktop computer.
//
// The mouse plays the role of the touch panel and the left, down and right
// arrow keys those of buttons A, B and C of an original M5Stack, so both kinds
// of input can be tried at once. This file is only compiled by `make sim`;
// the Arduino build ignores it.
#ifdef CLAY_LGFX_SIMULATOR

#define LGFX_AUTODETECT
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

#include "demo_app.hpp"

namespace {

int runDemo(bool *running) {
  // 320x240 like most M5Stack boards, shown at twice the size.
  LGFX display(320, 240, 2);
  display.init();

  DemoApp app(display);
  DemoApp::InputConfig config;
  config.buttonPins.prev = 39;
  config.buttonPins.select = 38;
  config.buttonPins.next = 37;
  config.useTouch = true;
  if (!app.begin(config)) return 1;

  while (*running) {
    app.frame();
    lgfx::delay(10);
  }
  return 0;
}

}  // namespace

int main(int, char **) { return lgfx::Panel_sdl::main(runDemo); }

#endif  // CLAY_LGFX_SIMULATOR
