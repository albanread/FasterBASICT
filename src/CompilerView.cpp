//
//  CompilerView.cpp
//  FasterBASIC - Compiler View Adapter
//
//  Implementation of compiler-oriented view adapter for SourceDocument.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#include "CompilerView.h"

namespace FasterBASIC {

// =============================================================================
// Construction
// =============================================================================

CompilerView::CompilerView(const SourceDocument& document)
    : m_document(&document)
    , m_sharedDoc(nullptr)
{
}

CompilerView::CompilerView(std::shared_ptr<SourceDocument> document)
    : m_document(document.get())
    , m_sharedDoc(document)
{
}

CompilerView::~CompilerView() = default;

// =============================================================================
// Source Access
// =============================================================================

std::string CompilerView::getSourceText() const {
    return m_document->generateSourceForCompiler();
}

std::vector<CompilerView::CompilerLine> CompilerView::getLines() const {
    std::vector<CompilerLine> result;
    
    const auto& lines = m_document->getLines();
    result.reserve(lines.size());
    
    for (size_t i = 0; i < lines.size(); ++i) {
        result.emplace_back(lines[i].lineNumber, lines[i].text, i);
    }
    
    return result;
}

size_t CompilerView::getLineCount() const {
    return m_document->getLineCount();
}

// =============================================================================
// Efficient Iteration
// =============================================================================

void CompilerView::forEachLine(std::function<void(const CompilerLine&)> callback) const {
    const auto& lines = m_document->getLines();
    
    for (size_t i = 0; i < lines.size(); ++i) {
        CompilerLine line(lines[i].lineNumber, lines[i].text, i);
        callback(line);
    }
}

void CompilerView::forEachLineIndexed(std::function<void(const CompilerLine&, size_t)> callback) const {
    const auto& lines = m_document->getLines();
    
    for (size_t i = 0; i < lines.size(); ++i) {
        CompilerLine line(lines[i].lineNumber, lines[i].text, i);
        callback(line, i);
    }
}

// =============================================================================
// Error Reporting Helpers
// =============================================================================

DocumentLocation CompilerView::getLocation(size_t lineIndex, size_t column) const {
    return m_document->getLocation(lineIndex, column);
}

int CompilerView::getLineNumber(size_t lineIndex) const {
    if (lineIndex >= m_document->getLineCount()) {
        return 0;
    }
    return m_document->getLineByIndex(lineIndex).lineNumber;
}

// =============================================================================
// Metadata
// =============================================================================

std::string CompilerView::getFilename() const {
    return m_document->getFilename();
}

bool CompilerView::hasLineNumbers() const {
    return m_document->hasLineNumbers();
}

bool CompilerView::isMixedMode() const {
    return m_document->isMixedMode();
}

// =============================================================================
// Statistics
// =============================================================================

CompilerView::Statistics CompilerView::getStatistics() const {
    Statistics stats;
    
    auto docStats = m_document->getStatistics();
    
    stats.lineCount = docStats.lineCount;
    stats.totalCharacters = docStats.totalCharacters;
    stats.hasLineNumbers = docStats.hasLineNumbers;
    stats.minLineNumber = docStats.minLineNumber;
    stats.maxLineNumber = docStats.maxLineNumber;
    
    return stats;
}

} // namespace FasterBASIC
