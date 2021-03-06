/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "../KeyboardInputHandlerAPI.h"

#include <string>
#include <map>
#include <mutex>
#include <SDL.h>

namespace KeyState {
    enum Enum {
        Pressed,
        Releasing,
        Released,
        Idle,
    };
}

class KeyboardInputHandlerAPISDLImpl : public KeyboardInputHandlerAPI {
    public:
    void registerToKeyPress(SDL_Keycode key, std::function<void()> f);

    void registerToKeyRelease(SDL_Keycode key, std::function<void()> f);

    void update();

    int eventSDL(const void* event);

    bool isKeyPressed(SDL_Keycode key) {
        return queryKeyState(key, KeyState::Pressed);
    }

    bool isKeyReleased(SDL_Keycode key) {
        return queryKeyState(key, KeyState::Released);
    }

    bool queryKeyState(SDL_Keycode key, KeyState::Enum state);

    private:
    std::mutex mutex;
    std::map<SDL_Keycode, KeyState::Enum> keyState;
    std::map<SDL_Keycode, std::function<void()>> keyPressed2callback;
    std::map<SDL_Keycode, std::function<void()>> keyReleased2callback;
};
