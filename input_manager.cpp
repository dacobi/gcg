#include "input_manager.h"
#include <algorithm>

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

void InputManager::beginFrame() {
    // In this "consume on read" model, beginFrame doesn't need to do much
    // for the Lua thread, but we might want to reset things for the main thread
    // if we ever add main-thread polling. For now, we do nothing to avoid clearing
    // what Lua hasn't read yet.
}

void InputManager::clearAccumulatedState() {
    std::lock_guard<std::mutex> lock(inputMutex);
    keysClickedAccum.clear();
    keysReleasedAccum.clear();
    mouseBtnsClickedAccum.clear();
    mouseBtnsReleasedAccum.clear();
    mouseDeltaXAccum = 0;
    mouseDeltaYAccum = 0;
    mouseWheelAccum = 0;
    mouseMovedAccum = false;
    for (auto& pair : joyStates) {
        pair.second.btnsHit.clear();
        pair.second.btnsReleased.clear();
    }
}

void InputManager::processEvent(const SDL_Event* event) {
    std::lock_guard<std::mutex> lock(inputMutex);
    
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (!event->key.repeat) {
            keysDown[event->key.key] = true;
            keysClickedAccum.insert(event->key.key);
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        keysDown[event->key.key] = false;
        keysReleasedAccum.insert(event->key.key);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        mouseX = (int)event->motion.x;
        mouseY = (int)event->motion.y;
        mouseDeltaXAccum += (int)event->motion.xrel;
        mouseDeltaYAccum += (int)event->motion.yrel;
        mouseMovedAccum = true;
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        mouseWheelAccum += (int)event->wheel.y;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        mouseBtnsDown[event->button.button] = true;
        mouseBtnsClickedAccum.insert(event->button.button);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mouseBtnsDown[event->button.button] = false;
        mouseBtnsReleasedAccum.insert(event->button.button);
    } else if (event->type == SDL_EVENT_JOYSTICK_AXIS_MOTION) {
        auto it = idToHandle.find(event->jaxis.which);
        if (it != idToHandle.end()) {
            float val = event->jaxis.value / 32767.0f;
            if (val < -1.0f) val = -1.0f;
            else if (val > 1.0f) val = 1.0f;
            joyStates[it->second].axes[event->jaxis.axis] = val;
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) {
        auto it = idToHandle.find(event->jbutton.which);
        if (it != idToHandle.end()) {
            joyStates[it->second].btnsDown[event->jbutton.button] = true;
            joyStates[it->second].btnsHit.insert(event->jbutton.button);
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_BUTTON_UP) {
        auto it = idToHandle.find(event->jbutton.which);
        if (it != idToHandle.end()) {
            joyStates[it->second].btnsDown[event->jbutton.button] = false;
            joyStates[it->second].btnsReleased.insert(event->jbutton.button);
        }
    } else if (event->type == SDL_EVENT_JOYSTICK_HAT_MOTION) {
        auto it = idToHandle.find(event->jhat.which);
        if (it != idToHandle.end()) {
            joyStates[it->second].hats[event->jhat.hat] = event->jhat.value;
        }
    }
}

bool InputManager::lua_isKeyHit(SDL_Keycode key) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = keysClickedAccum.find(key);
    if (it != keysClickedAccum.end()) {
        keysClickedAccum.erase(it);
        return true;
    }
    return false;
}

bool InputManager::lua_hasAnyKeyHit() {
    std::lock_guard<std::mutex> lock(inputMutex);
    if (!keysClickedAccum.empty()) {
        keysClickedAccum.clear();
        return true;
    }
    return false;
}

extern SDL_Window* window;
bool InputManager::isTextInputActive() {
    if (!window) return false;
    return SDL_TextInputActive(window);
}

bool InputManager::lua_isKeyDown(SDL_Keycode key) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = keysDown.find(key);
    return (it != keysDown.end()) ? it->second : false;
}

bool InputManager::lua_isKeyUp(SDL_Keycode key) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = keysReleasedAccum.find(key);
    if (it != keysReleasedAccum.end()) {
        keysReleasedAccum.erase(it);
        return true;
    }
    return false;
}

void InputManager::lua_getMousePos(int& x, int& y) {
    std::lock_guard<std::mutex> lock(inputMutex);
    x = mouseX;
    y = mouseY;
}

bool InputManager::lua_hasMouseMoved() {
    std::lock_guard<std::mutex> lock(inputMutex);
    bool moved = mouseMovedAccum;
    mouseMovedAccum = false;
    return moved;
}

void InputManager::lua_getMouseMotion(int& rx, int& ry) {
    std::lock_guard<std::mutex> lock(inputMutex);
    rx = mouseDeltaXAccum;
    ry = mouseDeltaYAccum;
    mouseDeltaXAccum = 0;
    mouseDeltaYAccum = 0;
}

int InputManager::lua_getMouseWheelMotion() {
    std::lock_guard<std::mutex> lock(inputMutex);
    int w = mouseWheelAccum;
    mouseWheelAccum = 0;
    return w;
}

