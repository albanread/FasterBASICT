//
// program_manager.cpp
// FasterBASIC Shell - Program Storage and Management
//
// Manages BASIC program lines in memory, handles line insertion/deletion,
// renumbering, and program generation for compilation.
//

#include "program_manager.h"
#include "../src/basic_formatter_lib.h"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace FasterBASIC {

ProgramManager::ProgramManager() 
    : m_modified(false)
    , m_autoMode(false)
    , m_autoStart(10)
    , m_autoStep(10)
    , m_autoCurrentLine(10)
{
}

ProgramManager::~ProgramManager() {
}

void ProgramManager::setLine(int lineNumber, const std::string& code) {
    if (!isValidLineNumber(lineNumber)) {
        return;
    }
    
    // Trim whitespace from code
    std::string trimmedCode = code;
    // Remove leading/trailing spaces
    size_t start = trimmedCode.find_first_not_of(" \t");
    if (start == std::string::npos) {
        // Empty line - delete it
        deleteLine(lineNumber);
        return;
    }
    
    size_t end = trimmedCode.find_last_not_of(" \t\r\n");
    trimmedCode = trimmedCode.substr(start, end - start + 1);
    
    if (trimmedCode.empty()) {
        // Empty line - delete it
        deleteLine(lineNumber);
        return;
    }
    
    m_lines[lineNumber] = trimmedCode;
    setModified(true);
    
    // Update auto-numbering if this line is at or past current auto line
    if (m_autoMode && lineNumber >= m_autoCurrentLine) {
        m_autoCurrentLine = lineNumber + m_autoStep;
    }
}

void ProgramManager::deleteLine(int lineNumber) {
    auto it = m_lines.find(lineNumber);
    if (it != m_lines.end()) {
        m_lines.erase(it);
        setModified(true);
    }
}

std::string ProgramManager::getLine(int lineNumber) const {
    auto it = m_lines.find(lineNumber);
    if (it != m_lines.end()) {
        return it->second;
    }
    return "";
}

bool ProgramManager::hasLine(int lineNumber) const {
    return m_lines.find(lineNumber) != m_lines.end();
}

void ProgramManager::clear() {
    m_lines.clear();
    m_filename.clear();
    setModified(false);
    
    // Reset auto-numbering to initial state
    m_autoCurrentLine = m_autoStart;
}

bool ProgramManager::isEmpty() const {
    return m_lines.empty();
}

bool ProgramManager::isModified() const {
    return m_modified;
}

void ProgramManager::setModified(bool modified) {
    m_modified = modified;
}

size_t ProgramManager::getLineCount() const {
    return m_lines.size();
}

std::vector<int> ProgramManager::getLineNumbers() const {
    std::vector<int> lineNumbers;
    lineNumbers.reserve(m_lines.size());
    
    for (const auto& pair : m_lines) {
        lineNumbers.push_back(pair.first);
    }
    
    return lineNumbers;
}

int ProgramManager::getFirstLineNumber() const {
    if (m_lines.empty()) {
        return -1;
    }
    return m_lines.begin()->first;
}

int ProgramManager::getLastLineNumber() const {
    if (m_lines.empty()) {
        return -1;
    }
    return m_lines.rbegin()->first;
}

int ProgramManager::getNextLineNumber(int currentLine) const {
    auto it = m_lines.upper_bound(currentLine);
    if (it != m_lines.end()) {
        return it->first;
    }
    return -1;
}

int ProgramManager::getPreviousLineNumber(int currentLine) const {
    auto it = m_lines.lower_bound(currentLine);
    if (it != m_lines.begin()) {
        --it;
        return it->first;
    }
    return -1;
}

std::string ProgramManager::generateProgram() const {
    std::ostringstream oss;
    
    for (const auto& pair : m_lines) {
        oss << pair.first << " " << pair.second << "\n";
    }
    
    return oss.str();
}

std::string ProgramManager::generateProgramRange(int startLine, int endLine) const {
    std::ostringstream oss;
    
    for (const auto& pair : m_lines) {
        int lineNum = pair.first;
        
        // Check if line is in range
        if (lineNum < startLine) continue;
        if (endLine != -1 && lineNum > endLine) break;
        
        oss << lineNum << " " << pair.second << "\n";
    }
    
    return oss.str();
}

