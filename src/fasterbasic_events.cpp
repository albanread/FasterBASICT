//
// fasterbasic_events.cpp
// FasterBASIC - Event System Implementation
//
// Modular event system for ON <event> CALL/GOTO/GOSUB handlers
//

#include "fasterbasic_events.h"
#include <algorithm>
#include <cstring>

namespace FasterBASIC {

// =============================================================================
// Global Event Manager Instance
// =============================================================================

static EventManager* g_eventManager = nullptr;

EventManager& getEventManager() {
    if (!g_eventManager) {
        g_eventManager = new EventManager();
    }
    return *g_eventManager;
}

// =============================================================================
// EventManager Implementation
// =============================================================================

EventManager::EventManager() 
    : eventsEnabled_(true)
{
    initializeEventNames();
    currentState_.reset();
    previousState_.reset();
}

EventManager::~EventManager() {
    clearAllHandlers();
}

void EventManager::registerHandler(const EventHandler& handler) {
    handlers_[handler.event] = handler;
}

void EventManager::removeHandler(EventType event) {
    handlers_.erase(event);
}

void EventManager::enableHandler(EventType event, bool enabled) {
    auto it = handlers_.find(event);
    if (it != handlers_.end()) {
        it->second.enabled = enabled;
    }
}

void EventManager::clearAllHandlers() {
    handlers_.clear();
}

void EventManager::updateEventState(const EventState& newState) {
    previousState_ = currentState_;
    currentState_ = newState;
}

std::vector<EventHandler> EventManager::checkTriggeredEvents() {
    std::vector<EventHandler> triggeredEvents;
    
    if (!eventsEnabled_) {
        return triggeredEvents;
    }
    
    for (const auto& pair : handlers_) {
        const EventHandler& handler = pair.second;
        
        if (handler.enabled && isEventTriggered(handler.event)) {
            triggeredEvents.push_back(handler);
        }
    }
    
    return triggeredEvents;
}

bool EventManager::isEventTriggered(EventType event) const {
    if (!eventsEnabled_) {
        return false;
    }
    
    switch (getEventCategory(event)) {
        case EventCategory::INPUT:
            if (checkKeyboardEvents(event)) return true;
            if (checkMouseEvents(event)) return true;
            if (checkJoystickEvents(event)) return true;
            break;
            
        case EventCategory::SYSTEM:
            if (checkSystemEvents(event)) return true;
            break;
            
        case EventCategory::CUSTOM:
        case EventCategory::NETWORK:
        case EventCategory::FILE:
            // Future expansion
            break;
    }
    
    return false;
}

bool EventManager::checkKeyboardEvents(EventType event) const {
    switch (event) {
        case EventType::KEYPRESSED:
            // Triggered if a new key was pressed (wasn't pressed before, is pressed now)
            return !currentState_.lastKeyPressed.empty() && 
                   currentState_.lastKeyPressed != previousState_.lastKeyPressed;
            
        case EventType::KEY_UP:
            // Triggered if any key was released
            for (int i = 0; i < 256; i++) {
                if (previousState_.keyDown[i] && !currentState_.keyDown[i]) {
                    return true;
                }
            }
            return false;
            
        case EventType::KEY_DOWN:
            // Triggered if any key is currently held down
            for (int i = 0; i < 256; i++) {
                if (currentState_.keyDown[i]) {
                    return true;
                }
            }
            return false;
            
        default:
            return false;
    }
}

bool EventManager::checkMouseEvents(EventType event) const {
    switch (event) {
        case EventType::LEFT_MOUSE:
            // Triggered on mouse button press (edge detection)
            return currentState_.leftButton && !previousState_.leftButton;
            
        case EventType::RIGHT_MOUSE:
            return currentState_.rightButton && !previousState_.rightButton;
            
        case EventType::MIDDLE_MOUSE:
            return currentState_.middleButton && !previousState_.middleButton;
            
        case EventType::MOUSE_MOVE:
            // Triggered if mouse position changed
            return (currentState_.mouseX != previousState_.mouseX) ||
                   (currentState_.mouseY != previousState_.mouseY);
                   
        case EventType::MOUSE_WHEEL:
            // Triggered if wheel was scrolled
            return currentState_.wheelDelta != 0.0f;
            
        default:
            return false;
    }
}

bool EventManager::checkJoystickEvents(EventType event) const {
    switch (event) {
        case EventType::FIRE_BUTTON:
            return currentState_.fireButton && !previousState_.fireButton;
            
        case EventType::FIRE2_BUTTON:
            return currentState_.fire2Button && !previousState_.fire2Button;
            
        case EventType::JOYSTICK_UP:
            return currentState_.joyUp && !previousState_.joyUp;
            
        case EventType::JOYSTICK_DOWN:
            return currentState_.joyDown && !previousState_.joyDown;
            
        case EventType::JOYSTICK_LEFT:
            return currentState_.joyLeft && !previousState_.joyLeft;
            
        case EventType::JOYSTICK_RIGHT:
            return currentState_.joyRight && !previousState_.joyRight;
            
        default:
            return false;
    }
}

bool EventManager::checkSystemEvents(EventType event) const {
    switch (event) {
        case EventType::TIMER:
            // Timer events are typically handled by comparing against a target time
            // For now, just check if timer value changed significantly
            return (currentState_.timerValue != previousState_.timerValue);
            
        case EventType::FRAME:
            // Frame events would typically be triggered every 1/60th second
            // This would need to be driven by the main rendering loop
            return false; // Placeholder
            
        case EventType::SECOND:
            // Second events would be triggered every second
            // This would need to be driven by a system timer
            return false; // Placeholder
            
        case EventType::ERROR_EVENT:
            // Error events would be triggered by the runtime error handler
            return false; // Placeholder
            
        case EventType::BREAK:
            return currentState_.breakPressed && !previousState_.breakPressed;
            
        default:
            return false;
    }
}

std::string EventManager::getEventName(EventType event) const {
    auto it = eventNames_.find(event);
    if (it != eventNames_.end()) {
        return it->second;
    }
    return "UNKNOWN_EVENT";
}

EventCategory EventManager::getEventCategory(EventType event) const {
    switch (event) {
        // Input events
        case EventType::KEYPRESSED:
        case EventType::KEY_UP:
        case EventType::KEY_DOWN:
        case EventType::LEFT_MOUSE:
        case EventType::RIGHT_MOUSE:
        case EventType::MIDDLE_MOUSE:
        case EventType::MOUSE_MOVE:
        case EventType::MOUSE_WHEEL:
        case EventType::FIRE_BUTTON:
        case EventType::FIRE2_BUTTON:
        case EventType::JOYSTICK_UP:
        case EventType::JOYSTICK_DOWN:
        case EventType::JOYSTICK_LEFT:
        case EventType::JOYSTICK_RIGHT:
            return EventCategory::INPUT;
            
        // System events
        case EventType::TIMER:
        case EventType::FRAME:
        case EventType::SECOND:
        case EventType::ERROR_EVENT:
        case EventType::BREAK:
            return EventCategory::SYSTEM;
            
        // Future events
        case EventType::WINDOW_RESIZE:
        case EventType::WINDOW_FOCUS:
        case EventType::WINDOW_BLUR:
        case EventType::NETWORK_CONNECT:
        case EventType::NETWORK_DATA:
        case EventType::USER_EVENT:
            return EventCategory::CUSTOM;
            
        default:
            return EventCategory::CUSTOM;
    }
}

std::vector<EventType> EventManager::getAvailableEvents() const {
    std::vector<EventType> events;
    for (const auto& pair : eventNames_) {
        events.push_back(pair.first);
    }
    return events;
}

void EventManager::initializeEventNames() {
    // Input Events
    eventNames_[EventType::KEYPRESSED] = "KEYPRESSED";
    eventNames_[EventType::KEY_UP] = "KEY_UP";
    eventNames_[EventType::KEY_DOWN] = "KEY_DOWN";
    eventNames_[EventType::LEFT_MOUSE] = "LEFT_MOUSE";
    eventNames_[EventType::RIGHT_MOUSE] = "RIGHT_MOUSE";
    eventNames_[EventType::MIDDLE_MOUSE] = "MIDDLE_MOUSE";
    eventNames_[EventType::MOUSE_MOVE] = "MOUSE_MOVE";
    eventNames_[EventType::MOUSE_WHEEL] = "MOUSE_WHEEL";
    eventNames_[EventType::FIRE_BUTTON] = "FIRE_BUTTON";
    eventNames_[EventType::FIRE2_BUTTON] = "FIRE2_BUTTON";
    eventNames_[EventType::JOYSTICK_UP] = "JOYSTICK_UP";
    eventNames_[EventType::JOYSTICK_DOWN] = "JOYSTICK_DOWN";
    eventNames_[EventType::JOYSTICK_LEFT] = "JOYSTICK_LEFT";
    eventNames_[EventType::JOYSTICK_RIGHT] = "JOYSTICK_RIGHT";
    
    // System Events
    eventNames_[EventType::TIMER] = "TIMER";
    eventNames_[EventType::FRAME] = "FRAME";
    eventNames_[EventType::SECOND] = "SECOND";
    eventNames_[EventType::ERROR_EVENT] = "ERROR";
    eventNames_[EventType::BREAK] = "BREAK";
    
    // Future Events
    eventNames_[EventType::WINDOW_RESIZE] = "WINDOW_RESIZE";
    eventNames_[EventType::WINDOW_FOCUS] = "WINDOW_FOCUS";
    eventNames_[EventType::WINDOW_BLUR] = "WINDOW_BLUR";
    eventNames_[EventType::NETWORK_CONNECT] = "NETWORK_CONNECT";
    eventNames_[EventType::NETWORK_DATA] = "NETWORK_DATA";
    eventNames_[EventType::USER_EVENT] = "USER_EVENT";
    
    // Build reverse mapping
    for (const auto& pair : eventNames_) {
        nameToEvent_[pair.second] = pair.first;
    }
}

// =============================================================================
// Global Functions
// =============================================================================

bool parseEventName(const std::string& name, EventType& outEvent) {
    EventManager& mgr = getEventManager();
    
    // Convert to uppercase for case-insensitive matching
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    
    const auto& nameToEvent = mgr.getNameToEventMap();
    auto it = nameToEvent.find(upperName);
    if (it != nameToEvent.end()) {
        outEvent = it->second;
        return true;
    }
    
    return false;
}

std::string getEventNameString(EventType event) {
    return getEventManager().getEventName(event);
}

bool isValidEventName(const std::string& name) {
    EventType dummy;
    return parseEventName(name, dummy);
}

std::vector<std::string> getAllEventNames() {
    std::vector<std::string> names;
    EventManager& mgr = getEventManager();
    
    auto events = mgr.getAvailableEvents();
    for (EventType event : events) {
        names.push_back(mgr.getEventName(event));
    }
    
    std::sort(names.begin(), names.end());
    return names;
}



} // namespace FasterBASIC