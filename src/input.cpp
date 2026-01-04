#include "input.h"
#include "bot.h"
#include <algorithm>

namespace ScrapHeap {

InputManager& InputManager::instance() {
    static InputManager manager;
    return manager;
}

void InputManager::initialize() {
    // Reset all controllers
    for (auto& controller : controllers) {
        controller = ControllerState{};
    }
    keyboard = KeyboardState{};

    // Open any already-connected gamepads
    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks && i < MAX_CONTROLLERS; ++i) {
            SDL_Gamepad* pad = SDL_OpenGamepad(joysticks[i]);
            if (pad) {
                controllers[i].gamepad = pad;
                controllers[i].id = joysticks[i];
                controllers[i].connected = true;
            }
        }
        SDL_free(joysticks);
    }
}

void InputManager::shutdown() {
    for (auto& controller : controllers) {
        if (controller.gamepad) {
            SDL_CloseGamepad(controller.gamepad);
            controller.gamepad = nullptr;
        }
    }
}

void InputManager::beginFrame() {
    // Save previous states for press detection
    for (auto& controller : controllers) {
        controller.prevDpadUp = controller.dpadUp;
        controller.prevDpadDown = controller.dpadDown;
        controller.prevDpadLeft = controller.dpadLeft;
        controller.prevDpadRight = controller.dpadRight;
        controller.prevButtonA = controller.buttonA;
        controller.prevButtonB = controller.buttonB;
        controller.prevButtonX = controller.buttonX;
        controller.prevButtonY = controller.buttonY;
        controller.prevButtonStart = controller.buttonStart;
    }

    keyboard.prevUp = keyboard.up;
    keyboard.prevDown = keyboard.down;
    keyboard.prevLeft = keyboard.left;
    keyboard.prevRight = keyboard.right;
    keyboard.prevSpace = keyboard.space;
    keyboard.prevShift = keyboard.shift;
    keyboard.prevEnter = keyboard.enter;
    keyboard.prevEscape = keyboard.escape;
    keyboard.prevTab = keyboard.tab;

    // Update controller states
    for (auto& controller : controllers) {
        if (controller.connected && controller.gamepad) {
            updateControllerState(controller);
        }
    }
}

void InputManager::updateControllerState(ControllerState& controller) {
    SDL_Gamepad* pad = controller.gamepad;

    // D-pad
    controller.dpadUp = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    controller.dpadDown = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    controller.dpadLeft = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    controller.dpadRight = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    // Face buttons
    controller.buttonA = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
    controller.buttonB = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
    controller.buttonX = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
    controller.buttonY = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
    controller.buttonStart = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);

    // Analog sticks
    float lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    float rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    float ry = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

    controller.leftStickX = applyDeadzone(lx);
    controller.leftStickY = applyDeadzone(ly);
    controller.rightStickX = applyDeadzone(rx);
    controller.rightStickY = applyDeadzone(ry);

    // Triggers for throttle/reverse
    float lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    float rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    controller.leftTrigger = std::max(0.0f, lt);
    controller.rightTrigger = std::max(0.0f, rt);

    // Shoulder buttons as alternative throttle/reverse
    controller.leftShoulder = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    controller.rightShoulder = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    // Left stick can act as d-pad (for menus only, not movement)
    if (controller.leftStickY < -0.5f) controller.dpadUp = true;
    if (controller.leftStickY > 0.5f) controller.dpadDown = true;
    if (controller.leftStickX < -0.5f) controller.dpadLeft = true;
    if (controller.leftStickX > 0.5f) controller.dpadRight = true;
}

float InputManager::applyDeadzone(float value) const {
    if (std::abs(value) < STICK_DEADZONE) return 0.0f;
    float sign = value > 0 ? 1.0f : -1.0f;
    return sign * (std::abs(value) - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
}

void InputManager::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            // Find empty slot
            for (int i = 0; i < MAX_CONTROLLERS; ++i) {
                if (!controllers[i].connected) {
                    SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which);
                    if (pad) {
                        controllers[i].gamepad = pad;
                        controllers[i].id = event.gdevice.which;
                        controllers[i].connected = true;
                    }
                    break;
                }
            }
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED: {
            for (int i = 0; i < MAX_CONTROLLERS; ++i) {
                if (controllers[i].id == event.gdevice.which) {
                    if (controllers[i].gamepad) {
                        SDL_CloseGamepad(controllers[i].gamepad);
                    }
                    controllers[i] = ControllerState{};
                    break;
                }
            }
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
            switch (event.key.scancode) {
                case SDL_SCANCODE_UP:
                case SDL_SCANCODE_W:
                    keyboard.up = pressed;
                    break;
                case SDL_SCANCODE_DOWN:
                case SDL_SCANCODE_S:
                    keyboard.down = pressed;
                    break;
                case SDL_SCANCODE_LEFT:
                case SDL_SCANCODE_A:
                    keyboard.left = pressed;
                    break;
                case SDL_SCANCODE_RIGHT:
                case SDL_SCANCODE_D:
                    keyboard.right = pressed;
                    break;
                case SDL_SCANCODE_SPACE:
                    keyboard.space = pressed;
                    break;
                case SDL_SCANCODE_LSHIFT:
                case SDL_SCANCODE_RSHIFT:
                    keyboard.shift = pressed;
                    break;
                case SDL_SCANCODE_RETURN:
                    keyboard.enter = pressed;
                    break;
                case SDL_SCANCODE_ESCAPE:
                    keyboard.escape = pressed;
                    break;
                case SDL_SCANCODE_TAB:
                    keyboard.tab = pressed;
                    break;
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }
}

