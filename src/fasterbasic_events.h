//
// fasterbasic_events.h
// FasterBASIC - Event System
//
// Modular event system for ON <event> CALL/GOTO/GOSUB handlers
// Supports keyboard, mouse, joystick, and system events
//

#ifndef FASTERBASIC_EVENTS_H
#define FASTERBASIC_EVENTS_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace FasterBASIC {

// =============================================================================
// Event Types
// =============================================================================

/// Event categories for modular organization
enum class EventCategory {
    INPUT,      // Keyboard, mouse, joystick
    SYSTEM,     // Timer, frame, etc.
    CUSTOM,     // User-defined events
    NETWORK,    // Future: network events
    FILE        // Future: file system events
};

/// Core event types
enum class EventType {
    // === INPUT EVENTS ===
    
    // Keyboard Events
    KEYPRESSED,         // Any key pressed (sets INKEY$)
    KEY_UP,            // Any key released
    KEY_DOWN,          // Any key held down
    
    // Mouse Events  
    LEFT_MOUSE,        // Left mouse button pressed
    RIGHT_MOUSE,       // Right mouse button pressed
    MIDDLE_MOUSE,      // Middle mouse button pressed
    MOUSE_MOVE,        // Mouse moved
    MOUSE_WHEEL,       // Mouse wheel scrolled
    
    // Joystick/Gamepad Events
    FIRE_BUTTON,       // Primary fire button (joystick button 0)
    FIRE2_BUTTON,      // Secondary fire button (joystick button 1)
    JOYSTICK_UP,       // Joystick/D-pad up
    JOYSTICK_DOWN,     // Joystick/D-pad down
    JOYSTICK_LEFT,     // Joystick/D-pad left
    JOYSTICK_RIGHT,    // Joystick/D-pad right
    
    // === SYSTEM EVENTS ===
    
    // Timing Events
    TIMER,             // Timer expired (ON TIMER)
    FRAME,             // Frame/vertical blank (60Hz)
    SECOND,            // Every second
    
    // Program Events
    ERROR_EVENT,       // Runtime error occurred
    BREAK,             // User pressed Ctrl+C/Break
    
    // === FUTURE EXPANSION ===
    
    // Window Events (future)
    WINDOW_RESIZE,     // Window resized
    WINDOW_FOCUS,      // Window gained focus
    WINDOW_BLUR,       // Window lost focus
    
    // Network Events (future)
    NETWORK_CONNECT,   // Network connection established
    NETWORK_DATA,      // Network data received
    
    // Custom Events (future)
    USER_EVENT         // Custom user-defined events
};

/// Event handler types
enum class HandlerType {
    CALL,              // ON EVENT CALL function
    GOTO,              // ON EVENT GOTO line
    GOSUB              // ON EVENT GOSUB line
};

// =============================================================================
// Event Handler Definition
// =============================================================================

struct EventHandler {
    EventType event;
    HandlerType type;
    std::string target;        // Function name or line number
    int lineNumber;            // Source line where handler was defined
    bool enabled;              // Can be disabled with EVENT OFF
    
    EventHandler()
        : event(EventType::KEYPRESSED)
        , type(HandlerType::CALL)
        , lineNumber(0)
        , enabled(true)
    {}
    
    EventHandler(EventType evt, HandlerType hdlType, const std::string& tgt, int line = 0)
        : event(evt)
        , type(hdlType)
        , target(tgt)
        , lineNumber(line)
        , enabled(true)
    {}
};

// =============================================================================
// Event State and Management
// =============================================================================

/// Current state of all input devices
struct EventState {
    // Keyboard state
    std::string lastKeyPressed;    // INKEY$ value
    bool keyDown[256];            // Key states for extended checking
    
    // Mouse state
    int mouseX, mouseY;           // Current mouse position
    bool leftButton;              // Left button state
    bool rightButton;             // Right button state  
    bool middleButton;            // Middle button state
    float wheelDelta;             // Mouse wheel delta
    
    // Joystick state
    bool fireButton;              // Primary fire button
    bool fire2Button;             // Secondary fire button
    bool joyUp, joyDown;          // D-pad/stick directions
    bool joyLeft, joyRight;
    
    // System state
    double timerValue;            // Current timer value
    bool breakPressed;            // Break/Ctrl+C pressed
    
    // Constructor
    EventState() {
        reset();
    }
    
    void reset() {
        lastKeyPressed = "";
        for (int i = 0; i < 256; i++) keyDown[i] = false;
        mouseX = mouseY = 0;
        leftButton = rightButton = middleButton = false;
        wheelDelta = 0.0f;
        fireButton = fire2Button = false;
        joyUp = joyDown = joyLeft = joyRight = false;
        timerValue = 0.0;
        breakPressed = false;
    }
};

/// Event registry and management
class EventManager {
public:
    EventManager();
    ~EventManager();
    
    // Handler registration
    void registerHandler(const EventHandler& handler);
    void removeHandler(EventType event);
    void enableHandler(EventType event, bool enabled = true);
    void disableHandler(EventType event) { enableHandler(event, false); }
    void clearAllHandlers();
    
    // Event state management
    void updateEventState(const EventState& newState);
    EventState& getEventState() { return currentState_; }
    const EventState& getEventState() const { return currentState_; }
    
    // Event checking and processing
    std::vector<EventHandler> checkTriggeredEvents();
    bool isEventTriggered(EventType event) const;
    
    // Event information
    std::string getEventName(EventType event) const;
    EventCategory getEventCategory(EventType event) const;
    std::vector<EventType> getAvailableEvents() const;
    
    // Enable/disable event checking globally
    void setEventsEnabled(bool enabled) { eventsEnabled_ = enabled; }
    bool areEventsEnabled() const { return eventsEnabled_; }
    
    // Access to name mapping (needed by global functions)
    const std::map<std::string, EventType>& getNameToEventMap() const { return nameToEvent_; }

private:
    std::map<EventType, EventHandler> handlers_;
    EventState currentState_;
    EventState previousState_;
    bool eventsEnabled_;
    
    // Internal helpers
    bool checkKeyboardEvents(EventType event) const;
    bool checkMouseEvents(EventType event) const;
    bool checkJoystickEvents(EventType event) const;
    bool checkSystemEvents(EventType event) const;
    
    // Event name mapping
    void initializeEventNames();
    std::map<EventType, std::string> eventNames_;
    std::map<std::string, EventType> nameToEvent_;
};

// =============================================================================
// Global Event System Functions
// =============================================================================

/// Get global event manager instance
EventManager& getEventManager();

/// Parse event name string to EventType
bool parseEventName(const std::string& name, EventType& outEvent);

/// Get event name string from EventType
std::string getEventNameString(EventType event);

/// Check if event name is valid
bool isValidEventName(const std::string& name);

/// Get list of all available event names
std::vector<std::string> getAllEventNames();

// =============================================================================
// Event System Integration Macros
// =============================================================================

// Helper macros for easier event handling in generated code
#define BASIC_EVENT_CHECK(mgr, event) ((mgr).isEventTriggered(EventType::event))
#define BASIC_EVENT_REGISTER(mgr, event, type, target, line) \
    (mgr).registerHandler(EventHandler(EventType::event, HandlerType::type, target, line))

} // namespace FasterBASIC

#endif // FASTERBASIC_EVENTS_H