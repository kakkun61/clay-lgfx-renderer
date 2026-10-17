/**
 * @file input.hpp
 * @brief Feeds touch panels and hardware buttons into Clay's pointer model.
 *
 * Clay understands a single pointer that hovers and clicks. A touch panel
 * maps onto that directly. Boards with buttons and no touch panel, such as
 * the original M5Stack or the M5StickC, are handled by keeping a keyboard
 * style focus: two buttons move the focus between elements the application
 * marked as focusable, and the third one "clicks" the focused element by
 * pressing the pointer at its centre. Either way, the application reacts to
 * input with the usual Clay_Hovered(), Clay_PointerOver() and Clay_OnHover().
 */
#pragma once

#include <clay.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

namespace clay_lgfx {

/** @brief Bits reported by a ButtonReader. */
enum ButtonBits : uint8_t {
  buttonPrev = 1 << 0,    ///< Moves the focus to the previous element. Button A on an M5Stack.
  buttonSelect = 1 << 1,  ///< Clicks the focused element. Button B on an M5Stack.
  buttonNext = 1 << 2,    ///< Moves the focus to the next element. Button C on an M5Stack.
};

/**
 * @brief Returns which buttons are currently held down, as a mask of ButtonBits.
 *
 * @p context is the pointer registered together with the function. Providing
 * a function instead of pin numbers lets the application read buttons
 * through whatever it likes: GPIO, an I/O expander, M5Unified, and so on.
 */
using ButtonReader = uint8_t (*)(void *context);

/**
 * @brief Reads up to three push buttons wired straight to GPIO pins.
 *
 * Works anywhere LovyanGFX provides `lgfx::gpio_in()`, which includes the SDL
 * simulator. The simulator maps the left, down and right arrow keys to GPIO
 * 39, 38 and 37, the pins of the buttons on an original M5Stack.
 */
class GpioButtons {
 public:
  /** @brief Pin assignment. A negative pin number means the button is absent. */
  struct Pins {
    int16_t prev = -1;      ///< Pin of the "previous" button.
    int16_t select = -1;    ///< Pin of the "select" button.
    int16_t next = -1;      ///< Pin of the "next" button.
    bool activeLow = true;  ///< True when a pressed button pulls its pin low.
  };

  /** @brief Creates a reader for the given pins. Call begin() before reading. */
  explicit GpioButtons(const Pins &pins) : pins(pins) {}

  /** @brief Configures the pins as inputs, with pull-ups for active low buttons. */
  void begin() const;

  /** @brief Returns the buttons held down right now as a mask of ButtonBits. */
  uint8_t read() const;

  /** @brief Adapter for InputController::setButtons(). @p self must point to a GpioButtons. */
  static uint8_t reader(void *self) { return static_cast<const GpioButtons *>(self)->read(); }

 private:
  bool pressed(int16_t pin) const;

  Pins pins;
};

/**
 * @brief Turns touch and button input into Clay pointer state, once per frame.
 *
 * A typical frame looks like this:
 * @code
 * input.update(deltaTime);            // before Clay_BeginLayout()
 * Clay_BeginLayout();
 * CLAY(CLAY_ID("Ok"), { ... }) {
 *     bool focused = input.focusable(CLAY_ID("Ok"));
 *     ...
 * }
 * Clay_RenderCommandArray commands = Clay_EndLayout(deltaTime);
 * @endcode
 *
 * Touch and buttons can be used together. A touch takes over the pointer
 * immediately and hides the focus. Pressing a button brings the focus back
 * and parks the pointer on the focused element, so hover effects double as a
 * focus indicator without any extra code.
 *
 * The controller talks to whichever Clay context is current when update()
 * is called, through Clay's own API.
 */
class InputController {
 public:
  /** @brief Maximum number of focusable elements per frame. Extra ones are ignored. */
  static constexpr int maxFocusables = 32;

