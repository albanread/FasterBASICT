//
// fbsh_voices.mm
// FasterBASIC Voice Shell - Interactive BASIC with Voice Synthesis
//

#include "../shell/shell_core.h"
#include "../runtime/terminal_io.h"
#include "modular_commands.h"
#include "command_registry_core.h"
#include "basic_formatter_lib.h"
#include "../../Framework/Audio/AudioManager.h"
#include "../../Framework/API/st_api_context.h"
#include "../../Framework/API/superterminal_api.h"
#include "../../FBRunner3/register_voice.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unistd.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace FasterBASIC;
using namespace FasterBASIC::ModularCommands;
using namespace SuperTerminal;
using namespace STApi;
using namespace FBRunner3::VoiceRegistration;

// Global AudioManager for terminal mode (includes VoiceController)
static std::shared_ptr<AudioManager> g_audioManager;

// fbsh_voices-specific Lua bindings
static int lua_wait(lua_State* L) {
    // WAIT n - waits for n/60 seconds (n is in 60ths of a second)
    double sixtieths = lua_tonumber(L, 1);
    double seconds = sixtieths / 60.0;
    int milliseconds = static_cast<int>(seconds * 1000.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
}

static int lua_shouldStopScript(lua_State* L) {
    // Check if the script should be cancelled
    bool shouldStop = STApi::Context::instance().shouldStopScript();
    lua_pushboolean(L, shouldStop);
    return 1;
}

// Voice Lua bindings - minimal set for fbsh_voices
static int lua_st_voices_are_playing(lua_State* L) {
    // Force volatile to prevent optimization/caching
    volatile int playing = st_voices_are_playing();
    fprintf(stderr, "lua_st_voices_are_playing: st_voices_are_playing() returned %d\n", (int)playing);
    fflush(stderr);
    lua_pushinteger(L, playing);  // Push as integer instead of boolean
    fprintf(stderr, "lua_st_voices_are_playing: pushed integer %d to Lua\n", (int)playing);
    fflush(stderr);
    return 1;
}

#define VOICE_BINDING(name, func) \
    static int lua_##func(lua_State* L) { \
        int voice = lua_tointeger(L, 1); \
        auto val = lua_tonumber(L, 2); \
        func(voice, val); \
        return 0; \
    }

VOICE_BINDING(voice_set_waveform, st_voice_set_waveform)
VOICE_BINDING(voice_set_frequency, st_voice_set_frequency)
VOICE_BINDING(voice_set_note, st_voice_set_note)
VOICE_BINDING(voice_set_volume, st_voice_set_volume)
VOICE_BINDING(voice_set_gate, st_voice_set_gate)
VOICE_BINDING(voice_set_pulse_width, st_voice_set_pulse_width)
VOICE_BINDING(voice_set_pan, st_voice_set_pan)

static int lua_st_voice_set_envelope(lua_State* L) {
    int voice = lua_tointeger(L, 1);
    int attack = lua_tointeger(L, 2);
    int decay = lua_tointeger(L, 3);
    double sustain = lua_tonumber(L, 4);
    int release = lua_tointeger(L, 5);
    st_voice_set_envelope(voice, attack, decay, sustain, release);
    return 0;
}

static int lua_st_voice_set_filter_routing(lua_State* L) {
    int voice = lua_tointeger(L, 1);
    int enabled = lua_tointeger(L, 2);
    st_voice_set_filter_routing(voice, enabled);
    return 0;
}

static int lua_st_voice_set_filter_type(lua_State* L) {
    int type = lua_tointeger(L, 1);
    st_voice_set_filter_type(type);
    return 0;
}

static int lua_st_voice_set_filter_cutoff(lua_State* L) {
    double cutoff = lua_tonumber(L, 1);
    st_voice_set_filter_cutoff(cutoff);
    return 0;
}

static int lua_st_voice_set_filter_resonance(lua_State* L) {
    double resonance = lua_tonumber(L, 1);
    st_voice_set_filter_resonance(resonance);
    return 0;
}

static int lua_st_voices_start(lua_State* L) {
    st_voices_start();
    return 0;
}

static int lua_st_voice_wait(lua_State* L) {
    double beats = lua_tonumber(L, 1);
    st_voice_wait(beats);
    return 0;
}

static int lua_st_voices_set_tempo(lua_State* L) {
    int bpm = lua_tointeger(L, 1);
    st_voices_set_tempo(bpm);
    return 0;
}

static int lua_st_voices_end_play(lua_State* L) {
    st_voices_end_play();
    return 0;
}

