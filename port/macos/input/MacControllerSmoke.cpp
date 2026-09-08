#include <radcontroller.hpp>
#include <radthread.hpp>
#include "MacInput.h"

#include <cstdio>
#include <cstring>

struct ButtonObserver final : IRadControllerInputPointCallback
{
    int changes = 0;
    float lastValue = 0.0f;
    void OnControllerInputPointChange(unsigned int, float value) override { ++changes; lastValue = value; }
};

int main()
{
    radThreadInitialize();
    IRadThreadMutex* mutex = nullptr;
    IRadThreadSemaphore* semaphore = nullptr;
    radThreadCreateMutex(&mutex);
    radThreadCreateSemaphore(&semaphore, 1);
    mutex->Lock(); mutex->Lock(); mutex->Unlock(); mutex->Unlock();
    semaphore->Wait(); semaphore->Signal();
    radControllerInitialize();
    IRadControllerSystem* system = radControllerSystemGet();
    if (system == nullptr || system->GetNumberOfControllers() != 4)
        return 1;

    const char* locations[] = { "Keyboard0", "Mouse0", "Joystick0", "SteeringWheel0" };
    for (const char* location : locations)
    {
        IRadController* controller = system->GetControllerAtLocation(location);
        if (controller == nullptr || std::strcmp(controller->GetLocation(), location) != 0 || controller->GetNumberOfInputPoints() == 0)
            return 2;
    }

    MacInputManager* input = MacInputShared();
    // The actual desktop consumer applies 2*v-1, so every idle stick/slider
    // must decode to zero even before the first service tick.
    IRadController* gamepad = system->GetControllerAtLocation("Joystick0");
    for (unsigned i = 0; i < 8; ++i)
        if (2.0f * gamepad->GetInputPointByIndex(i)->GetCurrentValue(nullptr) - 1.0f != 0.0f)
            return 7;
    if (system->GetControllerAtLocation("SteeringWheel0")->IsConnected()) return 8;
    IRadController* keyboard = system->GetControllerAtLocation("Keyboard0");
    IRadController* mouse = system->GetControllerAtLocation("Mouse0");
    ButtonObserver observer;
    IRadControllerInputPoint* returnKey = keyboard->GetInputPointByName("Key28");
    returnKey->SetTolerance(1.0f); // The game uses this exact digital-button tolerance.
    returnKey->RegisterControllerInputPointCallback(&observer, 0);
    // Native actions are the only input source exposed to the old DirectInput
    // facade.  Verify that confirm becomes the expected Return scan code.
    MacInputSetKeyboardAction(input, MAC_ACTION_CONFIRM, true);
    MacInputSetTrackpad(input, 10.0f, 20.0f, 4.0f, -2.0f);
    MacInputSetTrackpadButton(input, 0, true);
    MacInputSetTrackpadButton(input, 1, true);
    radControllerSystemService();

    if (keyboard->GetInputPointByName("Key28")->GetCurrentValue(nullptr) != 1.0f ||
        mouse->GetInputPointByName("XAxis")->GetCurrentValue(nullptr) != 1.0f ||
        mouse->GetInputPointByName("YAxis")->GetCurrentValue(nullptr) != -1.0f ||
        mouse->GetInputPointByName("Button0")->GetCurrentValue(nullptr) != 1.0f ||
        mouse->GetInputPointByName("Button1")->GetCurrentValue(nullptr) != 1.0f)
        return 3;
    if (observer.changes != 1 || observer.lastValue != 1.0f)
        return 5;

    MacInputSetKeyboardAction(input, MAC_ACTION_CONFIRM, false);
    MacInputSetTrackpadButton(input, 0, false);
    MacInputSetTrackpadButton(input, 1, false);
    radControllerSystemService();
    if (mouse->GetInputPointByName("Button0")->GetCurrentValue(nullptr) != 0.0f ||
        mouse->GetInputPointByName("Button1")->GetCurrentValue(nullptr) != 0.0f)
        return 4;
    if (observer.changes != 2 || observer.lastValue != 0.0f)
        return 6;
    ButtonObserver axisObserver;
    IRadControllerInputPoint* mouseX = mouse->GetInputPointByName("XAxis");
    mouseX->RegisterControllerInputPointCallback(&axisObserver, 0);
    MacInputSetTrackpad(input, 0, 0, 0.1f, 0);
    radControllerSystemService();
    radControllerSystemService(); // releases the consumed delta
    if (axisObserver.changes != 2 || axisObserver.lastValue != 0.0f) return 9;
    for (unsigned i = 0; i < 120; ++i) radControllerSystemService();
    if (axisObserver.changes != 2) return 10; // no phantom idle events
    mouseX->UnRegisterControllerInputPointCallback(&axisObserver);
    returnKey->UnRegisterControllerInputPointCallback(&observer);
    radControllerTerminate();
    mutex->Release();
    semaphore->Release();
    radThreadTerminate();
    std::puts("macOS controller backend: ok");
    return 0;
}