  /**
     * @brief Uses the touch panel attached to @p display.
     *
     * Pass nullptr to go without touch. Touch is read with
     * `display->getTouch()`, so it works with anything LovyanGFX can read
     * touches from, including the mouse of the SDL simulator, whose display
     * reports no touch controller through `touch()`. On hardware, checking
     * `display.touch() != nullptr` tells whether the board has a touch panel.
     * The display is referenced, not copied.
     */
  void setTouchDisplay(lgfx::LGFX_Device *display) { touchDisplay = display; }

  /** @brief Uses @p reader to read the buttons. Pass nullptr to go without buttons. */
  void setButtons(ButtonReader reader, void *context) {
    buttonReader = reader;
    buttonContext = context;
  }

  /**
     * @brief Treats touches below the bottom edge of the display as button presses.
     *
     * The M5Stack Core2 has three touch sensitive marks under its screen,
     * reported by the touch panel as coordinates just beyond the visible
     * area. With this enabled, that strip is split into thirds which act as
     * the previous, select and next buttons.
     */
  void setTouchButtonZone(bool enabled) { touchButtonZone = enabled; }

  /** @brief Returns true when touch input is in use. */
  bool hasTouch() const { return touchDisplay != nullptr; }

  /** @brief Returns true when some kind of button input is available. */
  bool hasButtons() const { return buttonReader != nullptr || (touchButtonZone && hasTouch()); }

  /**
     * @brief Polls the input devices and hands the result to Clay.
     *
     * Call once per frame before Clay_BeginLayout(). It calls
     * Clay_SetPointerState() and Clay_UpdateScrollContainers(). Drag
     * scrolling is enabled whenever a touch panel is present.
     *
     * @param deltaTime Seconds since the previous frame, for scroll momentum.
     */
  void update(float deltaTime);

  /**
     * @brief Declares the element @p id as a focus target for this frame.
     *
     * Call it while laying out, in the order the focus should travel. Ids are
     * remembered until the next update(), which is when the buttons act on
     * them, so focus always moves through the layout of the previous frame.
     *
     * @param id               The element that can receive focus.
     * @param scrollContainer  The scroll container holding the element, if any.
     *                         When given, moving the focus to the element
     *                         scrolls the container so that it is visible.
     * @return true when the element has the focus and the focus is visible,
     *         meaning it should be highlighted.
     */
  bool focusable(Clay_ElementId id, Clay_ElementId scrollContainer = Clay_ElementId{});

  /** @brief Returns true when @p id has the focus, whether or not the focus is visible. */
  bool isFocused(Clay_ElementId id) const { return focusedId != 0 && id.id == focusedId; }

  /** @brief Returns true while the focus should be shown, that is after the last input came from a button. */
  bool focusVisible() const { return focusShown; }

  /** @brief Returns the mask of ButtonBits held down during the last update(). */
  uint8_t buttons() const { return heldButtons; }

 private:
  struct Focusable {
    uint32_t id;
    uint32_t scrollContainer;
  };

  uint8_t readButtons(bool *touchInZone, int32_t *touchX, int32_t *touchY, bool *touched);
  int findFocus(const Focusable *list, int count) const;
  void scrollIntoView(const Focusable &f) const;

  lgfx::LGFX_Device *touchDisplay = nullptr;
  ButtonReader buttonReader = nullptr;
  void *buttonContext = nullptr;
  bool touchButtonZone = false;

  // Focusables are collected into one list while the other one, filled
  // during the previous frame, is used for navigation.
  Focusable lists[2][maxFocusables] = {};
  int counts[2] = { 0, 0 };
  int filling = 0;

  uint32_t focusedId = 0;
  bool focusShown = false;
  uint8_t heldButtons = 0;
  bool selectArmed = false;
  bool pointerWasDown = false;
  Clay_Vector2 lastPointer{ -1, -1 };

  // Distance in pixels a finger may travel before a touch stops being a tap.
  static constexpr float tapSlop = 8.0f;
  bool touchActive = false;
  bool touchMoved = false;
  Clay_Vector2 touchStart{ 0, 0 };
};

}  // namespace clay_lgfx
