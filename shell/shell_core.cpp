//
// shell_core.cpp
// FasterBASIC Shell - Core Shell Functionality
//
// Main shell logic that ties together program management, command parsing,
// and program execution. Provides the interactive BASIC shell experience.
//

#include "shell_core.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../src/basic_formatter_lib.h"
#include "../src/fasterbasic_lexer.h"
#include "../src/fasterbasic_parser.h"
#include "../src/modular_commands.h"
#include "../src/fasterbasic_semantic.h"
#include "../src/fasterbasic_optimizer.h"
#include "../src/fasterbasic_peephole.h"
#include "../src/fasterbasic_cfg.h"
#include "../src/fasterbasic_ircode.h"
#include "../src/fasterbasic_lua_codegen.h"
#include "../runtime/data_lua_bindings.h"
#include "../runtime/terminal_lua_bindings.h"
#include "../runtime/DataManager.h"

#ifdef VOICE_CONTROLLER_ENABLED
#include "../../FBRunner3/FBTBindings.h"
#include "../../FBRunner3/register_voice.h"
#endif

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Runtime module registration functions
extern "C" void register_unicode_module(lua_State* L);
extern "C" void register_bitwise_module(lua_State* L);
extern "C" void register_constants_module(lua_State* L);
extern "C" void set_constants_manager(FasterBASIC::ConstantsManager* manager);

// Forward declare file I/O bindings
namespace FasterBASIC {
    void register_fileio_functions(lua_State* L);
    void clear_fileio_state();
    void registerDataBindings(lua_State* L);
    void registerTerminalBindings(lua_State* L);
    void initializeDataManager(const std::vector<DataValue>& values);
    void addDataRestorePoint(int lineNumber, size_t dataIndex);
    void addDataRestorePointByLabel(const std::string& label, size_t dataIndex);
}

namespace FasterBASIC {

// Global function pointer for additional Lua bindings (e.g., fbsh_voices-specific)
AdditionalLuaBindingsFunc g_additionalLuaBindings = nullptr;

// Static constants
const std::string ShellCore::SHELL_VERSION = "1.0";
const std::string ShellCore::SHELL_PROMPT = "Ready.";
const std::string ShellCore::TEMP_FILE_PREFIX = "/tmp/fasterbasic_";
const int ShellCore::MAX_LINE_LENGTH = 1024;

// Static instance for signal handling
ShellCore* ShellCore::s_instance = nullptr;

ShellCore::ShellCore()
    : m_terminal(&g_terminal)
    , m_running(false)
    , m_verbose(false)
    , m_debug(false)
    , m_programRunning(false)
    , m_continueFromLine(-1)
    , m_autoContinueMode(false)
    , m_lastLineNumber(0)
    , m_suggestedNextLine(0)
    , m_lastSearchLine(0)
    , m_lastContextLines(3)
    , m_hasActiveSearch(false)
    , m_historyIndex(-1)
{
    // Set up signal handler for Ctrl+C
    s_instance = this;
    signal(SIGINT, signalHandler);
    
    // Ensure BASIC directories exist
    ensureBasicDirectories();
}

ShellCore::~ShellCore() {
    // Clean up signal handler
    s_instance = nullptr;
    signal(SIGINT, SIG_DFL);
}

void ShellCore::run() {
    m_running = true;

    while (m_running) {
        showPrompt();
        std::string input = readInput();

        if (!input.empty()) {
            executeCommand(input);
        }
    }
}

void ShellCore::handleReset() {
    // Terminate any running program/script immediately
    stopExecution();
    
    // Reset shell state completely
    m_programRunning = false;
    m_continueFromLine = -1;
    m_autoContinueMode = false;
    m_lastLineNumber = 0;
    m_suggestedNextLine = 0;
    
    // Clear program manager auto mode
    m_program.setAutoMode(false);
    
    // Clear any temporary files or execution state
    if (!m_tempFilename.empty()) {
        std::remove(m_tempFilename.c_str());
        m_tempFilename.clear();
    }
    
    // Reset terminal to normal state (in case we were in raw mode)
    if (m_terminal) {
        // Ensure terminal is back to normal state
        std::cout << "\x1B[0m";  // Reset all terminal attributes
        std::cout.flush();
    }
    
    // Clear any pending input
    while (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.get();
    }
    
    // Show reset message and prompt
    std::cout << "\n\nRESET (use QUIT to exit)\n\nReady.\n";
    std::cout.flush();
}

void ShellCore::signalHandler(int signal) {
    if (signal == SIGINT && s_instance) {
        // Flush any pending output
        std::cout.flush();
        std::cerr.flush();
        
        // Reset terminal state if we were in raw mode
        struct termios oldTermios;
        tcgetattr(STDIN_FILENO, &oldTermios);
        oldTermios.c_lflag |= (ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
        
        s_instance->handleReset();
    }
}

void ShellCore::quit() {
    m_running = false;
}

bool ShellCore::isRunning() const {
    return m_running;
}

bool ShellCore::executeCommand(const std::string& input) {
    ParsedCommand cmd = m_parser.parse(input);

    if (m_parser.hasError()) {
        showError(m_parser.getLastError());
        return false;
    }

    // Add recognized commands to history (not line entries)
    bool isCommand = false;
    switch (cmd.type) {
        case ShellCommandType::LIST:
        case ShellCommandType::LIST_RANGE:
        case ShellCommandType::LIST_LINE:
        case ShellCommandType::LIST_FROM:
        case ShellCommandType::LIST_TO:
        case ShellCommandType::RUN:
        case ShellCommandType::RUN_FROM:
        case ShellCommandType::LOAD:
        case ShellCommandType::SAVE:
        case ShellCommandType::MERGE:
        case ShellCommandType::NEW:
        case ShellCommandType::AUTO:
        case ShellCommandType::AUTO_PARAMS:
        case ShellCommandType::RENUM:
        case ShellCommandType::RENUM_PARAMS:
        case ShellCommandType::EDIT:
        case ShellCommandType::FIND:
        case ShellCommandType::FINDNEXT:
        case ShellCommandType::REPLACE:
        case ShellCommandType::REPLACENEXT:
        case ShellCommandType::VARS:
        case ShellCommandType::CLEAR:
        case ShellCommandType::CHECK:
        case ShellCommandType::FORMAT:
        case ShellCommandType::CLS:
        case ShellCommandType::DIR:
        case ShellCommandType::HELP:
        case ShellCommandType::QUIT:
            isCommand = true;
            addToHistory(input);
            break;
        default:
            break;
    }

    switch (cmd.type) {
        case ShellCommandType::DIRECT_LINE:
            return handleDirectLine(cmd);

        case ShellCommandType::DELETE_LINE:
            return handleDeleteLine(cmd);

        case ShellCommandType::LIST:
        case ShellCommandType::LIST_RANGE:
        case ShellCommandType::LIST_LINE:
        case ShellCommandType::LIST_FROM:
        case ShellCommandType::LIST_TO:
            return handleList(cmd);

        case ShellCommandType::RUN:
        case ShellCommandType::RUN_FROM:
            return handleRun(cmd);

        case ShellCommandType::LOAD:
            return handleLoad(cmd);

        case ShellCommandType::SAVE:
            return handleSave(cmd);

        case ShellCommandType::MERGE:
            return handleMerge(cmd);

        case ShellCommandType::NEW:
            return handleNew(cmd);

        case ShellCommandType::AUTO:
        case ShellCommandType::AUTO_PARAMS:
            return handleAuto(cmd);

        case ShellCommandType::RENUM:
        case ShellCommandType::RENUM_PARAMS:
            return handleRenum(cmd);

        case ShellCommandType::EDIT:
            return handleEdit(cmd);

        case ShellCommandType::FIND:
            return handleFind(cmd);

        case ShellCommandType::FINDNEXT:
            return handleFindNext(cmd);

        case ShellCommandType::REPLACE:
            return handleReplace(cmd);

        case ShellCommandType::REPLACENEXT:
            return handleReplaceNext(cmd);

        case ShellCommandType::VARS:
            return handleVars(cmd);

        case ShellCommandType::CLEAR:
            return handleClear(cmd);

        case ShellCommandType::CHECK:
            return handleCheck(cmd);

        case ShellCommandType::FORMAT:
            return handleFormat(cmd);

        case ShellCommandType::CLS:
            return handleCls(cmd);

        case ShellCommandType::DIR:
            return handleDir(cmd);

        case ShellCommandType::HELP:
            return handleHelp(cmd);

        case ShellCommandType::QUIT:
            return handleQuit(cmd);

        case ShellCommandType::IMMEDIATE:
            return handleImmediate(cmd);

        case ShellCommandType::EMPTY:
            return true;  // Just show prompt again

        default:
            showError("Unknown or invalid command");
            return false;
    }
}

void ShellCore::showPrompt() {
    if (m_program.isAutoMode()) {
        int nextLine = m_program.getNextAutoLine();
        std::cout << nextLine << " ";
        std::cout.flush();
    } else if (m_autoContinueMode) {
        // Use inline editing for auto-continuation
        std::string result = readInputWithInlineEditing();
        if (result.empty()) {
            // Empty - exit auto-continue mode
            m_autoContinueMode = false;
            m_suggestedNextLine = 0;
            std::cout << "\nReady.\n";
        } else {
            // Check if user typed their own line number (starts with digit)
            if (!result.empty() && result[0] >= '0' && result[0] <= '9') {
                // User entered their own line number, exit auto-continue
                m_autoContinueMode = false;
                m_suggestedNextLine = 0;
                executeCommand(result);
            } else {
                // Process the line with our suggested line number
                std::string fullLine = std::to_string(m_suggestedNextLine) + " " + result;
                executeCommand(fullLine);
            }
        }
        return;
    }
}

std::string ShellCore::readInput() {
    if (m_autoContinueMode) {
        // Skip normal input when in auto-continue mode
        // (handled by showPrompt now)
        return "";
    }

    // Use history-aware input reading
    return readInputWithHistory();
}

void ShellCore::addToHistory(const std::string& command) {
    // Don't add empty commands or duplicates of the last command
    if (command.empty()) {
        return;
    }
    if (!m_commandHistory.empty() && m_commandHistory.back() == command) {
        return;
    }
    
    // Add to history
    m_commandHistory.push_back(command);
    
    // Keep only last MAX_HISTORY_SIZE commands
    if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
        m_commandHistory.erase(m_commandHistory.begin());
    }
    
    // Reset history index
    m_historyIndex = -1;
}

std::string ShellCore::readInputWithHistory() {
    std::string buffer;
    size_t cursorPos = 0;
    bool done = false;
    
    // Save current terminal settings
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    
    // Enable raw mode for character-by-character input
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
    
    // Start with empty buffer at prompt
    std::cout.flush();
    
    while (!done) {
        char ch = std::cin.get();
        
        if (ch == '\n' || ch == '\r') {
            // Enter - accept input
            done = true;
            std::cout << std::endl;
        } else if (ch == '\x1B') {  // ESC key
            // Check for arrow key sequences
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq2 = std::cin.get();
                switch (seq2) {
                    case 'A':  // Up arrow - previous command in history
                        if (!m_commandHistory.empty()) {
                            if (m_historyIndex == -1) {
                                m_historyIndex = m_commandHistory.size() - 1;
                            } else if (m_historyIndex > 0) {
                                m_historyIndex--;
                            }
                            buffer = m_commandHistory[m_historyIndex];
                            cursorPos = buffer.length();
                            // Redraw line
                            std::cout << "\r\x1B[K" << buffer;
                            std::cout.flush();
                        }
                        break;
                    case 'B':  // Down arrow - next command in history
                        if (!m_commandHistory.empty() && m_historyIndex != -1) {
                            if (m_historyIndex < (int)m_commandHistory.size() - 1) {
                                m_historyIndex++;
                                buffer = m_commandHistory[m_historyIndex];
                                cursorPos = buffer.length();
                            } else {
                                // Go past end of history - clear line
                                m_historyIndex = -1;
                                buffer.clear();
                                cursorPos = 0;
                            }
                            // Redraw line
                            std::cout << "\r\x1B[K" << buffer;
                            std::cout.flush();
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            std::cout << "\r\x1B[K" << buffer;
                            // Move cursor to position
                            if (cursorPos < buffer.length()) {
                                std::cout << "\r\x1B[" << (cursorPos + 1) << "C";
                            }
                            std::cout.flush();
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            std::cout << "\r\x1B[K" << buffer;
                            // Move cursor to position
                            if (cursorPos > 0) {
                                std::cout << "\r\x1B[" << (cursorPos + 1) << "C";
                            } else {
                                std::cout << "\r";
                            }
                            std::cout.flush();
                        }
                        break;
                    case 'H':  // Home key
                        cursorPos = 0;
                        std::cout << "\r\x1B[K" << buffer << "\r";
                        std::cout.flush();
                        break;
                    case 'F':  // End key
                        cursorPos = buffer.length();
                        std::cout << "\r\x1B[K" << buffer;
                        std::cout.flush();
                        break;
                }
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                // Redraw line
                std::cout << "\r\x1B[K" << buffer;
                if (cursorPos < buffer.length()) {
                    std::cout << "\r\x1B[" << (cursorPos + 1) << "C";
                }
                std::cout.flush();
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel input
            buffer.clear();
            done = true;
            std::cout << "^C\n";
        } else if (ch == '\x04') {  // Ctrl+D - EOF
            if (buffer.empty()) {
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                quit();
                return "";
            }
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            // Redraw line
            std::cout << "\r\x1B[K" << buffer;
            if (cursorPos < buffer.length()) {
                std::cout << "\r\x1B[" << (cursorPos + 1) << "C";
            }
            std::cout.flush();
        }
    }
    
    // Restore normal terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    
    return buffer;
}

bool ShellCore::runProgram(int startLine) {
    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }

    // Generate program text
    std::string programText;
    if (startLine <= 0) {
        programText = m_program.generateProgram();
    } else {
        programText = m_program.generateProgramRange(startLine, -1);
    }

    return executeCompiledProgram(programText, startLine);
}

bool ShellCore::continueExecution() {
    // TODO: Implement continue execution functionality
    showError("Continue execution not yet implemented");
    return false;
}

void ShellCore::stopExecution() {
    // Force stop any running program
    m_programRunning = false;
    m_continueFromLine = -1;
    
    // Note: With embedded compiler, no subprocesses to kill
    // Lua state is in-process
    
    // Clear execution state
    if (!m_tempFilename.empty()) {
        std::remove(m_tempFilename.c_str());
    }
}

// Command handlers

bool ShellCore::handleDirectLine(const ParsedCommand& cmd) {
    // Format BASIC keywords in the code before storing
    std::string formattedCode = m_parser.formatBasicKeywords(cmd.code);
    m_program.setLine(cmd.lineNumber, formattedCode);

    if (m_program.isAutoMode()) {
        m_program.incrementAutoLine();
    } else {
        // Check if we should suggest the next line for auto-continuation
        m_lastLineNumber = cmd.lineNumber;
        int nextLine = findNextAvailableLineNumber(cmd.lineNumber);
        if (nextLine > 0) {
            m_autoContinueMode = true;
            m_suggestedNextLine = nextLine;
        }
    }

    // Don't print Ready after line entry - stay out of user's way
    return true;
}

bool ShellCore::handleDeleteLine(const ParsedCommand& cmd) {
    m_program.deleteLine(cmd.lineNumber);
    return true;
}

bool ShellCore::handleList(const ParsedCommand& cmd) {
    std::cout << "\n";
    switch (cmd.type) {
        case ShellCommandType::LIST:
            listAll();
            break;

        case ShellCommandType::LIST_RANGE:
            listRange(cmd.startLine, cmd.endLine);
            break;

        case ShellCommandType::LIST_LINE:
            listLine(cmd.lineNumber);
            break;

        case ShellCommandType::LIST_FROM:
            listFrom(cmd.startLine);
            break;

        case ShellCommandType::LIST_TO:
            listTo(cmd.endLine);
            break;

        default:
            return false;
    }

    std::cout << "\nReady.\n";
    return true;
}

bool ShellCore::handleRun(const ParsedCommand& cmd) {
    int startLine = -1;
    if (cmd.type == ShellCommandType::RUN_FROM) {
        startLine = cmd.lineNumber;
    }

    return runProgram(startLine);
}

bool ShellCore::handleLoad(const ParsedCommand& cmd) {
    return loadProgram(cmd.filename);
}

bool ShellCore::handleSave(const ParsedCommand& cmd) {
    // If no filename provided, use the current program's filename
    if (!cmd.hasFilename || cmd.filename.empty()) {
        std::string currentFilename = m_program.getFilename();
        if (currentFilename.empty()) {
            showError("No filename specified and no file loaded");
            return false;
        }
        return saveProgram(currentFilename);
    }
    return saveProgram(cmd.filename);
}

bool ShellCore::handleMerge(const ParsedCommand& cmd) {
    return mergeProgram(cmd.filename);
}