void ProgramManager::renumber(int startLine, int step) {
    if (m_lines.empty()) return;
    
    // Build the complete program text
    std::string programText = generateProgram();
    
    // Use the formatter to renumber with reference updating
    FasterBASIC::FormatterOptions options;
    options.start_line = startLine;
    options.step = step;
    options.indent_spaces = 0;  // Don't change indentation
    options.update_references = true;  // Update GOTO/GOSUB references
    options.add_indentation = false;   // Don't add indentation
    
    FasterBASIC::FormatterResult result = FasterBASIC::formatBasicCode(programText, options);
    
    if (!result.success) {
        // If formatting fails, fall back to simple renumbering without reference updating
        std::map<int, std::string> newLines;
        int currentLine = startLine;
        
        for (const auto& pair : m_lines) {
            newLines[currentLine] = pair.second;
            currentLine += step;
        }
        
        m_lines = std::move(newLines);
    } else {
        // Parse the formatted result back into lines
        m_lines.clear();
        std::istringstream stream(result.formatted_code);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            
            // Extract line number and code
            size_t pos = 0;
            while (pos < line.length() && std::isspace(line[pos])) pos++;
            
            if (pos < line.length() && std::isdigit(line[pos])) {
                size_t endPos = pos;
                while (endPos < line.length() && std::isdigit(line[endPos])) endPos++;
                
                int lineNum = std::stoi(line.substr(pos, endPos - pos));
                
                // Skip whitespace after line number
                pos = endPos;
                while (pos < line.length() && std::isspace(line[pos])) pos++;
                
                std::string code = line.substr(pos);
                if (!code.empty()) {
                    m_lines[lineNum] = code;
                }
            }
        }
    }
    
    setModified(true);
    
    // Update auto-numbering
    if (m_autoMode) {
        m_autoStart = startLine;
        m_autoStep = step;
        m_autoCurrentLine = startLine + (m_lines.size() * step);
    }
}

void ProgramManager::setFilename(const std::string& filename) {
    m_filename = filename;
}

std::string ProgramManager::getFilename() const {
    return m_filename;
}

bool ProgramManager::hasFilename() const {
    return !m_filename.empty();
}

std::vector<std::pair<int, std::string>> ProgramManager::getLines(const ListRange& range) const {
    return filterLinesByRange(range);
}

std::vector<std::pair<int, std::string>> ProgramManager::getAllLines() const {
    std::vector<std::pair<int, std::string>> result;
    result.reserve(m_lines.size());
    
    for (const auto& pair : m_lines) {
        result.emplace_back(pair.first, pair.second);
    }
    
    return result;
}

ProgramManager::ProgramStats ProgramManager::getStatistics() const {
    ProgramStats stats;
    
    if (m_lines.empty()) {
        return stats;
    }
    
    stats.lineCount = m_lines.size();
    stats.minLineNumber = getFirstLineNumber();
    stats.maxLineNumber = getLastLineNumber();
    
    // Calculate total characters
    for (const auto& pair : m_lines) {
        stats.totalCharacters += pair.second.length();
    }
    
    // Check for gaps in line numbering
    int expectedNext = stats.minLineNumber + 1;
    for (const auto& pair : m_lines) {
        if (pair.first > expectedNext) {
            stats.hasGaps = true;
            break;
        }
        expectedNext = pair.first + 1;
    }
    
    return stats;
}

void ProgramManager::setAutoMode(bool enabled, int start, int step) {
    m_autoMode = enabled;
    m_autoStart = start;
    m_autoStep = step;
    
    if (enabled) {
        // Set current auto line to first available line
        m_autoCurrentLine = start;
        while (hasLine(m_autoCurrentLine)) {
            m_autoCurrentLine += step;
        }
    }
}

bool ProgramManager::isAutoMode() const {
    return m_autoMode;
}

int ProgramManager::getNextAutoLine() {
    if (!m_autoMode) return -1;
    
    // Find next available line number
    while (hasLine(m_autoCurrentLine)) {
        m_autoCurrentLine += m_autoStep;
    }
    
    return m_autoCurrentLine;
}

void ProgramManager::incrementAutoLine() {
    if (m_autoMode) {
        m_autoCurrentLine += m_autoStep;
    }
}

// Private helper functions

bool ProgramManager::isValidLineNumber(int lineNumber) const {
    return lineNumber > 0 && lineNumber <= 65535;
}

void ProgramManager::updateAutoLine() {
    if (!m_autoMode) return;
    
    // Find the next available line number
    while (hasLine(m_autoCurrentLine)) {
        m_autoCurrentLine += m_autoStep;
    }
}

std::vector<std::pair<int, std::string>> ProgramManager::filterLinesByRange(const ListRange& range) const {
    std::vector<std::pair<int, std::string>> result;
    
    for (const auto& pair : m_lines) {
        int lineNum = pair.first;
        bool includeThis = true;
        
        // Check start range
        if (range.hasStart && lineNum < range.startLine) {
            includeThis = false;
        }
        
        // Check end range
        if (range.hasEnd && lineNum > range.endLine) {
            includeThis = false;
        }
        
        if (includeThis) {
            result.emplace_back(pair.first, pair.second);
        }
    }
    
    return result;
}

} // namespace FasterBASIC