ControllerState* InputManager::getController(int index) {
    if (index < 0 || index >= MAX_CONTROLLERS) return nullptr;
    return &controllers[index];
}

const ControllerState* InputManager::getController(int index) const {
    if (index < 0 || index >= MAX_CONTROLLERS) return nullptr;
    return &controllers[index];
}

int InputManager::findUnassignedController() const {
    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        if (controllers[i].connected && controllers[i].playerSlot < 0) {
            return i;
        }
    }
    return -1;
}

void InputManager::assignController(int controllerIndex, int playerSlot) {
    if (controllerIndex >= 0 && controllerIndex < MAX_CONTROLLERS) {
        controllers[controllerIndex].playerSlot = playerSlot;
    }
}

void InputManager::unassignController(int controllerIndex) {
    if (controllerIndex >= 0 && controllerIndex < MAX_CONTROLLERS) {
        controllers[controllerIndex].playerSlot = -1;
    }
}

ControllerState* InputManager::getControllerForPlayer(int playerSlot) {
    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        if (controllers[i].connected && controllers[i].playerSlot == playerSlot) {
            return &controllers[i];
        }
    }
    return nullptr;
}

const ControllerState* InputManager::getControllerForPlayer(int playerSlot) const {
    for (int i = 0; i < MAX_CONTROLLERS; ++i) {
        if (controllers[i].connected && controllers[i].playerSlot == playerSlot) {
            return &controllers[i];
        }
    }
    return nullptr;
}

int InputManager::getConnectedControllerCount() const {
    int count = 0;
    for (const auto& controller : controllers) {
        if (controller.connected) ++count;
    }
    return count;
}

void InputManager::applyInputToBot(Bot& bot, int playerSlot) {
    const ControllerState* controller = getControllerForPlayer(playerSlot);

    // Store previous weapon/special state for press detection
    bool prevWeapon = bot.inputWeapon;
    bool prevSpecial = bot.inputSpecial;
    bool prevPowerup = bot.inputPowerup;

    // Reset input
    bot.inputForward = false;
    bot.inputBack = false;
    bot.inputLeft = false;
    bot.inputRight = false;
    bot.inputWeapon = false;
    bot.inputSpecial = false;
    bot.inputPowerup = false;
    bot.stickX = 0.0f;
    bot.stickY = 0.0f;
    bot.throttle = 0.0f;
    bot.reverse = 0.0f;

    if (controller) {
        // Steering with left stick
        bot.stickX = controller->leftStickX;
        bot.stickY = controller->leftStickY;

        // Tank controls: Right shoulder/trigger = throttle, Left = reverse
        if (controller->rightShoulder) {
            bot.throttle = 1.0f;
        } else if (controller->rightTrigger > 0.1f) {
            bot.throttle = controller->rightTrigger;
        }

        if (controller->leftShoulder) {
            bot.reverse = 1.0f;
        } else if (controller->leftTrigger > 0.1f) {
            bot.reverse = controller->leftTrigger;
        }

        // D-pad for digital turning (legacy support)
        bot.inputLeft = controller->dpadLeft;
        bot.inputRight = controller->dpadRight;

        // Buttons: B = attack, A = special
        bot.inputWeapon = controller->buttonB;
        bot.inputSpecial = controller->buttonA;
        bot.inputPowerup = controller->buttonX || controller->buttonY;
    }

    // Player 0 can also use keyboard
    if (playerSlot == 0) {
        // W/Up = throttle, S/Down = reverse
        if (keyboard.up) bot.throttle = 1.0f;
        if (keyboard.down) bot.reverse = 1.0f;
        if (keyboard.left) bot.inputLeft = true;
        if (keyboard.right) bot.inputRight = true;
        if (keyboard.space) bot.inputWeapon = true;
        if (keyboard.shift) bot.inputSpecial = true;
        if (keyboard.tab) bot.inputPowerup = true;
    }

    // Detect button presses
    bot.inputWeaponPressed = bot.inputWeapon && !prevWeapon;
    bot.inputSpecialPressed = bot.inputSpecial && !prevSpecial;
    bot.inputPowerupPressed = bot.inputPowerup && !prevPowerup;
}

} // namespace ScrapHeap
