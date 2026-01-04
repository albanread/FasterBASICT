// voice_registration_stubs.cpp
// Stub implementations for VoiceRegistration functions used by fbsh
// These provide empty implementations when voice controller functionality
// is not available in the standalone shell

#include "../src/fasterbasic_semantic.h"
#include "lua.hpp"

namespace FBRunner3 {
namespace VoiceRegistration {

// Stub implementation - does nothing in the shell
void registerVoiceConstants(FasterBASIC::ConstantsManager& constants) {
    // No-op in shell - voice constants not needed for command-line operation
}

// Stub implementation - does nothing in the shell
void registerVoiceLuaBindings(lua_State* L) {
    // No-op in shell - voice bindings not needed for command-line operation
}

} // namespace VoiceRegistration
} // namespace FBRunner3