static int lua_st_voice_reset_all(lua_State* L) {
    st_voice_reset_all();
    return 0;
}

static int lua_st_wait_ms(lua_State* L) {
    int milliseconds = luaL_checkinteger(L, 1);
    st_wait_ms(milliseconds);
    return 0;
}

void registerFBSHVoicesLuaBindings(lua_State* L) {
    // Register fbsh_voices-specific Lua functions
    lua_register(L, "wait", lua_wait);
    lua_register(L, "wait_ms", lua_st_wait_ms);
    lua_register(L, "shouldStopScript", lua_shouldStopScript);

    // Register voice system Lua functions
    lua_register(L, "voices_are_playing", lua_st_voices_are_playing);
    lua_register(L, "voice_set_waveform", lua_st_voice_set_waveform);
    lua_register(L, "voice_set_frequency", lua_st_voice_set_frequency);
    lua_register(L, "voice_set_note", lua_st_voice_set_note);
    lua_register(L, "voice_set_volume", lua_st_voice_set_volume);
    lua_register(L, "voice_set_gate", lua_st_voice_set_gate);
    lua_register(L, "voice_set_pulse_width", lua_st_voice_set_pulse_width);
    lua_register(L, "voice_set_pan", lua_st_voice_set_pan);
    lua_register(L, "voice_set_envelope", lua_st_voice_set_envelope);
    lua_register(L, "voice_set_filter_routing", lua_st_voice_set_filter_routing);
    lua_register(L, "voice_set_filter_type", lua_st_voice_set_filter_type);
    lua_register(L, "voice_set_filter_cutoff", lua_st_voice_set_filter_cutoff);
    lua_register(L, "voice_set_filter_resonance", lua_st_voice_set_filter_resonance);
    lua_register(L, "voices_start", lua_st_voices_start);
    lua_register(L, "voice_wait", lua_st_voice_wait);
    lua_register(L, "voices_set_tempo", lua_st_voices_set_tempo);
    lua_register(L, "voices_end_play", lua_st_voices_end_play);
    lua_register(L, "voice_reset_all", lua_st_voice_reset_all);
}

void initializeFBSHVoicesCommandRegistry() {
    CommandRegistry& registry = getGlobalCommandRegistry();
    CoreCommandRegistry::registerCoreCommands(registry);
    CoreCommandRegistry::registerCoreFunctions(registry);

    // Register voice commands and functions using shared registration
    registerVoiceCommands(registry);
    registerVoiceFunctions(registry);

    // WAIT command - fbsh_voices only - waits n/60 seconds (n is 60ths of a second)
    CommandDefinition wait("WAIT", "Wait for N/60 seconds (N in 60ths of second)", "wait", "timing");
    wait.addParameter("sixtieths", ParameterType::FLOAT, "Number of 60ths of a second to wait");
    registry.registerCommand(std::move(wait));

    // WAIT_MS command - wait for specific number of milliseconds
    CommandDefinition waitMs("WAIT_MS", "Wait for specified number of milliseconds", "wait_ms", "timing");
    waitMs.addParameter("milliseconds", ParameterType::INT, "Number of milliseconds to wait");
    registry.registerCommand(std::move(waitMs));

    CommandDefinition cls("CLS", "Clear the terminal screen", "basic_cls", "io");
    registry.registerCommand(std::move(cls));

    // Mark registry as initialized so parser doesn't clear it
    FasterBASIC::ModularCommands::markGlobalRegistryInitialized();

    // Set the global function pointer for fbsh_voices-specific Lua bindings
    FasterBASIC::g_additionalLuaBindings = registerFBSHVoicesLuaBindings;
}

void showWelcome() {
    std::cout << "FasterBASIC 2025 with Voice Synthesis\n";
    std::cout << "8 voices @ 48kHz\n";
    std::cout << "Ready.\n";
}

