#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <array>

namespace ScrapHeap {

// Forward declarations
struct Bot;

// Controller state
struct ControllerState {
    SDL_Gamepad* gamepad = nullptr;
    SDL_JoystickID id = 0;
    bool connected = false;
    int playerSlot = -1;  // -1 = unassigned

    // Button states (current frame)
    bool dpadUp = false;
    bool dpadDown = false;
    bool dpadLeft = false;
    bool dpadRight = false;
    bool buttonA = false;
    bool buttonB = false;
    bool buttonX = false;
    bool buttonY = false;
    bool buttonStart = false;

    // Button states (previous frame, for detecting presses)
    bool prevDpadUp = false;
    bool prevDpadDown = false;
    bool prevDpadLeft = false;
    bool prevDpadRight = false;
    bool prevButtonA = false;
    bool prevButtonB = false;
    bool prevButtonX = false;
    bool prevButtonY = false;
    bool prevButtonStart = false;

    // Analog sticks
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;

    // Triggers/shoulders for tank controls
    float leftTrigger = 0.0f;   // Reverse
    float rightTrigger = 0.0f;  // Throttle
    bool leftShoulder = false;
    bool rightShoulder = false;

    // Helpers for detecting button presses (not just held)
    bool dpadUpPressed() const { return dpadUp && !prevDpadUp; }
    bool dpadDownPressed() const { return dpadDown && !prevDpadDown; }
    bool dpadLeftPressed() const { return dpadLeft && !prevDpadLeft; }
    bool dpadRightPressed() const { return dpadRight && !prevDpadRight; }
    bool buttonAPressed() const { return buttonA && !prevButtonA; }
    bool buttonBPressed() const { return buttonB && !prevButtonB; }
    bool buttonXPressed() const { return buttonX && !prevButtonX; }
    bool buttonYPressed() const { return buttonY && !prevButtonY; }
    bool buttonStartPressed() const { return buttonStart && !prevButtonStart; }

    // Any direction pressed
    bool anyDirectionPressed() const {
        return dpadUpPressed() || dpadDownPressed() ||
               dpadLeftPressed() || dpadRightPressed();
    }
};

// Keyboard state (for player 1 / testing)
struct KeyboardState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool space = false;      // Weapon
    bool shift = false;      // Special
    bool enter = false;      // Confirm
    bool escape = false;     // Back
    bool tab = false;        // Powerup
    bool x = false;          // Add AI player

    // Previous frame
    bool prevUp = false;
    bool prevDown = false;
    bool prevLeft = false;
    bool prevRight = false;
    bool prevSpace = false;
    bool prevShift = false;
    bool prevEnter = false;
    bool prevEscape = false;
    bool prevTab = false;
    bool prevX = false;

    // Press detection
    bool upPressed() const { return up && !prevUp; }
    bool downPressed() const { return down && !prevDown; }
    bool leftPressed() const { return left && !prevLeft; }
    bool rightPressed() const { return right && !prevRight; }
    bool spacePressed() const { return space && !prevSpace; }
    bool shiftPressed() const { return shift && !prevShift; }
    bool enterPressed() const { return enter && !prevEnter; }
    bool escapePressed() const { return escape && !prevEscape; }
    bool tabPressed() const { return tab && !prevTab; }
    bool xPressed() const { return x && !prevX; }
};

// Input manager
class InputManager {
public:
    static InputManager& instance();

    // Initialize/shutdown
    void initialize();
    void shutdown();

    // Call at start of frame to save previous states
    void beginFrame();

    // Process SDL events
    void processEvent(const SDL_Event& event);

    // Get controller by index
    ControllerState* getController(int index);
    const ControllerState* getController(int index) const;

    // Get keyboard state
    KeyboardState& getKeyboard() { return keyboard; }
    const KeyboardState& getKeyboard() const { return keyboard; }

    // Find first unassigned controller
    int findUnassignedController() const;

    // Assign controller to player slot
    void assignController(int controllerIndex, int playerSlot);

    // Unassign controller from player slot
    void unassignController(int controllerIndex);

    // Get controller assigned to a player slot
    ControllerState* getControllerForPlayer(int playerSlot);
    const ControllerState* getControllerForPlayer(int playerSlot) const;

    // Number of connected controllers
    int getConnectedControllerCount() const;

    // Apply input to bot (combines controller + keyboard for player 0)
    void applyInputToBot(Bot& bot, int playerSlot);

private:
    InputManager() = default;

    static constexpr int MAX_CONTROLLERS = 8;
    static constexpr float STICK_DEADZONE = 0.2f;

    std::array<ControllerState, MAX_CONTROLLERS> controllers;
    KeyboardState keyboard;

    void updateControllerState(ControllerState& controller);
    float applyDeadzone(float value) const;
};

} // namespace ScrapHeap