bool ShellCore::handleNew(const ParsedCommand& cmd) {
    newProgram();
    return true;
}

bool ShellCore::handleAuto(const ParsedCommand& cmd) {
    if (cmd.type == ShellCommandType::AUTO_PARAMS) {
        m_program.setAutoMode(true, cmd.startLine, cmd.step);
    } else {
        m_program.setAutoMode(true);
    }

    showMessage("Automatic line numbering enabled");
    return true;
}

bool ShellCore::handleRenum(const ParsedCommand& cmd) {
    if (m_program.isEmpty()) {
        showError("No program to renumber");
        return false;
    }

    m_program.renumber(cmd.startLine, cmd.step);
    showMessage("Program renumbered");
    return true;
}

bool ShellCore::handleEdit(const ParsedCommand& cmd) {
    // Get the current content of the line (if it exists)
    std::string currentContent = m_program.getLine(cmd.lineNumber);

    // Call the interactive line editor with pre-filled content
    std::string editedContent = editLineInteractive(cmd.lineNumber, currentContent);

    // Check if edit was cancelled or navigation occurred
    if (editedContent == "\x1B") {  // ESC character indicates cancel or navigation
        // Line was already saved during navigation
        std::cout << "\nReady.\n";
        return true;
    }

    // If empty input, check if line still exists (navigation may have saved it)
    if (editedContent.empty()) {
        std::string existingContent = m_program.getLine(cmd.lineNumber);
        if (existingContent.empty() && !currentContent.empty()) {
            // Line was cleared during edit
            m_program.deleteLine(cmd.lineNumber);
        }
        // If line exists with content, it was already saved during navigation
    } else {
        // Format BASIC keywords and set the new line content
        std::string formattedContent = m_parser.formatBasicKeywords(editedContent);
        m_program.setLine(cmd.lineNumber, formattedContent);
    }

    std::cout << "\nReady.\n";
    return true;
}

bool ShellCore::handleFind(const ParsedCommand& cmd) {
    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }
    
    // Store search parameters for FINDNEXT and REPLACE
    m_lastSearchText = cmd.searchText;
    m_lastContextLines = cmd.contextLines;
    m_lastSearchLine = 0;  // Start from beginning
    m_hasActiveSearch = false;
    
    // Get all line numbers
    std::vector<int> lineNumbers = m_program.getLineNumbers();
    if (lineNumbers.empty()) {
        showError("No program lines to search");
        return false;
    }
    
    // Search for the text (case-insensitive)
    std::string searchLower = m_lastSearchText;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    
    for (int lineNum : lineNumbers) {
        std::string lineContent = m_program.getLine(lineNum);
        std::string contentLower = lineContent;
        std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);
        
        if (contentLower.find(searchLower) != std::string::npos) {
            // Found it! Display the line and context
            m_lastSearchLine = lineNum;
            m_hasActiveSearch = true;
            showSearchResult(lineNum, lineContent, m_lastContextLines);
            return true;
        }
    }
    
    // Not found
    showError("\"" + m_lastSearchText + "\" not found");
    return false;
}

bool ShellCore::handleFindNext(const ParsedCommand& cmd) {
    if (m_lastSearchText.empty()) {
        showError("No previous search. Use FIND first.");
        return false;
    }
    
    if (m_program.isEmpty()) {
        showError("No program in memory");
        return false;
    }
    
    // Get all line numbers
    std::vector<int> lineNumbers = m_program.getLineNumbers();
    if (lineNumbers.empty()) {
        showError("No program lines to search");
        return false;
    }
    
    // Find starting position (after last found line)
    std::string searchLower = m_lastSearchText;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    
    for (int lineNum : lineNumbers) {
        if (lineNum <= m_lastSearchLine) {
            continue;  // Skip lines we've already searched
        }
        
        std::string lineContent = m_program.getLine(lineNum);
        std::string contentLower = lineContent;
        std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);
        
        if (contentLower.find(searchLower) != std::string::npos) {
            // Found next occurrence!
            m_lastSearchLine = lineNum;
            m_hasActiveSearch = true;
            showSearchResult(lineNum, lineContent, m_lastContextLines);
            return true;
        }
    }
    
    // Not found - wrap around or end
    showError("\"" + m_lastSearchText + "\" not found (end of program)");
    return false;
}

bool ShellCore::handleReplace(const ParsedCommand& cmd) {
    if (!m_hasActiveSearch || m_lastSearchText.empty()) {
        showError("No active search. Use FIND first, then REPLACE.");
        return false;
    }
    
    if (m_lastSearchLine <= 0) {
        showError("No current search result to replace.");
        return false;
    }
    
    // Get the current line content
    std::string currentContent = m_program.getLine(m_lastSearchLine);
    if (currentContent.empty()) {
        showError("Search result line no longer exists.");
        m_hasActiveSearch = false;
        return false;
    }
    
    // Perform case-insensitive replacement of first occurrence
    std::string searchLower = m_lastSearchText;
    std::string contentLower = currentContent;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);
    
    size_t pos = contentLower.find(searchLower);
    if (pos == std::string::npos) {
        showError("Search text no longer found in line " + std::to_string(m_lastSearchLine));
        m_hasActiveSearch = false;
        return false;
    }
    
    // Replace the text (preserve original case context)
    std::string newContent = currentContent;
    newContent.replace(pos, m_lastSearchText.length(), cmd.replaceText);
    
    // Update the line
    m_program.setLine(m_lastSearchLine, newContent);
    
    // Show the result
    std::cout << "\nReplaced \"" << m_lastSearchText << "\" with \"" << cmd.replaceText 
              << "\" in line " << m_lastSearchLine << ":\n";
    std::cout << m_lastSearchLine << " " << newContent << "\n\n";
    
    // Clear active search since we've modified the content
    m_hasActiveSearch = false;
    
    return true;
}

bool ShellCore::handleReplaceNext(const ParsedCommand& cmd) {
    // First perform the replace on current found item
    if (!handleReplace(cmd)) {
        return false;
    }
    
    // Then find the next occurrence
    ParsedCommand findCmd;
    findCmd.type = ShellCommandType::FINDNEXT;
    if (handleFindNext(findCmd)) {
        std::cout << "Ready for next replace. Use REPLACE \"" << cmd.replaceText 
                  << "\" or REPLACENEXT \"" << cmd.replaceText << "\"\n\n";
    }
    
    return true;
}

bool ShellCore::handleVars(const ParsedCommand& cmd) {
    showVariables();
    return true;
}

bool ShellCore::handleClear(const ParsedCommand& cmd) {
    clearVariables();
    return true;
}

bool ShellCore::handleCheck(const ParsedCommand& cmd) {
    return checkSyntax();
}

bool ShellCore::handleFormat(const ParsedCommand& cmd) {
    return formatProgram();
}

bool ShellCore::handleCls(const ParsedCommand& cmd) {
    m_terminal->clearScreen();
    return true;
}

bool ShellCore::handleDir(const ParsedCommand& cmd) {
    std::string scriptsDir = getBasicScriptsDir();
    std::string libDir = getBasicLibDir();
    
    std::vector<std::pair<std::string, std::string>> basFiles; // filename, full path
    
    // Scan scripts directory
    DIR* dir = opendir(scriptsDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 && 
                filename.substr(filename.length() - 4) == ".bas") {
                basFiles.push_back({filename, scriptsDir + filename});
            }
        }
        closedir(dir);
    }
    
    // Scan lib directory
    dir = opendir(libDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 && 
                filename.substr(filename.length() - 4) == ".bas") {
                basFiles.push_back({"lib/" + filename, libDir + filename});
            }
        }
        closedir(dir);
    }

    // Sort files alphabetically by display name
    std::sort(basFiles.begin(), basFiles.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    if (basFiles.empty()) {
        showMessage("No .bas files found");
        std::cout << "Scripts directory: " << scriptsDir << "\n";
        std::cout << "Library directory: " << libDir << "\n";
        return true;
    }

    // Display files
    std::cout << "\nBASIC files:\n";
    for (const auto& file : basFiles) {
        // Get file size
        struct stat st;
        if (stat(file.second.c_str(), &st) == 0) {
            std::cout << "  " << file.first;
            // Pad to 40 characters
            if (file.first.length() < 40) {
                std::cout << std::string(40 - file.first.length(), ' ');
            }
            std::cout << " (" << st.st_size << " bytes)\n";
        } else {
            std::cout << "  " << file.first << "\n";
        }
    }
    std::cout << "\n" << basFiles.size() << " file(s)\n";
    std::cout << "Scripts: " << scriptsDir << "\n";
    std::cout << "Library: " << libDir << "\n";
    
    return true;
}

