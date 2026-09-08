#import <Foundation/Foundation.h>
// GameController is used below only for physical gamepads (joysticks are a
// genuine HID device, so a controller abstraction is appropriate for them).
// The keyboard is intentionally NOT read through GCKeyboard: unlike NSEvent,
// GCKeyboard's coalescedKeyboard reports physical key state independent of
// which window/app is key, so it can observe keystrokes the player types
// into other applications. All keyboard state below is fed exclusively by
// window-scoped NSEvent handling in MacHost.mm.
#import <GameController/GameController.h>

#include "MacInput.h"

#include <algorithm>
#include <cmath>
#include <cstring>

struct MacInputManager
{
    bool keyboard[MAC_ACTION_COUNT] = {};
    bool legacyKeys[256] = {};
    float trackpadX = 0.0f;
    float trackpadY = 0.0f;
    float trackpadDeltaX = 0.0f;
    float trackpadDeltaY = 0.0f;
    bool trackpadButtons[2] = {};
};

static MacInputManager* gSharedInput = nullptr;

static float ClampAxis(float value, float deadZone)
{
    value = std::clamp(value, -1.0f, 1.0f);
    const float magnitude = std::abs(value);
    if (magnitude <= deadZone)
        return 0.0f;

    // Rescale the remaining range so the camera does not jump when the stick
    // leaves its neutral zone.  A larger zone is deliberately used for the
    // right stick: small residual values are very noticeable as a camera that
    // keeps rotating after the player releases it.
    return std::copysign((magnitude - deadZone) / (1.0f - deadZone), value);
}

static bool Pressed(GCControllerButtonInput* button)
{
    return button != nil && button.isPressed;
}

static bool TriggerPressed(float value)
{
    // Bluetooth Xbox pads can report a small non-zero trigger value at rest.
    // The PC game expects triggers as digital buttons, so accepting a value
    // as low as 0.05 made a released trigger look permanently pressed.
    return value >= 0.20f;
}

static float PovFromDirection(float x, float y)
{
    constexpr float threshold = 0.35f;
    const int horizontal = x > threshold ? 1 : (x < -threshold ? -1 : 0);
    const int vertical = y > threshold ? 1 : (y < -threshold ? -1 : 0);
    if (horizontal == 0 && vertical == 0) return 1.0f;
    if (vertical > 0) return horizontal > 0 ? 0.125f : (horizontal < 0 ? 0.875f : 0.0f);
    if (vertical < 0) return horizontal > 0 ? 0.375f : (horizontal < 0 ? 0.625f : 0.5f);
    return horizontal > 0 ? 0.25f : 0.75f;
}

MacInputManager* MacInputCreate(void)
{
    MacInputManager* manager = new MacInputManager();
    return manager;
}

MacInputManager* MacInputShared(void)
{
    if (gSharedInput == nullptr)
        gSharedInput = MacInputCreate();
    return gSharedInput;
}

void MacInputResetKeyboard(MacInputManager* manager)
{
    if (manager == nullptr)
        return;
    // Called when the app/window loses key status. Without this, a key held
    // down at the moment focus is lost never gets its keyUp (that keyUp goes
    // to whichever app the player switched to instead), leaving the game
    // convinced the player is still holding it down indefinitely.
    std::memset(manager->keyboard, 0, sizeof(manager->keyboard));
    std::memset(manager->legacyKeys, 0, sizeof(manager->legacyKeys));
}

void MacInputDestroy(MacInputManager* manager)
{
    if (manager == gSharedInput)
        gSharedInput = nullptr;
    delete manager;
}

void MacInputSetKeyboardAction(MacInputManager* manager, MacGameAction action, bool pressed)
{
    if (manager != nullptr && action >= 0 && action < MAC_ACTION_COUNT)
        manager->keyboard[action] = pressed;
}

void MacInputSetLegacyKey(MacInputManager* manager, int keyCode, bool pressed)
{
    if (manager != nullptr && keyCode >= 0 && keyCode < 256)
        manager->legacyKeys[keyCode] = pressed;
}

void MacInputSetTrackpad(MacInputManager* manager, float x, float y, float deltaX, float deltaY)
{
    if (manager == nullptr)
        return;

    manager->trackpadX = x;
    manager->trackpadY = y;
    manager->trackpadDeltaX += deltaX;
    manager->trackpadDeltaY += deltaY;
}

void MacInputSetTrackpadButton(MacInputManager* manager, int button, bool pressed)
{
    if (manager != nullptr && button >= 0 && button < 2)
        manager->trackpadButtons[button] = pressed;
}

