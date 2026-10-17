#include "input.hpp"

namespace clay_lgfx {

namespace {

Clay_ElementId elementId(uint32_t id) {
  Clay_ElementId e{};
  e.id = id;
  return e;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Returns how far a scroll position has to move along one axis so that the
// span [itemStart, itemEnd) becomes visible in [viewStart, viewEnd).
float scrollDelta(float itemStart, float itemEnd, float viewStart, float viewEnd) {
  if (itemStart < viewStart) return viewStart - itemStart;
  if (itemEnd > viewEnd) {
    const float d = viewEnd - itemEnd;
    // Never push the start of an item taller than the view out of sight.
    return (itemStart + d < viewStart) ? viewStart - itemStart : d;
  }
  return 0;
}

}  // namespace

void GpioButtons::begin() const {
  const lgfx::pin_mode_t mode = pins.activeLow ? lgfx::pin_mode_t::input_pullup : lgfx::pin_mode_t::input;
  const int16_t assigned[] = { pins.prev, pins.select, pins.next };
  for (int16_t pin : assigned) {
    if (pin >= 0) lgfx::pinMode(pin, mode);
  }
}

bool GpioButtons::pressed(int16_t pin) const {
  if (pin < 0) return false;
  return lgfx::gpio_in(pin) != pins.activeLow;
}

uint8_t GpioButtons::read() const {
  uint8_t mask = 0;
  if (pressed(pins.prev)) mask |= buttonPrev;
  if (pressed(pins.select)) mask |= buttonSelect;
  if (pressed(pins.next)) mask |= buttonNext;
  return mask;
}

bool InputController::focusable(Clay_ElementId id, Clay_ElementId scrollContainer) {
  if (counts[filling] < maxFocusables) {
    lists[filling][counts[filling]++] = Focusable{ id.id, scrollContainer.id };
  }
  return focusShown && isFocused(id);
}

uint8_t InputController::readButtons(bool *touchInZone, int32_t *touchX, int32_t *touchY, bool *touched) {
  uint8_t mask = buttonReader ? buttonReader(buttonContext) : 0;
  *touched = false;
  *touchInZone = false;
  if (hasTouch()) {
    int32_t x, y;
    if (touchDisplay->getTouch(&x, &y)) {
      *touched = true;
      *touchX = x;
      *touchY = y;
      if (touchButtonZone && y >= touchDisplay->height()) {
        *touchInZone = true;
        const int32_t third = touchDisplay->width() / 3;
        mask |= x < third ? buttonPrev : (x < third * 2 ? buttonSelect : buttonNext);
      }
    }
  }
  return mask;
}

int InputController::findFocus(const Focusable *list, int count) const {
  for (int i = 0; i < count; ++i) {
    if (list[i].id == focusedId) return i;
  }
  return -1;
}

void InputController::scrollIntoView(const Focusable &f) const {
  if (f.scrollContainer == 0) return;
  const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(elementId(f.scrollContainer));
  const Clay_ElementData view = Clay_GetElementData(elementId(f.scrollContainer));
  const Clay_ElementData item = Clay_GetElementData(elementId(f.id));
  if (!scroll.found || !view.found || !item.found) return;

  const Clay_BoundingBox &v = view.boundingBox;
  const Clay_BoundingBox &it = item.boundingBox;
  Clay_Vector2 *pos = scroll.scrollPosition;
  // Clay stores scroll positions as zero or negative offsets of the content.
  const float minX = -(scroll.contentDimensions.width - scroll.scrollContainerDimensions.width);
  const float minY = -(scroll.contentDimensions.height - scroll.scrollContainerDimensions.height);
  if (scroll.config.horizontal) {
    pos->x = clampf(pos->x + scrollDelta(it.x, it.x + it.width, v.x, v.x + v.width), minX < 0 ? minX : 0, 0);
  }
  if (scroll.config.vertical) {
    pos->y = clampf(pos->y + scrollDelta(it.y, it.y + it.height, v.y, v.y + v.height), minY < 0 ? minY : 0, 0);
  }
}

void InputController::update(float deltaTime) {
  // The list filled during the previous frame becomes the navigation list,
  // and the other one is cleared to collect this frame's focusables.
  const Focusable *list = lists[filling];
  const int count = counts[filling];
  filling ^= 1;
  counts[filling] = 0;

  bool touched, touchInZone;
  int32_t touchX = 0, touchY = 0;
  const uint8_t held = readButtons(&touchInZone, &touchX, &touchY, &touched);
  const uint8_t pressed = held & static_cast<uint8_t>(~heldButtons);
  heldButtons = held;

  if (pressed) {
    // While the focus is hidden, the first press only reveals it, so a
    // user switching from touch to buttons does not act blindly.
    const bool wasVisible = focusShown;
    focusShown = true;
    int index = findFocus(list, count);
    if (index < 0) {
      index = count > 0 ? 0 : -1;
    } else if (wasVisible && count > 0) {
      if (pressed & buttonPrev) index = (index + count - 1) % count;
      if (pressed & buttonNext) index = (index + 1) % count;
    }
    focusedId = index >= 0 ? list[index].id : 0;
    if (index >= 0 && (pressed & (buttonPrev | buttonNext))) scrollIntoView(list[index]);
    if ((pressed & buttonSelect) && wasVisible) selectArmed = true;
  }
  if (!(held & buttonSelect)) selectArmed = false;

  Clay_Vector2 position{ -1, -1 };
  bool down = false;
  const Clay_ElementData focused = focusedId ? Clay_GetElementData(elementId(focusedId)) : Clay_ElementData{};
  if (touched && !touchInZone) {
    position = Clay_Vector2{ static_cast<float>(touchX), static_cast<float>(touchY) };
    down = true;
    focusShown = false;
    if (!touchActive) {
      touchActive = true;
      touchMoved = false;
      touchStart = position;
    } else {
      const float dx = position.x - touchStart.x;
      const float dy = position.y - touchStart.y;
      if (dx * dx + dy * dy > tapSlop * tapSlop) touchMoved = true;
    }
  } else if (focusShown && focused.found) {
    const Clay_BoundingBox &b = focused.boundingBox;
    position = Clay_Vector2{ b.x + b.width / 2, b.y + b.height / 2 };
    down = selectArmed;
  } else if (pointerWasDown) {
    // Report the release where the finger was lifted, so that Clay sees a
    // RELEASED_THIS_FRAME over the element, then move the pointer away
    // on the next frame. A finger lifted from a panel does not hover.
    position = lastPointer;
    // After a drag, which most likely scrolled something, the release is
    // reported away from every element so that it does not count as a tap.
    if (touchActive && touchMoved) position = Clay_Vector2{ -1, -1 };
  }
  if (!(touched && !touchInZone)) touchActive = false;

  Clay_SetPointerState(position, down);
  Clay_UpdateScrollContainers(hasTouch(), Clay_Vector2{ 0, 0 }, deltaTime);
  pointerWasDown = down;
  lastPointer = position;
}

}  // namespace clay_lgfx