bool ShellCore::handleHelp(const ParsedCommand& cmd) {
    // Check if a topic or command was specified
    if (!cmd.searchText.empty()) {
        showHelpForTopicOrCommand(cmd.searchText);
    } else {
        showHelp();
    }
    return true;
}

bool ShellCore::handleQuit(const ParsedCommand& cmd) {
    quit();
    return true;
}

bool ShellCore::handleImmediate(const ParsedCommand& cmd) {
    // For Phase 1, just show a message
    showMessage("Immediate mode not yet implemented");
    return true;
}

// List command variants

void ShellCore::listAll() {
    if (m_program.isEmpty()) {
        showMessage("No program in memory");
        return;
    }

    // Get formatted program for display
    std::string programText = m_program.generateProgram();
    FormatterOptions options;
    options.start_line = -1; // Don't renumber for listing, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    FormatterResult result = formatBasicCode(programText, options);

    if (result.success && !result.formatted_code.empty()) {
        // Display the formatted program
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        auto lines = m_program.getAllLines();
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listRange(int start, int end) {
    ProgramManagerV2::ListRange range(start, end);
    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines in specified range");
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    FormatterResult result = formatBasicCode(programText, options);

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listFrom(int start) {
    ProgramManagerV2::ListRange range;
    range.startLine = start;
    range.hasStart = true;
    range.hasEnd = false;

    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines from line " + std::to_string(start));
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    FormatterResult result = formatBasicCode(programText, options);

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listTo(int end) {
    ProgramManagerV2::ListRange range;
    range.endLine = end;
    range.hasStart = false;
    range.hasEnd = true;

    auto lines = m_program.getLines(range);

    if (lines.empty()) {
        showMessage("No lines up to line " + std::to_string(end));
        return;
    }

    // Generate program text for range
    std::string programText;
    for (const auto& line : lines) {
        programText += std::to_string(line.first) + " " + line.second + "\n";
    }

    // Format the range
    FormatterOptions options;
    options.start_line = -1; // Don't renumber, just format
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = false;
    options.add_indentation = true;

    FormatterResult result = formatBasicCode(programText, options);

    if (result.success && !result.formatted_code.empty()) {
        std::cout << result.formatted_code;
    } else {
        // Fallback to simple listing
        for (const auto& line : lines) {
            printProgramLine(line.first, line.second);
        }
    }
}

void ShellCore::listLine(int line) {
    if (m_program.hasLine(line)) {
        std::string code = m_program.getLine(line);

        // Format single line
        std::string programText = std::to_string(line) + " " + code + "\n";

        FormatterOptions options;
        options.start_line = -1; // Don't renumber, just format
        options.step = 10;
        options.indent_spaces = 2;
        options.update_references = false;
        options.add_indentation = true;

        FormatterResult result = formatBasicCode(programText, options);

        if (result.success && !result.formatted_code.empty()) {
            std::cout << result.formatted_code;
        } else {
            printProgramLine(line, code);
        }
    } else {
        showError("Line " + std::to_string(line) + " not found");
    }
}

// Program execution

bool ShellCore::executeCompiledProgram(const std::string& program, int startLine) {
    try {
        // Start timing
        auto compileStartTime = std::chrono::high_resolution_clock::now();
        
        // Lexical analysis
        if (m_verbose) {
            std::cout << "Lexing...\n";
        }
        
        Lexer lexer;
        lexer.tokenize(program);
        auto tokens = lexer.getTokens();
        
        if (tokens.empty()) {
            showError("No tokens generated from program");
            return false;
        }
        
        // Parsing
        if (m_verbose) {
            std::cout << "Parsing...\n";
        }
        
        Parser parser;
        auto ast = parser.parse(tokens, "<shell>");
        
        if (!ast || parser.hasErrors()) {
            showError("Parsing failed");
            for (const auto& error : parser.getErrors()) {
                std::cerr << "  " << error.toString() << "\n";
            }
            return false;
        }
        
        // Get compiler options
        const auto& compilerOptions = parser.getOptions();
        
        // Semantic analysis
        if (m_verbose) {
            std::cout << "Semantic analysis...\n";
        }
        
        SemanticAnalyzer semantic;
        
        // Register voice constants if voice controller is enabled
        #ifdef VOICE_CONTROLLER_ENABLED
        FBRunner3::VoiceRegistration::registerVoiceConstants(semantic.getConstantsManager());
        #endif
        
        semantic.analyze(*ast, compilerOptions);
        
        // Build control flow graph (not strictly needed for execution but follows fbc pattern)
        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast, semantic.getSymbolTable());
        
        // Generate IR
        if (m_verbose) {
            std::cout << "Generating IR...\n";
        }
        
        IRGenerator irGen;
        auto irCode = irGen.generate(*cfg, semantic.getSymbolTable());
        
        if (!irCode) {
            showError("Failed to generate IR code");
            return false;
        }
        
        // Generate Lua code
        if (m_verbose) {
            std::cout << "Generating Lua code...\n";
        }
        
        LuaCodeGenConfig config;
        config.emitComments = false;
        LuaCodeGenerator luaGen(config);
        std::string luaCode = luaGen.generate(*irCode);
        
        // DEBUG: Save generated Lua code
        {
            std::ofstream debugLua("/tmp/generated.lua");
            debugLua << luaCode;
            debugLua.close();
            std::cout << "DEBUG: Generated Lua saved to /tmp/generated.lua\n";
        }
        
        // Create Lua state
        lua_State* L = luaL_newstate();
        if (!L) {
            showError("Cannot create Lua state");
            return false;
        }
        
        // Open standard libraries
        luaL_openlibs(L);
        
        // Register runtime modules
        register_unicode_module(L);
        register_bitwise_module(L);
        register_constants_module(L);
        set_constants_manager(&semantic.getConstantsManager());
        
        FasterBASIC::register_fileio_functions(L);
        FasterBASIC::registerDataBindings(L);
        FasterBASIC::registerTerminalBindings(L);
        
        // Register voice bindings if available (terminal-only, no GUI)
        #ifdef VOICE_CONTROLLER_ENABLED
        fprintf(stderr, "DEBUG: Registering voice Lua bindings\n");
        fflush(stderr);
        FBRunner3::VoiceRegistration::registerVoiceLuaBindings(L);
        fprintf(stderr, "DEBUG: Voice Lua bindings registered\n");
        fflush(stderr);
        #else
        fprintf(stderr, "DEBUG: VOICE_CONTROLLER_ENABLED not defined - voice bindings NOT registered\n");
        fflush(stderr);
        #endif
        
        // Register additional Lua bindings if set (e.g., fbsh_voices-specific functions)
        if (g_additionalLuaBindings) {
            g_additionalLuaBindings(L);
        }
        
        // Initialize DATA segment
        if (!irCode->dataValues.empty()) {
            FasterBASIC::initializeDataManager(irCode->dataValues);
            
            for (const auto& entry : irCode->dataLineRestorePoints) {
                FasterBASIC::addDataRestorePoint(entry.first, entry.second);
            }
            
            for (const auto& entry : irCode->dataLabelRestorePoints) {
                FasterBASIC::addDataRestorePointByLabel(entry.first, entry.second);
            }
        }
        
        // Execute the program
        auto startTime = std::chrono::high_resolution_clock::now();
        
        fprintf(stderr, "DEBUG: About to load Lua code\n");
        fflush(stderr);
        
        if (luaL_loadstring(L, luaCode.c_str()) != 0) {
            showError(std::string("Error loading Lua code: ") + lua_tostring(L, -1));
            lua_close(L);
            return false;
        }
        
        fprintf(stderr, "DEBUG: Lua code loaded, about to call lua_pcall\n");
        fflush(stderr);
        
        int result = lua_pcall(L, 0, 0, 0);
        
        fprintf(stderr, "DEBUG: lua_pcall returned with result=%d\n", result);
        fflush(stderr);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // Check for errors BEFORE closing Lua state
        if (result != 0) {
            std::string errorMsg = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Unknown error";
            lua_close(L);
            showError(std::string("Execution error: ") + errorMsg);
            return false;
        }
        
        // Clean up Lua state
        lua_close(L);

        // Show timing
        // Format time in human-friendly way
        long long totalUs = duration.count();
        long long totalMs = totalUs / 1000;
        
        std::cout << "\nTime taken: ";
        
        if (totalMs < 10) {
            // Less than 10ms (1 centisecond) - show milliseconds
            std::cout << totalMs << "ms\n";
        } else if (totalMs < 1000) {
            // Less than 1 second - show centiseconds
            long long centiseconds = totalMs / 10;
            std::cout << centiseconds << "cs\n";
        } else {
            // 1 second or more - show seconds with hundredths
            long long minutes = totalMs / 60000;
            long long seconds = (totalMs % 60000) / 1000;
            long long hundredths = (totalMs % 1000) / 10;
            
            if (minutes > 0) {
                std::cout << minutes << "m ";
            }
            std::cout << seconds << "." << std::setfill('0') << std::setw(2) << hundredths << "s\n";
        }
        
        std::cout << "Ready.\n";
        return true;
        
    } catch (const std::exception& e) {
        showError(std::string("Execution error: ") + e.what());
        return false;
    }
}

// File operations

bool ShellCore::loadProgram(const std::string& filename) {
    std::string fullFilename = addExtensionIfNeeded(filename);
    fullFilename = resolveFilePath(fullFilename);

    if (!fileExists(fullFilename)) {
        showError("File not found: " + fullFilename);
        return false;
    }

    std::string content = readFileContent(fullFilename);
    if (content.empty()) {
        showError("Failed to read file or file is empty: " + fullFilename);
        return false;
    }

    // Check if current program is modified and warn user
    if (!m_program.isEmpty() && m_program.isModified()) {
        std::cout << "Warning: Current program has unsaved changes.\n";
        std::cout << "Continue loading? (Y/N): ";
        std::string response = readInput();
        if (response.empty() || (response[0] != 'Y' && response[0] != 'y')) {
            showMessage("Load cancelled");
            return false;
        }
    }

    // Parse the file content and load into program manager
    m_program.clear();

    std::istringstream iss(content);
    std::string line;
    int lineCount = 0;
    int errorCount = 0;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            if (!code.empty()) {
                m_program.setLine(lineNum, code);
                lineCount++;
            }
        } else {
            errorCount++;
            if (m_verbose) {
                showError("Skipped invalid line: " + line);
            }
        }
    }

    m_program.setFilename(fullFilename);
    m_program.setModified(false);

    std::string message = "Loaded " + std::to_string(lineCount) + " lines from " + fullFilename;
    if (errorCount > 0) {
        message += " (" + std::to_string(errorCount) + " lines skipped)";
    }
    showSuccess(message);
    return true;
}

bool ShellCore::saveProgram(const std::string& filename) {
    if (m_program.isEmpty()) {
        showError("No program to save");
        return false;
    }

    std::string fullFilename = addExtensionIfNeeded(filename);
    fullFilename = resolveFilePath(fullFilename);

    // Check if file exists and warn user
    if (fileExists(fullFilename)) {
        std::cout << "File '" << fullFilename << "' already exists.\n";
        std::cout << "Overwrite? (Y/N): ";
        std::string response = readInput();
        if (response.empty() || (response[0] != 'Y' && response[0] != 'y')) {
            showMessage("Save cancelled");
            return false;
        }
    }

    // Get formatted program content
    std::string content = m_program.generateProgram();

    // Optionally format before saving
    if (m_verbose) {
        FormatterOptions options;
        options.start_line = -1; // Don't renumber, just format
        options.step = 10;
        options.indent_spaces = 2;
        options.update_references = false;
        options.add_indentation = true;

        FormatterResult result = formatBasicCode(content, options);
        if (result.success && !result.formatted_code.empty()) {
            content = result.formatted_code;
        }
    }

    if (writeFileContent(fullFilename, content)) {
        m_program.setFilename(fullFilename);
        m_program.setModified(false);
        auto stats = m_program.getStatistics();
        showSuccess("Program saved to " + fullFilename + " (" + std::to_string(stats.lineCount) + " lines, " + std::to_string(stats.totalCharacters) + " chars)");
        return true;
    } else {
        showError("Failed to save program to " + fullFilename);
        return false;
    }
}

bool ShellCore::mergeProgram(const std::string& filename) {
    std::string fullFilename = addExtensionIfNeeded(filename);

    if (!fileExists(fullFilename)) {
        showError("File not found: " + fullFilename);
        return false;
    }

    std::string content = readFileContent(fullFilename);
    if (content.empty()) {
        showError("Failed to read file or file is empty: " + fullFilename);
        return false;
    }

    // Parse the file content and merge into current program
    std::istringstream iss(content);
    std::string line;
    int lineCount = 0;
    int replacedCount = 0;
    int errorCount = 0;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            if (!code.empty()) {
                if (m_program.hasLine(lineNum)) {
                    replacedCount++;
                }
                m_program.setLine(lineNum, code);
                lineCount++;
            }
        } else {
            errorCount++;
            if (m_verbose) {
                showError("Skipped invalid line: " + line);
            }
        }
    }

    std::string message = "Merged " + std::to_string(lineCount) + " lines from " + fullFilename;
    if (replacedCount > 0) {
        message += " (" + std::to_string(replacedCount) + " lines replaced)";
    }
    if (errorCount > 0) {
        message += " (" + std::to_string(errorCount) + " lines skipped)";
    }
    showSuccess(message);
    return true;
}

