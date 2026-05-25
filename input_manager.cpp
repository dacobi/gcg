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
    mouseMovedAccum = false;
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
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        mouseBtnsDown[event->button.button] = true;
        mouseBtnsClickedAccum.insert(event->button.button);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mouseBtnsDown[event->button.button] = false;
        mouseBtnsReleasedAccum.insert(event->button.button);
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
