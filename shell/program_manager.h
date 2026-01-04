//
// program_manager.h
// FasterBASIC Shell - Program Storage and Management
//
// Manages BASIC program lines in memory, handles line insertion/deletion,
// renumbering, and program generation for compilation.
//

#ifndef PROGRAM_MANAGER_H
#define PROGRAM_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <utility>

namespace FasterBASIC {

class ProgramManager {
public:
    ProgramManager();
    ~ProgramManager();

    // Line management
    void setLine(int lineNumber, const std::string& code);
    void deleteLine(int lineNumber);
    std::string getLine(int lineNumber) const;
    bool hasLine(int lineNumber) const;
    void clear();

    // Program queries
    bool isEmpty() const;
    bool isModified() const;
    void setModified(bool modified = true);
    size_t getLineCount() const;
    
    // Line number operations
    std::vector<int> getLineNumbers() const;
    int getFirstLineNumber() const;
    int getLastLineNumber() const;
    int getNextLineNumber(int currentLine) const;
    int getPreviousLineNumber(int currentLine) const;

    // Program generation
    std::string generateProgram() const;
    std::string generateProgramRange(int startLine, int endLine = -1) const;
    
    // Renumbering
    void renumber(int startLine = 10, int step = 10);
    
    // File operations
    void setFilename(const std::string& filename);
    std::string getFilename() const;
    bool hasFilename() const;

    // Listing operations
    struct ListRange {
        int startLine;
        int endLine;
        bool hasStart;
        bool hasEnd;
        
        ListRange() : startLine(0), endLine(0), hasStart(false), hasEnd(false) {}
        ListRange(int start, int end) : startLine(start), endLine(end), hasStart(true), hasEnd(true) {}
    };
    
    std::vector<std::pair<int, std::string>> getLines(const ListRange& range) const;
    std::vector<std::pair<int, std::string>> getAllLines() const;

    // Statistics
    struct ProgramStats {
        size_t lineCount;
        size_t totalCharacters;
        int minLineNumber;
        int maxLineNumber;
        bool hasGaps;
        
        ProgramStats() : lineCount(0), totalCharacters(0), minLineNumber(0), maxLineNumber(0), hasGaps(false) {}
    };
    
    ProgramStats getStatistics() const;

    // Auto-numbering support
    void setAutoMode(bool enabled, int start = 10, int step = 10);
    bool isAutoMode() const;
    int getNextAutoLine();
    void incrementAutoLine();

private:
    std::map<int, std::string> m_lines;
    std::string m_filename;
    bool m_modified;
    
    // Auto-numbering state
    bool m_autoMode;
    int m_autoStart;
    int m_autoStep;
    int m_autoCurrentLine;
    
    // Helper functions
    bool isValidLineNumber(int lineNumber) const;
    void updateAutoLine();
    std::vector<std::pair<int, std::string>> filterLinesByRange(const ListRange& range) const;
};

} // namespace FasterBASIC

#endif // PROGRAM_MANAGER_H