void ShellCore::newProgram() {
    m_program.clear();
    showMessage("Program cleared");
}

// Development tools

bool ShellCore::checkSyntax() {
    showMessage("Syntax check not yet implemented");
    return false;
}

bool ShellCore::formatProgram() {
    if (m_program.isEmpty()) {
        showError("No program to format");
        return false;
    }

    // Get current program
    std::string programText = m_program.generateProgram();

    // Use the basic formatter to format and renumber the program
    FormatterOptions options;
    options.start_line = 10;
    options.step = 10;
    options.indent_spaces = 2;
    options.update_references = true;
    options.add_indentation = true;

    FormatterResult result = formatBasicCode(programText, options);

    if (!result.success || result.formatted_code.empty()) {
        showError("Failed to format program: " + result.error_message);
        return false;
    }

    // Clear current program and reload the formatted version
    m_program.clear();

    // Parse the formatted program back into the program manager
    std::istringstream iss(result.formatted_code);
    std::string line;
    int lineCount = 0;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        // Parse line number and code
        std::istringstream lineStream(line);
        int lineNum;
        if (lineStream >> lineNum) {
            std::string code;
            std::getline(lineStream, code);
            // Remove leading space
            if (!code.empty() && code[0] == ' ') {
                code = code.substr(1);
            }
            m_program.setLine(lineNum, code);
            lineCount++;
        }
    }

    showSuccess("Program formatted and renumbered (" + std::to_string(lineCount) + " lines)");
    return true;
}

void ShellCore::showVariables() {
    showMessage("Variable display not yet implemented");
}

void ShellCore::clearVariables() {
    showMessage("Variable clearing not yet implemented");
}

// Information and help

void ShellCore::showHelp() {
    std::cout << "\nFasterBASIC Shell Commands:\n";
    std::cout << "===========================\n";
    std::cout << "\nProgram Entry:\n";
    std::cout << "  10 PRINT \"Hello\"   Add or replace line 10\n";
    std::cout << "  10               Delete line 10\n";
    std::cout << "\nListing:\n";
    std::cout << "  LIST             List entire program\n";
    std::cout << "  LIST 10          List line 10\n";
    std::cout << "  LIST 10-50       List lines 10 through 50\n";
    std::cout << "  LIST 10-         List from line 10 to end\n";
    std::cout << "  LIST -50         List from start to line 50\n";
    std::cout << "\nExecution:\n";
    std::cout << "  RUN              Run program from beginning\n";
    std::cout << "  RUN 100          Run program starting from line 100\n";
    std::cout << "\nFile Operations:\n";
    std::cout << "  NEW              Clear program from memory\n";
    std::cout << "  LOAD \"file\"       Load program from file\n";
    std::cout << "  SAVE \"file\"       Save program to file\n";
    std::cout << "  DIR              List .bas files in current directory\n";
    std::cout << "\nProgram Management:\n";
    std::cout << "  AUTO             Enable auto line numbering\n";
    std::cout << "  AUTO 1000,10     Auto numbering starting at 1000, step 10\n";
    std::cout << "  RENUM            Renumber program (start=10, step=10)\n";
    std::cout << "  RENUM 100,5      Renumber starting at 100, step 5\n";
    std::cout << "  EDIT 100         Edit line 100 with full line editor\n";
    std::cout << "  FIND \"text\"       Find first occurrence of text\n";
    std::cout << "  FIND text,5      Find text with 5 context lines\n";
    std::cout << "  FINDNEXT         Find next occurrence of last search\n";
    std::cout << "  REPLACE \"new\"     Replace found text with new text\n";
    std::cout << "  REPLACENEXT \"new\" Replace and find next occurrence\n";
    std::cout << "\nOther:\n";
    std::cout << "  CLS              Clear screen\n";
    std::cout << "  FORMAT           Format and renumber program\n";
    std::cout << "  HELP             Show this help\n";
    std::cout << "  HELP <category>  Show commands in a category\n";
    std::cout << "  HELP <command>   Show detailed help for a command\n";
    std::cout << "  QUIT             Exit shell\n";
    std::cout << "\n";
    
    // Show available BASIC command categories
    showHelpCategories();
}