bool InputManager::lua_isMouseBtnHit(int button) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = mouseBtnsClickedAccum.find(button);
    if (it != mouseBtnsClickedAccum.end()) {
        mouseBtnsClickedAccum.erase(it);
        return true;
    }
    return false;
}

bool InputManager::lua_isMouseBtnDown(int button) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = mouseBtnsDown.find(button);
    return (it != mouseBtnsDown.end()) ? it->second : false;
}

bool InputManager::lua_isMouseBtnUp(int button) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = mouseBtnsReleasedAccum.find(button);
    if (it != mouseBtnsReleasedAccum.end()) {
        mouseBtnsReleasedAccum.erase(it);
        return true;
    }
    return false;
}

SDL_Keycode InputManager::stringToKeycode(const std::string& keyName) {
    std::string name = keyName;
    if (name.substr(0, 5) == "SDLK_") {
        name = name.substr(5);
    }
    
    SDL_Keycode kc = SDL_GetKeyFromName(name.c_str());
    if (kc != SDLK_UNKNOWN) return kc;

    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    kc = SDL_GetKeyFromName(upper.c_str());
    if (kc != SDLK_UNKNOWN) return kc;

    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    kc = SDL_GetKeyFromName(lower.c_str());
    
    return kc;
}

int InputManager::lua_ioJoystickOpen(int index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    int count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
    if (!joysticks || index < 0 || index >= count) {
        if (joysticks) SDL_free(joysticks);
        return -1;
    }
    SDL_Joystick* joy = SDL_OpenJoystick(joysticks[index]);
    SDL_free(joysticks);
    if (!joy) return -1;
    
    int handle = nextJoystickHandle++;
    SDL_JoystickID id = SDL_GetJoystickID(joy);
    handleToJoystick[handle] = joy;
    idToHandle[id] = handle;
    
    JoystickState state;
    int num_axes = SDL_GetNumJoystickAxes(joy);
    for (int i = 0; i < num_axes; ++i) {
        Sint16 initial_state = 0;
        if (SDL_GetJoystickAxisInitialState(joy, i, &initial_state)) {
            state.axes[i] = initial_state / 32767.0f;
        }
    }
    joyStates[handle] = state;
    
    return handle;
}

void InputManager::lua_ioJoystickClose(int handle) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = handleToJoystick.find(handle);
    if (it != handleToJoystick.end()) {
        SDL_JoystickID id = SDL_GetJoystickID(it->second);
        SDL_CloseJoystick(it->second);
        handleToJoystick.erase(it);
        idToHandle.erase(id);
        joyStates.erase(handle);
    }
}

float InputManager::lua_ioJoystickGetAxis(int handle, int axis_index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = joyStates.find(handle);
    if (it != joyStates.end()) {
        auto ait = it->second.axes.find(axis_index);
        if (ait != it->second.axes.end()) {
            return ait->second;
        }
    }
    return 0.0f;
}

bool InputManager::lua_ioJoystickGetButtonDown(int handle, int button_index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = joyStates.find(handle);
    if (it != joyStates.end()) {
        auto bit = it->second.btnsDown.find(button_index);
        if (bit != it->second.btnsDown.end()) {
            return bit->second;
        }
    }
    return false;
}

bool InputManager::lua_ioJoystickGetButtonHit(int handle, int button_index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = joyStates.find(handle);
    if (it != joyStates.end()) {
        auto bit = it->second.btnsHit.find(button_index);
        if (bit != it->second.btnsHit.end()) {
            it->second.btnsHit.erase(bit);
            return true;
        }
    }
    return false;
}

bool InputManager::lua_ioJoystickGetButtonUp(int handle, int button_index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = joyStates.find(handle);
    if (it != joyStates.end()) {
        auto bit = it->second.btnsReleased.find(button_index);
        if (bit != it->second.btnsReleased.end()) {
            it->second.btnsReleased.erase(bit);
            return true;
        }
    }
    return false;
}

int InputManager::lua_ioJoystickGetHat(int handle, int hat_index) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = joyStates.find(handle);
    if (it != joyStates.end()) {
        auto hit = it->second.hats.find(hat_index);
        if (hit != it->second.hats.end()) {
            return hit->second;
        }
    }
    return 0;
}

int InputManager::lua_ioJoystickGetNumAxes(int handle) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = handleToJoystick.find(handle);
    if (it != handleToJoystick.end()) {
        return SDL_GetNumJoystickAxes(it->second);
    }
    return 0;
}

int InputManager::lua_ioJoystickGetNumButtons(int handle) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = handleToJoystick.find(handle);
    if (it != handleToJoystick.end()) {
        return SDL_GetNumJoystickButtons(it->second);
    }
    return 0;
}

int InputManager::lua_ioJoystickGetNumHats(int handle) {
    std::lock_guard<std::mutex> lock(inputMutex);
    auto it = handleToJoystick.find(handle);
    if (it != handleToJoystick.end()) {
        return SDL_GetNumJoystickHats(it->second);
    }
    return 0;
}
