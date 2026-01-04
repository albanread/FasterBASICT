//
// basic_formatter.h
// FasterBASIC - BASIC Code Formatter and Renumberer
//
// Public interface for BASIC code formatting and renumbering functionality.
//

#ifndef BASIC_FORMATTER_H
#define BASIC_FORMATTER_H

#include <string>
#include <vector>
#include <map>

namespace FasterBASIC {

// Structure to hold a parsed BASIC line
struct BasicLine {
    int original_line_number;
    int new_line_number;
    std::string content;  // Line content after the line number
    int indent_level;
};

// Check if a token is a block-opening keyword
bool isBlockOpener(const std::string& token);

// Check if a token is a block-closing keyword
bool isBlockCloser(const std::string& token);

// Check if a token is a middle-block keyword (ELSE, ELSEIF, WHEN)
bool isMiddleBlock(const std::string& token);

// Extract line number from start of line
// Returns -1 if no line number found
// Updates pos to position after line number and whitespace
int extractLineNumber(const std::string& line, size_t& pos);

// Tokenize a line into words (simple tokenizer)
// Handles strings, comments, and basic delimiters
std::vector<std::string> tokenizeLine(const std::string& content);

// Calculate indent level change for a line
// indent_before: how much to decrease indent before this line
// indent_after: how much to increase indent after this line
void calculateIndent(const std::string& content, int& indent_before, int& indent_after);

// Parse a BASIC program into lines with indent levels
std::vector<BasicLine> parseProgram(const std::string& source);

// Build mapping from old to new line numbers
std::map<int, int> buildLineMapping(std::vector<BasicLine>& lines, 
                                     int start_line, int step);

// Replace line number references in content (GOTO, GOSUB, RESTORE, etc.)
std::string replaceLineRefs(const std::string& content, 
                            const std::map<int, int>& mapping);

// Format the program with new line numbers and indentation
std::string formatProgram(std::vector<BasicLine>& lines, 
                         const std::map<int, int>& mapping);

} // namespace FasterBASIC

#endif // BASIC_FORMATTER_H