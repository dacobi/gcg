#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <SDL3/SDL.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <mutex>

class InputManager {
public:
    static InputManager& getInstance();

    void beginFrame();
    void processEvent(const SDL_Event* event);
    void clearAccumulatedState();

    // Lua API - These consume "Hit/Released/Moved" state
    bool lua_isKeyHit(SDL_Keycode key);
    bool lua_hasAnyKeyHit();
    bool isTextInputActive();
    bool lua_isKeyDown(SDL_Keycode key);
    bool lua_isKeyUp(SDL_Keycode key);

    void lua_getMousePos(int& x, int& y);
    bool lua_hasMouseMoved();
    void lua_getMouseMotion(int& rx, int& ry);
    int lua_getMouseWheelMotion();

    bool lua_isMouseBtnHit(int button);
    bool lua_isMouseBtnDown(int button);
    bool lua_isMouseBtnUp(int button);

    int lua_ioJoystickOpen(int index);
    void lua_ioJoystickClose(int handle);
    float lua_ioJoystickGetAxis(int handle, int axis_index);
    bool lua_ioJoystickGetButtonDown(int handle, int button_index);
    bool lua_ioJoystickGetButtonHit(int handle, int button_index);
    bool lua_ioJoystickGetButtonUp(int handle, int button_index);
    int lua_ioJoystickGetHat(int handle, int hat_index);
    int lua_ioJoystickGetNumAxes(int handle);
    int lua_ioJoystickGetNumButtons(int handle);
    int lua_ioJoystickGetNumHats(int handle);

    static SDL_Keycode stringToKeycode(const std::string& keyName);

private:
    InputManager() = default;
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    std::mutex inputMutex;

    // Current state
    std::unordered_map<SDL_Keycode, bool> keysDown;
    std::unordered_map<int, bool> mouseBtnsDown;
    int mouseX = 0;
    int mouseY = 0;

    // Accumulated state for Lua (consumed on read)
    std::unordered_set<SDL_Keycode> keysClickedAccum;
    std::unordered_set<SDL_Keycode> keysReleasedAccum;
    
    int mouseDeltaXAccum = 0;
    int mouseDeltaYAccum = 0;
    int mouseWheelAccum = 0;
    bool mouseMovedAccum = false;

    std::unordered_set<int> mouseBtnsClickedAccum;
    std::unordered_set<int> mouseBtnsReleasedAccum;

    struct JoystickState {
        std::unordered_map<int, bool> btnsDown;
        std::unordered_set<int> btnsHit;
        std::unordered_set<int> btnsReleased;
        std::unordered_map<int, float> axes;
        std::unordered_map<int, int> hats;
    };
    int nextJoystickHandle = 0;
    std::unordered_map<int, SDL_Joystick*> handleToJoystick;
    std::unordered_map<SDL_JoystickID, int> idToHandle;
    std::unordered_map<int, JoystickState> joyStates;
};

#endif
