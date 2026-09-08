#include "MacInput.h"

#include <radcontroller.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
class MacControllerInputPoint final : public IRadControllerInputPoint
{
public:
    MacControllerInputPoint(const char* name, const char* type)
        : mName(name), mType(type) {}

    void AddRef() override { ++mReferences; }
    void Release() override { if (--mReferences == 0) delete this; }
    const char* GetName() override { return mName.c_str(); }
    const char* GetType() override { return mType.c_str(); }
    void SetTolerance(float tolerance) override { mTolerance = std::clamp(tolerance, 0.0f, 1.0f); }
    float GetTolerance() override { return mTolerance; }
    void RegisterControllerInputPointCallback(IRadControllerInputPointCallback* callback, unsigned int userData) override
    {
        if (callback != nullptr) mCallbacks.push_back({ callback, userData });
    }
    void UnRegisterControllerInputPointCallback(IRadControllerInputPointCallback* callback) override
    {
        mCallbacks.erase(std::remove_if(mCallbacks.begin(), mCallbacks.end(), [callback](const Callback& item) { return item.callback == callback; }), mCallbacks.end());
    }
    float GetCurrentValue(unsigned int* time) override
    {
        if (time != nullptr) *time = 0;
        return mValue;
    }
    void SetRange(float minimum, float maximum) override { mMinimum = minimum; mMaximum = maximum; }
    void GetRange(float* minimum, float* maximum) override
    {
        if (minimum != nullptr) *minimum = mMinimum;
        if (maximum != nullptr) *maximum = mMaximum;
    }
    void SetValue(float value)
    {
        value = std::clamp(value, mMinimum, mMaximum);
        // The original controller layer assigns a tolerance of 1.0 to
        // digital buttons.  That describes their full [0,1] range, not a
        // dead-zone: treating it as one swallowed every 0 -> 1 key press.
        // Buttons must therefore report each binary edge exactly, while axes
        // retain the requested noise tolerance.
        if (mType == "Button")
        {
            if (value == mValue) return;
        }
        // Axis tolerances are intended to suppress tiny fluctuations, but a
        // neutral value must always be delivered.  Otherwise an axis that had
        // moved past its tolerance could remain latched at its old value when
        // the stick is released, producing an endlessly rotating camera.
        else if (value == mValue || (value != 0.0f && std::abs(value - mValue) <= mTolerance))
        {
            return;
        }
        mValue = value;
        for (const Callback& item : mCallbacks) item.callback->OnControllerInputPointChange(item.userData, value);
    }

private:
    struct Callback { IRadControllerInputPointCallback* callback; unsigned int userData; };
    int mReferences = 1;
    std::string mName;
    std::string mType;
    float mValue = 0.0f;
    float mTolerance = 0.0f;
    float mMinimum = -1.0f;
    float mMaximum = 1.0f;
    std::vector<Callback> mCallbacks;
};

class MacController final : public IRadController
{
public:
    MacController(const char* location, const char* type, const char* classification)
        : mLocation(location), mType(type), mClassification(classification) {}
    ~MacController() { for (MacControllerInputPoint* point : mPoints) point->Release(); }
    void AddRef() override { ++mReferences; }
    void Release() override { if (--mReferences == 0) delete this; }
    bool IsConnected() override { return mConnected; }
    const char* GetType() override { return mType.c_str(); }
    const char* GetClassification() override { return mClassification.c_str(); }
    unsigned int GetNumberOfInputPointsOfType(const char* type) override
    {
        return static_cast<unsigned int>(std::count_if(mPoints.begin(), mPoints.end(), [type](MacControllerInputPoint* point) { return std::strcmp(point->GetType(), type) == 0; }));
    }
    unsigned int GetNumberOfOutputPointsOfType(const char*) override { return 0; }
    IRadControllerInputPoint* GetInputPointByTypeAndIndex(const char* type, unsigned int index) override
    {
        for (MacControllerInputPoint* point : mPoints)
            if (std::strcmp(point->GetType(), type) == 0 && index-- == 0) return point;
        return nullptr;
    }
    IRadControllerOutputPoint* GetOutputPointByTypeAndIndex(const char*, unsigned int) override { return nullptr; }
    IRadControllerInputPoint* GetInputPointByName(const char* name) override
    {
        for (MacControllerInputPoint* point : mPoints)
            if (std::strcmp(point->GetName(), name) == 0) return point;
        return nullptr;
    }
    IRadControllerOutputPoint* GetOutputPointByName(const char*) override { return nullptr; }
    const char* GetLocation() override { return mLocation.c_str(); }
    unsigned int GetNumberOfInputPoints() override { return static_cast<unsigned int>(mPoints.size()); }
    IRadControllerInputPoint* GetInputPointByIndex(unsigned int index) override { return index < mPoints.size() ? mPoints[index] : nullptr; }
    unsigned int GetNumberOfOutputPoints() override { return 0; }
    IRadControllerOutputPoint* GetOutputPointByIndex(unsigned int) override { return nullptr; }
    MacControllerInputPoint* AddPoint(const char* name, const char* type)
    {
        auto* point = new MacControllerInputPoint(name, type);
        mPoints.push_back(point);
        return point;
    }
    MacControllerInputPoint* Point(unsigned int index) { return index < mPoints.size() ? mPoints[index] : nullptr; }
    void SetConnected(bool connected) { mConnected = connected; }

private:
    int mReferences = 1;
    bool mConnected = true;
    std::string mLocation, mType, mClassification;
    std::vector<MacControllerInputPoint*> mPoints;
};