int main(int argc, char* argv[]) {
    // Initialize AudioManager (includes VoiceController + CoreAudio playback)
    g_audioManager = std::make_shared<AudioManager>();
    if (!g_audioManager->initialize()) {
        std::cerr << "Failed to initialize audio system\n";
        return 1;
    }

    // Register AudioManager with API context so st_api_audio.cpp can find it
    Context::instance().setAudio(g_audioManager);

    // Initialize command registry
    initializeFBSHVoicesCommandRegistry();

    bool verbose = false;
    bool debug = false;
    bool runMode = false;
    float waitSeconds = 5.0;  // Default wait time in run mode
    std::string loadFile;

    // Parse command line
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "FasterBASIC Voice Shell\n";
            std::cout << "Usage: " << argv[0] << " [options] [file.bas]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help     Show this help message\n";
            std::cout << "  -v, --verbose  Verbose output\n";
            std::cout << "  -d, --debug    Debug mode\n";
            std::cout << "  -r <file>      Run file and exit (no interactive mode)\n";
            std::cout << "  -w <seconds>   Wait time before exit in run mode (default: 5.0)\n";
            std::cout << "  file.bas       Load file and enter interactive mode\n";
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-d" || arg == "--debug") {
            debug = true;
        } else if (arg == "-r") {
            runMode = true;
            if (i + 1 < argc) {
                loadFile = argv[++i];
            } else {
                std::cerr << "Error: -r requires a filename\n";
                return 1;
            }
        } else if (arg == "-w") {
            if (i + 1 < argc) {
                try {
                    waitSeconds = std::stof(argv[++i]);
                    if (waitSeconds < 0) {
                        std::cerr << "Error: wait time must be non-negative\n";
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid wait time value\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: -w requires a number (seconds)\n";
                return 1;
            }
        } else if (arg.length() > 0 && arg[0] != '-') {
            loadFile = arg;
        }
    }

    try {
        ShellCore shell;
        shell.setVerbose(verbose);
        shell.setDebug(debug);

        if (!runMode) {
            showWelcome();
        }

        if (!loadFile.empty()) {
            if (runMode) {
                // Run mode: Load and execute, then exit
                std::cout << "Running \"" << loadFile << "\"...\n";

                // Read the file content
                std::ifstream inFile(loadFile);
                if (!inFile.is_open()) {
                    std::cerr << "Error: Cannot open file: " << loadFile << "\n";
                    g_audioManager->shutdown();
                    g_audioManager.reset();
                    return 1;
                }

                std::stringstream buffer;
                buffer << inFile.rdbuf();
                std::string source = buffer.str();
                inFile.close();

                // Format the program (adds line numbers if needed)
                FasterBASIC::FormatterOptions opts;
                opts.start_line = 10;
                opts.step = 10;
                FasterBASIC::FormatterResult result = FasterBASIC::formatBasicCode(source, opts);

                if (!result.success) {
                    std::cerr << "Error: Failed to format program: " << result.error_message << "\n";
                    g_audioManager->shutdown();
                    g_audioManager.reset();
                    return 1;
                }

                // Write to temporary file
                std::string tempFile = "/tmp/fbsh_voices_temp.bas";
                std::ofstream outFile(tempFile);
                if (!outFile.is_open()) {
                    std::cerr << "Error: Cannot create temporary file\n";
                    g_audioManager->shutdown();
                    g_audioManager.reset();
                    return 1;
                }
                outFile << result.formatted_code;
                outFile.close();

                // Load the formatted program
                if (!shell.loadProgram(tempFile)) {
                    std::cerr << "Error: Failed to load formatted program\n";
                    g_audioManager->shutdown();
                    g_audioManager.reset();
                    return 1;
                }

                // Execute the program directly
                std::cout << "Executing program...\n";
                std::cout.flush();
                if (!shell.runProgram()) {
                    std::cerr << "Error: Program execution failed\n";
                    g_audioManager->shutdown();
                    g_audioManager.reset();
                    return 1;
                }
                std::cout << "Program finished executing.\n";
                std::cout << "Waiting " << waitSeconds << " seconds for audio playback to complete...\n";
                std::cout.flush();
                // Wait for audio to finish
                int waitMs = static_cast<int>(waitSeconds * 1000);
                std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
                std::cout << "Audio wait complete, shutting down...\n";
                std::cout.flush();

                // Cleanup
                g_audioManager->shutdown();
                g_audioManager.reset();

                // Force immediate exit to prevent hanging threads
                _exit(0);
            } else {
                // Interactive mode: Load and enter shell
                std::cout << "Loading \"" << loadFile << "\"...\n";
                shell.loadProgram(loadFile);
                // ShellCore will register voice bindings automatically via #ifdef
                // FBTBindings calls st_voice_*() which uses ST_CONTEXT.audio()
                shell.run();
            }
        } else {
            // No file specified, enter interactive mode
            // ShellCore will register voice bindings automatically via #ifdef
            // FBTBindings calls st_voice_*() which uses ST_CONTEXT.audio()
            shell.run();
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        g_audioManager->shutdown();
        g_audioManager.reset();
        _exit(1);
    }

    // Cleanup (for interactive mode only - run mode exits above)
    g_audioManager->shutdown();
    g_audioManager.reset();

    return 0;
}
