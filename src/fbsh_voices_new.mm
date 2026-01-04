//
// fbsh.cpp
// FasterBASIC Shell - Interactive BASIC Shell
//
// Main executable for the FasterBASIC interactive shell.
// Provides a classic BASIC programming environment with line numbers,
// immediate mode, and traditional commands like LIST, RUN, LOAD, SAVE.
//

#include "../shell/shell_core.h"
#include "../runtime/terminal_io.h"
#include "modular_commands.h"
#include "command_registry_core.h"
#include <iostream>
#include <string>
#include <cstdlib>

using namespace FasterBASIC;
using namespace FasterBASIC::ModularCommands;

void initializeFBSHCommandRegistry() {
    // Initialize global registry with core commands for shell use
    CommandRegistry& registry = getGlobalCommandRegistry();
    
    // Add core BASIC commands and functions
    CoreCommandRegistry::registerCoreCommands(registry);
    CoreCommandRegistry::registerCoreFunctions(registry);
    
    // Add shell-specific I/O commands
    // CLS - Clear terminal screen (Unix terminal implementation)
    CommandDefinition cls("CLS",
                         "Clear the terminal screen",
                         "basic_cls", "io");
    registry.registerCommand(std::move(cls));
    
    // Add shell-specific commands (LIST, RUN, LOAD, SAVE, etc.)
    // TODO: Create shell command registry for commands like:
    // - LIST (list program lines)
    // - RUN (execute program)
    // - LOAD/SAVE (file operations)
    // - NEW (clear program)
    // - AUTO (auto line numbering)
    // - RENUM (renumber lines)
}

void showUsage(const char* programName) {
    std::cout << "FasterBASIC Shell v1.0 - Interactive BASIC Programming Environment\n";
    std::cout << "\n";
    std::cout << "Usage: " << programName << " [options] [file.bas]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "  -v, --verbose  Enable verbose output\n";
    std::cout << "  -d, --debug    Enable debug mode\n";
    std::cout << "  --version      Show version information\n";
    std::cout << "\n";
    std::cout << "If a .bas file is specified, it will be loaded automatically.\n";
    std::cout << "\n";
    std::cout << "Interactive Commands:\n";
    std::cout << "  LIST           List program lines\n";
    std::cout << "  LIST 10-50     List lines 10 through 50\n";
    std::cout << "  RUN            Execute the program\n";
    std::cout << "  NEW            Clear current program\n";
    std::cout << "  LOAD \"file\"    Load program from file\n";
    std::cout << "  SAVE \"file\"    Save program to file\n";
    std::cout << "  AUTO           Enable automatic line numbering\n";
    std::cout << "  RENUM          Renumber program lines\n";
    std::cout << "  HELP           Show help information\n";
    std::cout << "  QUIT           Exit the shell\n";
    std::cout << "\n";
    std::cout << "Program Entry:\n";
    std::cout << "  10 PRINT \"Hello\"   Add/replace line 10\n";
    std::cout << "  10               Delete line 10\n";
    std::cout << "\n";
}

void showVersion() {
    std::cout << "FasterBASIC Shell v1.0\n";
    std::cout << "Built on " << __DATE__ << " at " << __TIME__ << "\n";
    std::cout << "Copyright (c) 2024 FasterBASIC Project\n";
    std::cout << "\n";
    std::cout << "Features:\n";
    std::cout << "  - Interactive BASIC programming\n";
    std::cout << "  - Line-based program entry\n";
    std::cout << "  - Classic BASIC commands (LIST, RUN, LOAD, SAVE)\n";
    std::cout << "  - Terminal I/O with colors and positioning\n";
    std::cout << "  - LuaJIT-powered execution\n";
    std::cout << "  - Cross-platform compatibility\n";
}

void showWelcome() {
    std::cout << "FasterBASIC 2025\nReady.\n";
}

int main(int argc, char* argv[]) {
    // Initialize modular commands registry
    initializeFBSHCommandRegistry();
    
    bool verbose = false;
    bool debug = false;
    std::string loadFile;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            showUsage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            showVersion();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-d" || arg == "--debug") {
            debug = true;
        } else if (arg.length() > 0 && arg[0] != '-') {
            // Assume it's a filename to auto-load
            loadFile = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return 1;
        }
    }
    
    try {
        // Initialize the shell
        ShellCore shell;
        shell.setVerbose(verbose);
        shell.setDebug(debug);
        
        if (verbose) {
            std::cout << "Starting FasterBASIC Shell...\n";
            std::cout << "Verbose mode: ON\n";
            if (debug) {
                std::cout << "Debug mode: ON\n";
            }
        }
        
        // Show welcome message
        showWelcome();
        
        // Auto-load file if specified
        if (!loadFile.empty()) {
            std::cout << "Loading \"" << loadFile << "\"...\n";
            if (shell.loadProgram(loadFile)) {
                std::cout << "Program loaded successfully.\n";
            } else {
                std::cout << "Failed to load program.\n";
            }
            std::cout << "\n";
        }
        
        // Run the interactive shell
        shell.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
    
    if (verbose) {
        std::cout << "FasterBASIC Shell terminated normally.\n";
    }
    
    return 0;
}