class MacControllerSystem final : public IRadControllerSystem
{
public:
    MacControllerSystem()
    {
        mKeyboard = new MacController("Keyboard0", "Mac Keyboard", "Keyboard");
        for (int key = 0; key < 256; ++key) mKeyboard->AddPoint(("Key" + std::to_string(key)).c_str(), "Button");
        mMouse = new MacController("Mouse0", "Mac Trackpad", "Mouse");
        mMouse->AddPoint("XAxis", "XAxis"); mMouse->AddPoint("YAxis", "YAxis"); mMouse->AddPoint("ZAxis", "ZAxis");
        for (int button = 0; button < 8; ++button) mMouse->AddPoint(("Button" + std::to_string(button)).c_str(), "Button");
        mGamepad = new MacController("Joystick0", "Mac GameController", "Joystick");
        struct Axis { const char* name; const char* type; };
        const Axis axes[] = {
            { "XAxis", "XAxis" }, { "YAxis", "YAxis" }, { "ZAxis", "ZAxis" },
            { "RXAxis", "RxAxis" }, { "RYAxis", "RyAxis" }, { "RZAxis", "RzAxis" },
            { "Slider0", "Slider" }, { "Slider1", "Slider" },
            { "POV0", "POV" }, { "POV1", "POV" }
        };
        for (const Axis& axis : axes) mGamepad->AddPoint(axis.name, axis.type);
        // Desktop UserController::DeriveDirectionValues maps [0,1] to [-1,1].
        // Initialise before callbacks are registered: zero would mean full left.
        for (unsigned i = 0; i < 8; ++i) mGamepad->Point(i)->SetValue(0.5f);
        for (unsigned i = 8; i < 10; ++i) mGamepad->Point(i)->SetValue(1.0f);
        mGamepad->SetConnected(false);
        for (int button = 0; button < 32; ++button) mGamepad->AddPoint(("Button" + std::to_string(button)).c_str(), "Button");
        mWheel = new MacController("SteeringWheel0", "Mac GameController", "Joystick");
        for (unsigned int index = 0; index < mGamepad->GetNumberOfInputPoints(); ++index) mWheel->AddPoint(mGamepad->GetInputPointByIndex(index)->GetName(), mGamepad->GetInputPointByIndex(index)->GetType());
        for (unsigned i = 0; i < 8; ++i) mWheel->Point(i)->SetValue(0.5f);
        for (unsigned i = 8; i < 10; ++i) mWheel->Point(i)->SetValue(1.0f);
        mWheel->SetConnected(false);
        mControllers = { mKeyboard, mGamepad, mMouse, mWheel };
    }
    ~MacControllerSystem() { for (MacController* controller : mControllers) controller->Release(); }
    void AddRef() override { ++mReferences; }
    void Release() override { if (--mReferences == 0) delete this; }
    unsigned int GetNumberOfControllers() override { return static_cast<unsigned int>(mControllers.size()); }
    IRadController* GetControllerByIndex(unsigned int index) override { return index < mControllers.size() ? mControllers[index] : nullptr; }
    IRadController* GetControllerAtLocation(const char* location) override
    {
        for (MacController* controller : mControllers) if (std::strcmp(controller->GetLocation(), location) == 0) return controller;
        return nullptr;
    }
    void SetBufferTime(unsigned int) override { }
    void MapVirtualTime(unsigned int, unsigned int) override { }
    void SetVirtualTime(unsigned int) override { }
    void SetCaptureRate(unsigned int) override { }
    void RegisterConnectionChangeCallback(IRadControllerConnectionChangeCallback* callback) override { if (callback != nullptr) mConnectionCallbacks.push_back(callback); }
    void UnRegisterConnectionChangeCallback(IRadControllerConnectionChangeCallback* callback) override { mConnectionCallbacks.erase(std::remove(mConnectionCallbacks.begin(), mConnectionCallbacks.end(), callback), mConnectionCallbacks.end()); }
    void Service()
    {
        MacInputState state;
        MacInputUpdate(MacInputShared(), &state);
        // The game consumes DirectInput scan codes.  Build that one legacy
        // keyboard view here from the native action state; Cocoa events never
        // write directly into the old controller layer.  This prevents a key
        // press from being interpreted twice (the source of the runaway menu).
        bool keys[256] = {};
        keys[0x11] = state.actions[MAC_ACTION_ACCELERATE]; // W
        keys[0x1f] = state.actions[MAC_ACTION_BRAKE];      // S
        keys[0x1e] = state.actions[MAC_ACTION_STEER_LEFT]; // A
        keys[0x20] = state.actions[MAC_ACTION_STEER_RIGHT];// D
        keys[0x39] = state.actions[MAC_ACTION_JUMP];       // Space
        // Some PC action maps use the keyboard binding for GetOutCar while
        // others use Mouse Button 0 for entering/interacting.  Feed both
        // routes from E so a vehicle can always be exited.
        keys[0x12] = state.actions[MAC_ACTION_ACTION];     // E
        keys[0x2a] = state.actions[MAC_ACTION_SPRINT];     // Shift
        keys[0x1c] = state.actions[MAC_ACTION_CONFIRM];    // Return
        // Escape remains the front-end Back key.  Pause gets its own scan
        // code and is also mapped to InputManager::Start below, so Esc and P
        // both open the in-game pause screen without conflating press edges.
        keys[0x01] = state.actions[MAC_ACTION_CANCEL]; // Escape
        keys[0x19] = state.actions[MAC_ACTION_PAUSE];  // P
        keys[0xc8] = state.actions[MAC_ACTION_MENU_UP];
        keys[0xd0] = state.actions[MAC_ACTION_MENU_DOWN];
        keys[0xcb] = state.actions[MAC_ACTION_MENU_LEFT];
        keys[0xcd] = state.actions[MAC_ACTION_MENU_RIGHT];
        for (int key = 0; key < 256; ++key) mKeyboard->Point(key)->SetRange(0.0f, 1.0f), mKeyboard->Point(key)->SetValue(keys[key] ? 1.0f : 0.0f);
        mMouse->Point(0)->SetValue(state.trackpadDeltaX); mMouse->Point(1)->SetValue(state.trackpadDeltaY); mMouse->Point(2)->SetValue(0.0f);
        // The original PC mapping uses Mouse Button 0 for both DoAction and
        // GetOutCar.  Give Mac users a keyboard equivalent (E) and map the
        // pad's X/Square/Y action through this exact same input point.
        mMouse->Point(3)->SetRange(0.0f, 1.0f); mMouse->Point(3)->SetValue((state.trackpadButtons[0] || state.actions[MAC_ACTION_ACTION]) ? 1.0f : 0.0f);
        mMouse->Point(4)->SetRange(0.0f, 1.0f); mMouse->Point(4)->SetValue(state.trackpadButtons[1] ? 1.0f : 0.0f);
        const float values[] = {
            // Native sticks use [-1,1]; the PC mapping consumes [0,1].
            (state.steering + 1.0f) * 0.5f,
            (state.gamepadMoveY + 1.0f) * 0.5f,
            (state.cameraX + 1.0f) * 0.5f,
            0.5f, 0.5f, (state.cameraY + 1.0f) * 0.5f,
            // Unused sliders are centred too. Triggers currently use W/S.
            0.5f, 0.5f, 1.0f, 1.0f
        };
        for (unsigned int index = 0; index < 10; ++index) { mGamepad->Point(index)->SetValue(values[index]); mWheel->Point(index)->SetValue(values[index]); }
        for (unsigned int button = 0; button < 32; ++button)
        {
            bool pressed = state.gamepadButtons[button];
            mGamepad->Point(10 + button)->SetRange(0.0f, 1.0f); mGamepad->Point(10 + button)->SetValue(pressed ? 1.0f : 0.0f);
            mWheel->Point(10 + button)->SetRange(0.0f, 1.0f); mWheel->Point(10 + button)->SetValue(pressed ? 1.0f : 0.0f);
        }
        bool gamepadConnected = state.connectedControllerCount != 0;
        if (mGamepadConnected != gamepadConnected)
        {
            mGamepadConnected = gamepadConnected; mGamepad->SetConnected(gamepadConnected);
            // An Xbox pad is not a second, simultaneously active steering wheel.
            for (IRadControllerConnectionChangeCallback* callback : mConnectionCallbacks) { callback->OnControllerConnectionStatusChange(mGamepad); }
        }
    }
private:
    int mReferences = 1;
    bool mGamepadConnected = false;
    MacController *mKeyboard, *mGamepad, *mMouse, *mWheel;
    std::vector<MacController*> mControllers;
    std::vector<IRadControllerConnectionChangeCallback*> mConnectionCallbacks;
};

MacControllerSystem* gControllerSystem = nullptr;
}

void radControllerInitialize(IRadControllerConnectionChangeCallback* callback, radMemoryAllocator)
{
    if (gControllerSystem == nullptr) gControllerSystem = new MacControllerSystem();
    if (callback != nullptr) gControllerSystem->RegisterConnectionChangeCallback(callback);
}
void radControllerTerminate(void) { if (gControllerSystem != nullptr) { gControllerSystem->Release(); gControllerSystem = nullptr; } }
IRadControllerSystem* radControllerSystemGet(void) { return gControllerSystem; }
void radControllerSystemService(void) { if (gControllerSystem != nullptr) gControllerSystem->Service(); }