void ShellCore::showHelpCategories() {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();
    auto categories = registry.getCategories();
    
    if (categories.empty()) {
        return;
    }
    
    std::cout << "BASIC Command Categories:\n";
    std::cout << "========================\n";
    std::cout << "Type HELP <category> to see commands in that category:\n\n";
    
    // Sort categories alphabetically
    std::sort(categories.begin(), categories.end());
    
    // Get category descriptions
    std::unordered_map<std::string, std::string> categoryDesc = {
        {"audio", "Music and sound playback"},
        {"cart", "Cart/cartridge system"},
        {"circle", "Circle ID system"},
        {"control", "Control flow commands"},
        {"data", "Data storage commands"},
        {"file", "File I/O operations"},
        {"graphics", "Graphics primitives"},
        {"input", "Keyboard and mouse input"},
        {"line", "Line ID system"},
        {"math", "Mathematical functions"},
        {"particle", "Particle effects"},
        {"rectangle", "Rectangle ID system"},
        {"sprite", "Sprite management"},
        {"string", "String manipulation"},
        {"system", "System utilities"},
        {"text", "Text display and manipulation"},
        {"tilemap", "Tilemap operations"},
        {"voice", "Voice synthesis"}
    };
    
    for (const auto& cat : categories) {
        std::string catUpper = cat;
        std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
        
        std::string desc = "Commands";
        auto it = categoryDesc.find(cat);
        if (it != categoryDesc.end()) {
            desc = it->second;
        }
        
        std::cout << "  " << std::setw(12) << std::left << catUpper << " - " << desc << "\n";
    }
    
    std::cout << "\n";
}

