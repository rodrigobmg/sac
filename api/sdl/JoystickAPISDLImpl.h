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

#if !SAC_MOBILE
#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <api/JoystickAPI.h>

namespace JoystickButton {
    enum Enum {
        GREEN = 0,
        RED,
        BLUE,
        YELLOW,
        LB,
        RB,
        BACK,
        START,
        XBOX,
        LEFTCLICK,
        RIGHTCLICK,
        TOTAL
    };
}

namespace JoystickPad {
    enum Enum {
        LEFT = 0,
        RIGHT,
        TOTAL,
    };
}

struct JoystickState {
    bool down[JoystickButton::TOTAL];
    bool clicked[JoystickButton::TOTAL];
    bool doubleclicked[JoystickButton::TOTAL];
    float lastClickTime[JoystickButton::TOTAL];

    glm::vec2 lastDirection[JoystickPad::TOTAL];

    JoystickState() {
        for (unsigned b = 0; b < JoystickButton::TOTAL; ++b) {
            down[b] = clicked[b] = doubleclicked[b] = false;
            lastClickTime[b] = 0;
        }
        for (size_t p = 0; p<JoystickPad::TOTAL; p++) {
            lastDirection[p] = glm::vec2(0.0f);
        }
    }

    void* joystickPtr;
};

class JoystickAPISDLImpl : public JoystickAPI {
    public:
    JoystickAPISDLImpl();
    ~JoystickAPISDLImpl();

    int availableJoystick() const;

    bool isDown(int idx, int btn) const {
        return (joysticks.size() > (unsigned)idx) &&
            joysticks[idx].down[btn];
    }

    bool hasClicked(int idx, int btn) const {
        return (joysticks.size() > (unsigned)idx) &&
               joysticks[idx].clicked[btn];
    }

    bool hasDoubleClicked(int idx, int btn) const {
        return (joysticks.size() > (unsigned)idx) &&
               joysticks[idx].doubleclicked[btn];
    }

    void resetDoubleClick(int idx, int btn);

    const glm::vec2& getPadDirection(int idx, int pad) const {
        if (joysticks.size() > (unsigned)idx) {
            return joysticks[idx].lastDirection[pad];
        } else {
            static const glm::vec2 zero(0.0f);
            return zero;
        }
    }

    void update();

    int eventSDL(void* event);

    private:
    std::vector<JoystickState> joysticks;
};
#endif