void MacInputUpdate(MacInputManager* manager, MacInputState* state)
{
    if (manager == nullptr || state == nullptr)
        return;

    std::memset(state, 0, sizeof(*state));
    state->gamepadPov0 = 1.0f;
    for (int action = 0; action < MAC_ACTION_COUNT; ++action)
        state->actions[action] = manager->keyboard[action];
    std::memcpy(state->legacyKeys, manager->legacyKeys, sizeof(state->legacyKeys));

    state->trackpadX = manager->trackpadX;
    state->trackpadY = manager->trackpadY;
    state->trackpadDeltaX = manager->trackpadDeltaX;
    state->trackpadDeltaY = manager->trackpadDeltaY;
    std::memcpy(state->trackpadButtons, manager->trackpadButtons, sizeof(state->trackpadButtons));
    manager->trackpadDeltaX = 0.0f;
    manager->trackpadDeltaY = 0.0f;

    for (GCController* controller in GCController.controllers)
    {
        GCExtendedGamepad* pad = controller.extendedGamepad;
        if (pad == nil)
            continue;

#if defined(RAD_MACOS)
        // Temporary native-controller diagnostic. This is intentionally the
        // unfiltered GameController state, so an idle controller that still
        // drives menus can be diagnosed from the launch log rather than by
        // guessing at another dead-zone value.
        static unsigned int diagnosticFrame = 0;
        static float previousCameraX = 0.0f;
        static float previousCameraY = 0.0f;
        const float rawCameraX = pad.rightThumbstick.xAxis.value;
        const float rawCameraY = pad.rightThumbstick.yAxis.value;
        if (std::abs(rawCameraX - previousCameraX) > 0.001f || std::abs(rawCameraY - previousCameraY) > 0.001f)
        {
            NSLog(@"[mac-camera] RX=%+.3f RY=%+.3f", rawCameraX, rawCameraY);
            previousCameraX = rawCameraX;
            previousCameraY = rawCameraY;
        }
        if ((++diagnosticFrame % 120u) == 0u)
        {
            NSLog(@"[mac-input] LX=%+.3f LY=%+.3f RX=%+.3f RY=%+.3f LT=%.3f RT=%.3f D=(%d,%d,%d,%d) A=%d B=%d X=%d Y=%d",
                  pad.leftThumbstick.xAxis.value, pad.leftThumbstick.yAxis.value,
                  pad.rightThumbstick.xAxis.value, pad.rightThumbstick.yAxis.value,
                  pad.leftTrigger.value, pad.rightTrigger.value,
                  Pressed(pad.dpad.up), Pressed(pad.dpad.down), Pressed(pad.dpad.left), Pressed(pad.dpad.right),
                  Pressed(pad.buttonA), Pressed(pad.buttonB), Pressed(pad.buttonX), Pressed(pad.buttonY));
        }
#endif

        ++state->connectedControllerCount;
        // GameController normalises the physical layouts of Xbox, PlayStation
        // and Switch Pro pads into these logical controls.
        // Filter physical stick noise before conversion to the PC axis range.
        const float steering = ClampAxis(pad.leftThumbstick.xAxis.value, 0.30f);
        const float moveY = ClampAxis(pad.leftThumbstick.yAxis.value, 0.30f);
        if (std::abs(steering) > std::abs(state->steering)) state->steering = steering;
        if (std::abs(moveY) > std::abs(state->gamepadMoveY)) state->gamepadMoveY = moveY;
        const float throttle = TriggerPressed(pad.rightTrigger.value) ? pad.rightTrigger.value : 0.0f;
        const float brake = TriggerPressed(pad.leftTrigger.value) ? pad.leftTrigger.value : 0.0f;
        state->throttle = std::max(state->throttle, throttle);
        state->brake = std::max(state->brake, brake);
        const float cameraX = ClampAxis(pad.rightThumbstick.xAxis.value, 0.24f);
        const float cameraY = ClampAxis(pad.rightThumbstick.yAxis.value, 0.24f);
        if (std::abs(cameraX) > std::abs(state->cameraX)) state->cameraX = cameraX;
        if (std::abs(cameraY) > std::abs(state->cameraY)) state->cameraY = cameraY;
        state->actions[MAC_ACTION_ACCELERATE] |= TriggerPressed(pad.rightTrigger.value);
        state->actions[MAC_ACTION_BRAKE] |= TriggerPressed(pad.leftTrigger.value);
        state->actions[MAC_ACTION_CONFIRM] |= Pressed(pad.buttonA);
        state->actions[MAC_ACTION_CANCEL] |= Pressed(pad.buttonB);
        state->actions[MAC_ACTION_ACTION] |= Pressed(pad.buttonX);
        state->actions[MAC_ACTION_JUMP] |= Pressed(pad.buttonY);
        state->actions[MAC_ACTION_PAUSE] |= Pressed(pad.buttonMenu);
        state->actions[MAC_ACTION_SPRINT] |= Pressed(pad.rightShoulder);
        // Do not infer digital directions from analogue d-pad axes.  A few
        // Xbox Bluetooth firmwares leave a small axis value after release,
        // which the 2003 UI repeatedly interprets as a held selection key.
        state->actions[MAC_ACTION_MENU_UP] |= Pressed(pad.dpad.up);
        state->actions[MAC_ACTION_MENU_DOWN] |= Pressed(pad.dpad.down);
        state->actions[MAC_ACTION_MENU_LEFT] |= Pressed(pad.dpad.left);
        state->actions[MAC_ACTION_MENU_RIGHT] |= Pressed(pad.dpad.right);

        state->gamepadButtons[0] |= Pressed(pad.buttonA);
        state->gamepadButtons[1] |= Pressed(pad.buttonB);
        state->gamepadButtons[2] |= Pressed(pad.buttonX);
        state->gamepadButtons[3] |= Pressed(pad.buttonY);
        state->gamepadButtons[4] |= Pressed(pad.leftShoulder);
        state->gamepadButtons[5] |= Pressed(pad.rightShoulder);
        // Triggers are sent via the W/S keyboard bridge above.  Do not expose
        // them as legacy gamepad Button6/7 as the original PC defaults bind
        // those same buttons to camera zoom/look-up as well as driving.
        state->gamepadButtons[8] |= Pressed(pad.buttonMenu);
        // The game UI receives the d-pad through the explicit keyboard-arrow
        // bridge above.  Leaving the legacy POV neutral prevents the same
        // press being dispatched a second time through DirectInput.
    }
}
