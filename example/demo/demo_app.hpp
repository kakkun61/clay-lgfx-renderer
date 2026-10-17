/**
 * @file demo_app.hpp
 * @brief A small Clay user interface used to try the renderer on real hardware.
 *
 * The same code runs on the board and in the SDL simulator. Only the entry
 * points, demo.ino and sim_main.cpp, differ: they create the display, decide
 * which input devices exist and then call DemoApp::frame() in a loop.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ClayLGFX.hpp>

/**
 * @brief Owns everything the demo needs: Clay's memory, fonts, renderer and input.
 *
 * Nothing in here is global. The entry points keep one instance alive for
 * the lifetime of the program.
 */
class DemoApp {
 public:
  /** @brief Describes the input hardware of the board the demo runs on. */
  struct InputConfig {
    /** Buttons wired to GPIO pins. Leave every pin negative when there are none. */
    clay_lgfx::GpioButtons::Pins buttonPins;
    /** Read touches from the display. */
    bool useTouch = false;
    /** Treat touches below the screen as buttons, as on the M5Stack Core2. */
    bool touchButtonZone = false;
  };

  /** @brief Creates the demo for @p display, which must outlive it. */
  explicit DemoApp(lgfx::LGFX_Device &display);
  ~DemoApp();

  DemoApp(const DemoApp &) = delete;
  DemoApp &operator=(const DemoApp &) = delete;

  /**
     * @brief Sets up Clay and the input devices. Call after the display has been initialised.
     * @return false when memory could not be allocated.
     */
  bool begin(const InputConfig &config);

  /** @brief Handles input, lays out and draws one frame. */
  void frame();

 private:
  static void handleClayError(Clay_ErrorData error);
  static void drawLamp(lgfx::LovyanGFX &target, const Clay_RenderCommand &command, const clay_lgfx::Rect &bounds,
                       void *userData);

  void layout();
  void layoutHeader();
  void layoutList();
  void layoutDetails();
  void layoutFooter();
  bool button(Clay_ElementId id, Clay_String label, Clay_Color color);

  lgfx::LGFX_Device &display;
  clay_lgfx::FontTable fonts;
  clay_lgfx::Renderer renderer;
  clay_lgfx::BandedPresenter presenter;
  clay_lgfx::GpioButtons buttons;
  clay_lgfx::InputController input;

  void *clayMemory = nullptr;
  Clay_Context *clay = nullptr;
  uint32_t lastMillis = 0;

  // Application state.
  int selected = 0;
  int counts[12] = {};
  bool lampOn[12] = {};

  // Text built at run time has to stay alive until the frame is drawn.
  char countText[16] = {};
  char statusText[48] = {};

  static constexpr int imageSize = 24;
  uint16_t imagePixels[imageSize * imageSize] = {};
  clay_lgfx::Image image;
};
