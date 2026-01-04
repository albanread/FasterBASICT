//
//  CompilerView.h
//  FasterBASIC - Compiler View Adapter
//
//  Adapter class that provides compiler-oriented interface to SourceDocument.
//  Designed for parser/compiler consumption with efficient iteration.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#ifndef COMPILER_VIEW_H
#define COMPILER_VIEW_H

#include "SourceDocument.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace FasterBASIC {

// =============================================================================
// CompilerView - Compiler-oriented view of SourceDocument
// =============================================================================

class CompilerView {
public:
    // =========================================================================
    // Construction
    // =========================================================================
    
    /// Create view from SourceDocument
    explicit CompilerView(const SourceDocument& document);
    
    /// Create view from shared pointer
    explicit CompilerView(std::shared_ptr<SourceDocument> document);
    
    /// Destructor
    ~CompilerView();
    
    // =========================================================================
    // Compiler Line Structure
    // =========================================================================
    
    /// Line structure for compilation
    struct CompilerLine {
        int lineNumber;         // BASIC line number (0 if unnumbered)
        std::string text;       // Line text
        size_t originalIndex;   // Original index in document (for error reporting)
        
        CompilerLine(int num, const std::string& txt, size_t idx)
            : lineNumber(num), text(txt), originalIndex(idx) {}
    };
    
    // =========================================================================
    // Source Access
    // =========================================================================
    
    /// Get source text for compilation
    /// @return Source code with line numbers (if present)
    std::string getSourceText() const;
    
    /// Get all lines as vector
    /// @return Vector of compiler lines
    std::vector<CompilerLine> getLines() const;
    
    /// Get line count
    size_t getLineCount() const;
    
    // =========================================================================
    // Efficient Iteration
    // =========================================================================
    
    /// Iterate over each line with callback
    /// @param callback Function called for each line
    void forEachLine(std::function<void(const CompilerLine&)> callback) const;
    
    /// Iterate with index
    /// @param callback Function called for each line with index
    void forEachLineIndexed(std::function<void(const CompilerLine&, size_t)> callback) const;
    
    // =========================================================================
    // Error Reporting Helpers
    // =========================================================================
    
    /// Get source location for error reporting
    /// @param lineIndex Line index (from CompilerLine.originalIndex)
    /// @param column Column position
    /// @return DocumentLocation structure
    DocumentLocation getLocation(size_t lineIndex, size_t column) const;
    
    /// Get line number for index
    /// @param lineIndex Line index
    /// @return BASIC line number (0 if unnumbered)
    int getLineNumber(size_t lineIndex) const;
    
    // =========================================================================
    // Metadata
    // =========================================================================
    
    /// Get filename
    std::string getFilename() const;
    
    /// Check if document has line numbers
    bool hasLineNumbers() const;
    
    /// Check if document is in mixed mode (some numbered, some not)
    bool isMixedMode() const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /// Compiler statistics
    struct Statistics {
        size_t lineCount;
        size_t totalCharacters;
        bool hasLineNumbers;
        int minLineNumber;
        int maxLineNumber;
        
        Statistics()
            : lineCount(0), totalCharacters(0)
            , hasLineNumbers(false)
            , minLineNumber(0), maxLineNumber(0) {}
    };
    
    /// Get statistics
    Statistics getStatistics() const;

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    const SourceDocument* m_document;               // Document pointer (non-owning or owning)
    std::shared_ptr<SourceDocument> m_sharedDoc;    // Shared pointer (if provided)
};

} // namespace FasterBASIC

#endif // COMPILER_VIEW_H