void ShellCore::showHelpForTopicOrCommand(const std::string& topic) {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();
    
    // Convert to uppercase for comparison
    std::string topicUpper = topic;
    std::transform(topicUpper.begin(), topicUpper.end(), topicUpper.begin(), ::toupper);
    
    // Check if it's a category
    std::string topicLower = topic;
    std::transform(topicLower.begin(), topicLower.end(), topicLower.begin(), ::tolower);
    
    auto categories = registry.getCategories();
    bool isCategory = std::find(categories.begin(), categories.end(), topicLower) != categories.end();
    
    if (isCategory) {
        showHelpForCategory(topicLower);
        return;
    }
    
    // Check if it's a command or function
    const auto* cmd = registry.getCommandOrFunction(topicUpper);
    if (cmd) {
        showHelpForCommand(cmd);
        return;
    }
    
    // Not found - show error and suggestions
    std::cout << "\nUnknown command or category: " << topic << "\n\n";
    
    // Try to find partial matches
    std::vector<std::string> matches;
    auto allCommands = registry.getAllNames();
    for (const auto& cmdName : allCommands) {
        if (cmdName.find(topicUpper) != std::string::npos) {
            matches.push_back(cmdName);
        }
    }
    
    if (!matches.empty()) {
        std::cout << "Did you mean one of these commands?\n";
        for (const auto& match : matches) {
            const auto* matchCmd = registry.getCommandOrFunction(match);
            if (matchCmd) {
                std::cout << "  " << std::setw(25) << std::left << match 
                         << matchCmd->description << "\n";
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "Type HELP to see all categories\n";
    std::cout << "Type HELP <category> to see commands in a category\n";
    std::cout << "\n";
}

void ShellCore::showHelpForCategory(const std::string& category) {
    using namespace FasterBASIC::ModularCommands;
    auto& registry = getGlobalCommandRegistry();
    
    auto commands = registry.getCommandsByCategory(category);
    auto functions = registry.getFunctionsByCategory(category);
    
    if (commands.empty() && functions.empty()) {
        std::cout << "\nNo commands found in category: " << category << "\n\n";
        return;
    }
    
    std::string catUpper = category;
    std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
    
    std::cout << "\n" << catUpper << " Commands\n";
    std::cout << std::string(catUpper.length() + 9, '=') << "\n\n";
    
    // Display commands
    if (!commands.empty()) {
        std::cout << "Commands:\n";
        for (const auto& cmdName : commands) {
            const auto* cmd = registry.getCommand(cmdName);
            if (cmd) {
                std::string signature = formatCommandSignature(cmd);
                std::cout << "  " << std::setw(40) << std::left << signature 
                         << cmd->description << "\n";
            }
        }
        std::cout << "\n";
    }
    
    // Display functions
    if (!functions.empty()) {
        std::cout << "Functions:\n";
        for (const auto& funcName : functions) {
            const auto* func = registry.getFunction(funcName);
            if (func) {
                std::string signature = formatFunctionSignature(func);
                std::cout << "  " << std::setw(40) << std::left << signature 
                         << func->description << "\n";
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "Type HELP <command> for detailed help on a specific command\n\n";
}

void ShellCore::showHelpForCommand(const FasterBASIC::ModularCommands::CommandDefinition* cmd) {
    using namespace FasterBASIC::ModularCommands;
    
    // Header
    std::cout << "\n" << cmd->commandName << " - " << cmd->description << "\n";
    std::cout << std::string(cmd->commandName.length() + cmd->description.length() + 3, '=') << "\n\n";
    
    // Category
    std::string catUpper = cmd->category;
    std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
    std::cout << "Category: " << catUpper << "\n\n";
    
    // Syntax
    std::cout << "Syntax:\n";
    if (cmd->isFunction) {
        std::cout << "  result = " << formatFunctionSignature(cmd) << "\n\n";
    } else {
        std::cout << "  " << formatCommandSignature(cmd) << "\n\n";
    }
    
    // Parameters
    if (!cmd->parameters.empty()) {
        std::cout << "Parameters:\n";
        for (const auto& param : cmd->parameters) {
            std::cout << "  " << param.name << " (" 
                     << parameterTypeToString(param.type);
            if (param.isOptional) {
                std::cout << ", optional";
                if (!param.defaultValue.empty()) {
                    std::cout << ", default: " << param.defaultValue;
                }
            } else {
                std::cout << ", required";
            }
            std::cout << ")\n";
            
            if (!param.description.empty()) {
                std::cout << "    " << param.description << "\n";
            }
            std::cout << "\n";
        }
    }
    
    // Return type for functions
    if (cmd->isFunction && cmd->returnType != ReturnType::VOID) {
        std::cout << "Returns:\n";
        std::cout << "  " << returnTypeToString(cmd->returnType) << "\n\n";
    }
    
    // See also
    std::cout << "See Also:\n";
    std::cout << "  HELP " << catUpper << " for all " << cmd->category << " commands\n\n";
}

std::string ShellCore::formatCommandSignature(const FasterBASIC::ModularCommands::CommandDefinition* cmd) {
    std::ostringstream oss;
    oss << cmd->commandName;
    
    for (size_t i = 0; i < cmd->parameters.size(); ++i) {
        const auto& param = cmd->parameters[i];
        if (i == 0) oss << " ";
        else oss << ", ";
        
        if (param.isOptional) oss << "[";
        oss << param.name;
        if (param.isOptional) oss << "]";
    }
    
    return oss.str();
}

std::string ShellCore::formatFunctionSignature(const FasterBASIC::ModularCommands::CommandDefinition* func) {
    std::ostringstream oss;
    oss << func->commandName << "(";
    
    for (size_t i = 0; i < func->parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        const auto& param = func->parameters[i];
        if (param.isOptional) oss << "[";
        oss << param.name;
        if (param.isOptional) oss << "]";
    }
    
    oss << ")";
    return oss.str();
}

void ShellCore::showVersion() {
    std::cout << "FasterBASIC Shell v" << SHELL_VERSION << std::endl;
}

void ShellCore::showStatistics() {
    auto stats = m_program.getStatistics();
    std::cout << "\nProgram Statistics:\n";
    std::cout << "==================\n";
    std::cout << "Lines: " << stats.lineCount << std::endl;
    std::cout << "Characters: " << stats.totalCharacters << std::endl;
    if (stats.lineCount > 0) {
        std::cout << "Range: " << stats.minLineNumber << "-" << stats.maxLineNumber << std::endl;
        std::cout << "Gaps in numbering: " << (stats.hasGaps ? "Yes" : "No") << std::endl;
    }
    std::cout << "Modified: " << (m_program.isModified() ? "Yes" : "No") << std::endl;
    if (m_program.hasFilename()) {
        std::cout << "File: " << m_program.getFilename() << std::endl;
    }
    std::cout << std::endl;
}

std::string ShellCore::editLineInteractive(int lineNumber, const std::string& initialContent) {
    std::string buffer = initialContent;
    size_t cursorPos = buffer.length();  // Start cursor at end
    bool done = false;

    // Save current terminal settings
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;

    // Enable raw mode for character-by-character input
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Display initial content
    std::cout << lineNumber << " " << buffer;
    std::cout.flush();

    while (!done) {
        char ch = std::cin.get();

        if (ch == '\n' || ch == '\r') {
            // Enter - accept changes
            done = true;
            std::cout << std::endl;
            
            // Save the current line before returning
            if (!buffer.empty()) {
                m_program.setLine(lineNumber, buffer);
            }
        } else if (ch == '\x1B') {  // ESC key
            // Check for arrow key sequences
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq2 = std::cin.get();
                switch (seq2) {
                    case 'A':  // Up arrow - move to previous line if it exists
                        {
                            // First, save current line content
                            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                            std::cout << std::endl;
                            
                            if (!buffer.empty()) {
                                m_program.setLine(lineNumber, buffer);
                            }
                            
                            // Find previous line
                            int prevLine = m_program.getPreviousLineNumber(lineNumber);
                            if (prevLine > 0) {
                                // Previous line exists, edit it
                                std::string prevContent = m_program.getLine(prevLine);
                                
                                // Recursively edit the previous line
                                editLineInteractive(prevLine, prevContent);
                                return "\x1B";  // Return ESC to signal already handled
                            }
                            // If no previous line, just continue editing current line
                            tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'B':  // Down arrow - move to next line or create it
                        {
                            // First, save current line content
                            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                            std::cout << std::endl;
                            
                            if (!buffer.empty()) {
                                m_program.setLine(lineNumber, buffer);
                            }
                            
                            // Find or create next line
                            int nextLine = m_program.getNextLineNumber(lineNumber);
                            if (nextLine == -1) {
                                // No next line exists, create one
                                nextLine = findNextAvailableLineNumber(lineNumber);
                            }
                            
                            // Get content of next line (empty if newly created)
                            std::string nextContent = m_program.getLine(nextLine);
                            
                            // Recursively edit the next line
                            editLineInteractive(nextLine, nextContent);
                            return "\x1B";  // Return ESC to signal already handled
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '1':  // Ctrl+Arrow sequences start with 1;5
                        if (std::cin.peek() == ';') {
                            std::cin.get();  // consume ';'
                            if (std::cin.peek() == '5') {
                                std::cin.get();  // consume '5'
                                char direction = std::cin.get();
                                switch (direction) {
                                    case 'C':  // Ctrl+Right - move word forward
                                        cursorPos = findNextWord(buffer, cursorPos);
                                        redrawLine(lineNumber, buffer, cursorPos);
                                        break;
                                    case 'D':  // Ctrl+Left - move word backward
                                        cursorPos = findPrevWord(buffer, cursorPos);
                                        redrawLine(lineNumber, buffer, cursorPos);
                                        break;
                                }
                            }
                        } else if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = 0;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case 'H':  // Home key
                        cursorPos = 0;
                        redrawLine(lineNumber, buffer, cursorPos);
                        break;
                    case 'F':  // End key
                        cursorPos = buffer.length();
                        redrawLine(lineNumber, buffer, cursorPos);
                        break;

                    case '4':  // End key (alternative sequence)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = buffer.length();
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '7':  // Home key (alternative sequence)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            cursorPos = 0;
                            redrawLine(lineNumber, buffer, cursorPos);
                        }
                        break;
                    case '3':  // Delete key (may have ~ after)
                        if (std::cin.peek() == '~') {
                            std::cin.get();  // consume ~
                            if (cursorPos < buffer.length()) {
                                buffer.erase(cursorPos, 1);
                                redrawLine(lineNumber, buffer, cursorPos);
                            }
                        }
                        break;

                }
            } else {
                // Single ESC - cancel edit
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                return "\x1B";  // Return ESC to indicate cancel
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel edit
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
            return "\x1B";  // Return ESC to indicate cancel
        } else if (ch == '\x01') {  // Ctrl+A - move to beginning of line
            cursorPos = 0;
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x05') {  // Ctrl+E - move to end of line
            cursorPos = buffer.length();
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x0B') {  // Ctrl+K - delete from cursor to end
            if (cursorPos < buffer.length()) {
                clearToEndOfLine(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x15') {  // Ctrl+U - delete from beginning to cursor
            if (cursorPos > 0) {
                clearToStartOfLine(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x17') {  // Ctrl+W - delete word backward
            if (cursorPos > 0) {
                deleteWordBackward(buffer, cursorPos);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x04') {  // Ctrl+D - delete character at cursor (like Del key)
            if (cursorPos < buffer.length()) {
                buffer.erase(cursorPos, 1);
                redrawLine(lineNumber, buffer, cursorPos);
            }
        } else if (ch == '\x0C') {  // Ctrl+L - refresh display
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch == '\x09') {  // Tab - insert spaces or complete keyword
            // Insert 4 spaces for tab (BASIC convention)
            for (int i = 0; i < 4; i++) {
                buffer.insert(cursorPos, 1, ' ');
                cursorPos++;
            }
            redrawLine(lineNumber, buffer, cursorPos);
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            redrawLine(lineNumber, buffer, cursorPos);
        }
        // Ignore other control characters
    }

    // Restore normal terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    return buffer;
}

size_t ShellCore::findWordStart(const std::string& buffer, size_t pos) {
    if (pos > buffer.length()) pos = buffer.length();
    while (pos > 0 && (buffer[pos-1] == ' ' || buffer[pos-1] == '\t')) {
        pos--;
    }
    while (pos > 0 && buffer[pos-1] != ' ' && buffer[pos-1] != '\t') {
        pos--;
    }
    return pos;
}

size_t ShellCore::findWordEnd(const std::string& buffer, size_t pos) {
    while (pos < buffer.length() && buffer[pos] != ' ' && buffer[pos] != '\t') {
        pos++;
    }
    return pos;
}

size_t ShellCore::findNextWord(const std::string& buffer, size_t pos) {
    // Skip current word
    while (pos < buffer.length() && buffer[pos] != ' ' && buffer[pos] != '\t') {
        pos++;
    }
    // Skip whitespace
    while (pos < buffer.length() && (buffer[pos] == ' ' || buffer[pos] == '\t')) {
        pos++;
    }
    return pos;
}

size_t ShellCore::findPrevWord(const std::string& buffer, size_t pos) {
    // Skip whitespace backward
    while (pos > 0 && (buffer[pos-1] == ' ' || buffer[pos-1] == '\t')) {
        pos--;
    }
    // Skip word backward
    while (pos > 0 && buffer[pos-1] != ' ' && buffer[pos-1] != '\t') {
        pos--;
    }
    return pos;
}

void ShellCore::clearToEndOfLine(std::string& buffer, size_t pos) {
    buffer.erase(pos);
}

void ShellCore::clearToStartOfLine(std::string& buffer, size_t& pos) {
    buffer.erase(0, pos);
    pos = 0;
}

void ShellCore::deleteWordBackward(std::string& buffer, size_t& pos) {
    size_t wordStart = findPrevWord(buffer, pos);
    buffer.erase(wordStart, pos - wordStart);
    pos = wordStart;
}

void ShellCore::redrawLine(int lineNumber, const std::string& buffer, size_t cursorPos) {
    // Clear current line and redraw
    std::cout << "\r\x1B[K";  // Move to start and clear line
    std::cout << lineNumber << " " << buffer;
    
    // Position cursor correctly
    if (cursorPos < buffer.length()) {
        size_t moveBack = buffer.length() - cursorPos;
        std::cout << "\x1B[" << moveBack << "D";  // Move cursor left
    }
    
    std::cout.flush();
}

void ShellCore::showSearchResult(int foundLine, const std::string& foundContent, int contextLines) {
    std::vector<int> lineNumbers = m_program.getLineNumbers();
    
    // Find the position of foundLine in the sorted list
    auto it = std::find(lineNumbers.begin(), lineNumbers.end(), foundLine);
    if (it == lineNumbers.end()) {
        return;  // Line not found (shouldn't happen)
    }
    
    int foundIndex = std::distance(lineNumbers.begin(), it);
    
    // Calculate range to display
    int startIndex = std::max(0, foundIndex - contextLines);
    int endIndex = std::min(static_cast<int>(lineNumbers.size()) - 1, foundIndex + contextLines);
    
    std::cout << "\nFound \"" << m_lastSearchText << "\" at line " << foundLine << ":\n\n";
    
    // Display context lines
    for (int i = startIndex; i <= endIndex; i++) {
        int lineNum = lineNumbers[i];
        std::string content = m_program.getLine(lineNum);
        
        if (lineNum == foundLine) {
            // Highlight the found line
            std::cout << ">>> " << lineNum << " " << content << "\n";
        } else {
            std::cout << "    " << lineNum << " " << content << "\n";
        }
    }
    
    std::cout << "\n";
}

int ShellCore::findNextAvailableLineNumber(int currentLine) {
    // Find the next line number that would make sense for continuation
    // Look for a gap of at least 10 or suggest currentLine + 10
    int suggested = currentLine + 10;

    // Check if there's a line close to our suggestion
    for (int step = 10; step <= 100; step += 10) {
        int candidate = currentLine + step;
        if (!m_program.hasLine(candidate)) {
            return candidate;
        }
    }

    // If all nearby multiples of 10 are taken, just suggest +10 anyway
    // The user can always override
    return suggested;
}

int ShellCore::findPreviousLineNumber(int currentLine) {
    // Find the previous line number that exists in the program
    std::vector<int> lineNumbers = m_program.getLineNumbers();

    // Find the largest line number that's less than currentLine
    int previous = -1;
    for (int lineNum : lineNumbers) {
        if (lineNum < currentLine && lineNum > previous) {
            previous = lineNum;
        }
    }

    return (previous > 0) ? previous : -1;
}

std::string ShellCore::readInputWithInlineEditing() {
    std::string buffer = "";
    size_t cursorPos = 0;
    bool done = false;

    // Enable raw mode for character input
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    // Show initial prompt
    std::cout << m_suggestedNextLine << " ";
    std::cout.flush();

    while (!done) {
        char ch = std::cin.get();

        if (ch == '\n' || ch == '\r') {
            // Enter - finish input
            done = true;
            std::cout << std::endl;
        } else if (ch == '\x1B') {  // ESC or arrow keys
            if (std::cin.peek() == '[') {
                std::cin.get(); // consume '['
                char seq = std::cin.get();
                switch (seq) {
                    case 'A':  // Up arrow - edit previous line
                        {
                            // Save current line if not empty
                            if (!buffer.empty()) {
                                std::string formattedBuffer = m_parser.formatBasicKeywords(buffer);
                                m_program.setLine(m_suggestedNextLine, formattedBuffer);
                            }

                            // Find previous line relative to the last entered line
                            int prevLine = findPreviousLineNumber(m_lastLineNumber);
                            if (prevLine > 0) {
                                // Get previous line content
                                std::string prevContent = m_program.getLine(prevLine);

                                // Switch to editing the previous line
                                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                                std::cout << "\r\x1B[K"; // Clear current line
                                std::cout << "Editing line " << prevLine << ":\n";

                                // Edit the previous line and save result
                                std::string editedContent = editLineInteractive(prevLine, prevContent);
                                if (editedContent != "\x1B") { // Not cancelled
                                    m_program.setLine(prevLine, editedContent);
                                }

                                // Return to continuation mode with next line after the edited one
                                m_suggestedNextLine = findNextAvailableLineNumber(prevLine);
                                m_autoContinueMode = true;
                                return "";
                            }
                        }
                        break;
                    case 'C':  // Right arrow
                        if (cursorPos < buffer.length()) {
                            cursorPos++;
                            std::cout << "\x1B[C";
                            std::cout.flush();
                        }
                        break;
                    case 'D':  // Left arrow
                        if (cursorPos > 0) {
                            cursorPos--;
                            std::cout << "\x1B[D";
                            std::cout.flush();
                        }
                        break;
                    default:
                        // Ignore other arrow keys
                        break;
                }
            } else {
                // Single ESC - cancel
                tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
                std::cout << "\r\x1B[K"; // Clear line
                return "";
            }
        } else if (ch == '\x7F' || ch == '\b') {  // Backspace
            if (cursorPos > 0) {
                buffer.erase(cursorPos - 1, 1);
                cursorPos--;
                // Redraw line
                std::cout << "\r" << m_suggestedNextLine << " " << buffer;
                // Position cursor
                for (size_t i = 0; i < buffer.length() - cursorPos; i++) {
                    std::cout << "\x1B[D";
                }
                std::cout << " \x1B[D"; // Clear extra character
                std::cout.flush();
            }
        } else if (ch == '\x03') {  // Ctrl+C
            // Cancel
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
            std::cout << "\r\x1B[K"; // Clear line
            return "";
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            buffer.insert(cursorPos, 1, ch);
            cursorPos++;
            // Redraw line
            std::cout << "\r" << m_suggestedNextLine << " " << buffer;
            // Position cursor
            for (size_t i = 0; i < buffer.length() - cursorPos; i++) {
                std::cout << "\x1B[D";
            }
            std::cout.flush();
        }
    }

    // Restore terminal mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

    return buffer;
}




// Configuration

void ShellCore::setVerbose(bool verbose) {
    m_verbose = verbose;
}

void ShellCore::setDebug(bool debug) {
    m_debug = debug;
}

bool ShellCore::isVerbose() const {
    return m_verbose;
}

bool ShellCore::isDebug() const {
    return m_debug;
}

// Utility functions

std::string ShellCore::generateTempFilename() {
    static int counter = 0;
    return TEMP_FILE_PREFIX + std::to_string(counter++) + ".bas";
}

bool ShellCore::fileExists(const std::string& filename) const {
    std::ifstream file(filename);
    return file.good();
}

std::string ShellCore::readFileContent(const std::string& filename) const {
    std::ifstream file(filename);
    if (!file) return "";

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool ShellCore::writeFileContent(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file) return false;

    file << content;
    return file.good();
}

void ShellCore::showError(const std::string& error) {
    std::cout << "Error: " << error << std::endl;
}

void ShellCore::showMessage(const std::string& message) {
    std::cout << message << std::endl;
}

void ShellCore::showSuccess(const std::string& message) {
    std::cout << message << std::endl;
}

std::string ShellCore::getDefaultExtension(const std::string& filename) const {
    return ".bas";
}

std::string ShellCore::addExtensionIfNeeded(const std::string& filename) const {
    if (filename.find('.') == std::string::npos) {
        return filename + getDefaultExtension(filename);
    }
    return filename;
}

void ShellCore::printProgramLine(int lineNumber, const std::string& code) {
    std::cout << lineNumber << " " << code << std::endl;
}

void ShellCore::printHeader(const std::string& title) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(title.length(), '=') << "\n";
}

void ShellCore::printSeparator() {
    std::cout << std::string(40, '-') << "\n";
}

// BASIC directory helpers
std::string ShellCore::getBasicScriptsDir() const {
    const char* home = getenv("HOME");
    if (!home) {
        return "./";
    }
    return std::string(home) + "/SuperTerminal/BASIC/";
}

std::string ShellCore::getBasicLibDir() const {
    return getBasicScriptsDir() + "lib/";
}

void ShellCore::ensureBasicDirectories() const {
    std::string scriptsDir = getBasicScriptsDir();
    std::string libDir = getBasicLibDir();
    
    // Create directories if they don't exist
    mkdir(scriptsDir.c_str(), 0755);
    mkdir(libDir.c_str(), 0755);
}

std::string ShellCore::resolveFilePath(const std::string& filename) const {
    // If it's an absolute path, use as-is
    if (!filename.empty() && filename[0] == '/') {
        return filename;
    }
    
    // If it's a relative path with directory components, check if it exists as-is first
    if (filename.find('/') != std::string::npos) {
        if (fileExists(filename)) {
            return filename;
        }
        // If not found, continue to check other locations
    }
    
    // Check current directory first (for files without path separators)
    if (fileExists(filename)) {
        return filename;
    }
    
    // Check in BASIC scripts directory
    std::string scriptsPath = getBasicScriptsDir() + filename;
    if (fileExists(scriptsPath)) {
        return scriptsPath;
    }
    
    // Then check in lib directory
    std::string libPath = getBasicLibDir() + filename;
    if (fileExists(libPath)) {
        return libPath;
    }
    
    // Default to current directory for new files (if no path separator)
    // or the original path (if it has path separators)
    if (filename.find('/') != std::string::npos) {
        return filename;
    }
    return scriptsPath;
}

} // namespace FasterBASIC
