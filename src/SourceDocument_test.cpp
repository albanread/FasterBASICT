//
//  SourceDocument_test.cpp
//  FasterBASIC Framework - SourceDocument Unit Tests
//
//  Comprehensive unit tests for unified source code structure.
//
//  Copyright © 2024 FasterBASIC. All rights reserved.
//

#include "SourceDocument.h"
#include <cassert>
#include <iostream>
#include <sstream>

using namespace FasterBASIC;

// Test counter
static int g_testsPassed = 0;
static int g_testsFailed = 0;

// Helper macros
#define TEST(name) void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { runTest(#name, test_##name); } \
    } g_testRegistrar_##name; \
    void test_##name()

#define ASSERT(condition) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << #condition << " at line " << __LINE__ << std::endl; \
        g_testsFailed++; \
        return; \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "FAILED: " << #a << " != " << #b << " at line " << __LINE__ << std::endl; \
        std::cerr << "  Expected: " << (b) << std::endl; \
        std::cerr << "  Got:      " << (a) << std::endl; \
        g_testsFailed++; \
        return; \
    }

#define ASSERT_TRUE(condition) ASSERT(condition)
#define ASSERT_FALSE(condition) ASSERT(!(condition))

void runTest(const char* name, void (*testFunc)()) {
    std::cout << "Running test: " << name << "... ";
    testFunc();
    std::cout << "PASSED" << std::endl;
    g_testsPassed++;
}

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST(DefaultConstruction) {
    SourceDocument doc;
    ASSERT_EQ(doc.getLineCount(), 0);  // Starts empty
    ASSERT_FALSE(doc.isDirty());
    ASSERT_FALSE(doc.hasLineNumbers());
}

TEST(CopyConstruction) {
    SourceDocument doc1;
    doc1.setLineByNumber(10, "PRINT \"Hello\"");
    doc1.setLineByNumber(20, "END");
    
    SourceDocument doc2(doc1);
    ASSERT_EQ(doc2.getLineCount(), 2);
    ASSERT_TRUE(doc2.hasLineNumber(10));
    ASSERT_TRUE(doc2.hasLineNumber(20));
}

TEST(MoveConstruction) {
    SourceDocument doc1;
    doc1.setLineByNumber(10, "PRINT \"Hello\"");
    
    SourceDocument doc2(std::move(doc1));
    ASSERT_EQ(doc2.getLineCount(), 1);
    ASSERT_TRUE(doc2.hasLineNumber(10));
}

// =============================================================================
// Line Access Tests
// =============================================================================

TEST(GetLineByIndex) {
    SourceDocument doc;
    doc.insertLineAtIndex(0, "First line", 0);
    doc.insertLineAtIndex(1, "Second line", 0);
    
    const auto& line0 = doc.getLineByIndex(0);
    ASSERT_EQ(line0.text, "First line");
    
    const auto& line1 = doc.getLineByIndex(1);
    ASSERT_EQ(line1.text, "Second line");
}

TEST(GetLineByNumber) {
    SourceDocument doc;
    doc.setLineByNumber(10, "Line 10");
    doc.setLineByNumber(20, "Line 20");
    doc.setLineByNumber(30, "Line 30");
    
    const SourceLine* line = doc.getLineByNumber(20);
    ASSERT_TRUE(line != nullptr);
    ASSERT_EQ(line->text, "Line 20");
    ASSERT_EQ(line->lineNumber, 20);
    
    const SourceLine* missing = doc.getLineByNumber(15);
    ASSERT_TRUE(missing == nullptr);
}

TEST(HasLineNumber) {
    SourceDocument doc;
    doc.setLineByNumber(10, "Test");
    
    ASSERT_TRUE(doc.hasLineNumber(10));
    ASSERT_FALSE(doc.hasLineNumber(20));
}

TEST(GetLineNumbers) {
    SourceDocument doc;
    doc.setLineByNumber(30, "Third");
    doc.setLineByNumber(10, "First");
    doc.setLineByNumber(20, "Second");
    
    auto numbers = doc.getLineNumbers();
    ASSERT_EQ(numbers.size(), 3);
    ASSERT_EQ(numbers[0], 10);
    ASSERT_EQ(numbers[1], 20);
    ASSERT_EQ(numbers[2], 30);
}

// =============================================================================
// REPL-Style Line Modification Tests
// =============================================================================

TEST(SetLineByNumber_Insert) {
    SourceDocument doc;
    doc.setLineByNumber(10, "First");
    doc.setLineByNumber(30, "Third");
    doc.setLineByNumber(20, "Second");
    
    ASSERT_EQ(doc.getLineCount(), 3);
    ASSERT_EQ(doc.getLineByIndex(0).lineNumber, 10);
    ASSERT_EQ(doc.getLineByIndex(1).lineNumber, 20);
    ASSERT_EQ(doc.getLineByIndex(2).lineNumber, 30);
}

TEST(SetLineByNumber_Replace) {
    SourceDocument doc;
    doc.setLineByNumber(10, "Original");
    doc.setLineByNumber(10, "Replaced");
    
    ASSERT_EQ(doc.getLineCount(), 1);
    const SourceLine* line = doc.getLineByNumber(10);
    ASSERT_TRUE(line != nullptr);
    ASSERT_EQ(line->text, "Replaced");
}

TEST(DeleteLineByNumber) {
    SourceDocument doc;
    doc.setLineByNumber(10, "First");
    doc.setLineByNumber(20, "Second");
    doc.setLineByNumber(30, "Third");
    
    ASSERT_TRUE(doc.deleteLineByNumber(20));
    ASSERT_EQ(doc.getLineCount(), 2);
    ASSERT_FALSE(doc.hasLineNumber(20));
    ASSERT_TRUE(doc.hasLineNumber(10));
    ASSERT_TRUE(doc.hasLineNumber(30));
}

// =============================================================================
// Editor-Style Line Modification Tests
// =============================================================================

TEST(InsertLineAtIndex) {
    SourceDocument doc;
    doc.clear();
    
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Third", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    
    ASSERT_EQ(doc.getLineCount(), 3);
    ASSERT_EQ(doc.getLineByIndex(0).text, "First");
    ASSERT_EQ(doc.getLineByIndex(1).text, "Second");
    ASSERT_EQ(doc.getLineByIndex(2).text, "Third");
}

TEST(DeleteLineAtIndex) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    ASSERT_TRUE(doc.deleteLineAtIndex(1));
    ASSERT_EQ(doc.getLineCount(), 2);
    ASSERT_EQ(doc.getLineByIndex(0).text, "First");
    ASSERT_EQ(doc.getLineByIndex(1).text, "Third");
}

TEST(ReplaceLineAtIndex) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Original", 0);
    
    ASSERT_TRUE(doc.replaceLineAtIndex(0, "Replaced"));
    ASSERT_EQ(doc.getLineByIndex(0).text, "Replaced");
}

TEST(SplitLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello World", 0);
    
    ASSERT_TRUE(doc.splitLine(0, 6));
    ASSERT_EQ(doc.getLineCount(), 2);
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello ");
    ASSERT_EQ(doc.getLineByIndex(1).text, "World");
}

TEST(JoinWithNext) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello", 0);
    doc.insertLineAtIndex(1, " World", 0);
    
    ASSERT_TRUE(doc.joinWithNext(0));
    ASSERT_EQ(doc.getLineCount(), 1);
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello World");
}

// =============================================================================
// Character-Level Operations Tests
// =============================================================================

TEST(InsertChar) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hllo", 0);
    
    ASSERT_TRUE(doc.insertChar(0, 1, 'e'));
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello");
}

TEST(DeleteChar) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Helllo", 0);
    
    ASSERT_TRUE(doc.deleteChar(0, 3));
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello");
}

TEST(InsertText) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello", 0);
    
    ASSERT_TRUE(doc.insertText(0, 5, " World"));
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello World");
}

TEST(InsertTextMultiline) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Start End", 0);
    
    ASSERT_TRUE(doc.insertText(0, 6, "Middle\nNew "));
    ASSERT_EQ(doc.getLineCount(), 2);
    ASSERT_EQ(doc.getLineByIndex(0).text, "Start Middle");
    ASSERT_EQ(doc.getLineByIndex(1).text, "New End");
}

// =============================================================================
// Range Operations Tests
// =============================================================================

TEST(GetTextRange_SingleLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello World", 0);
    
    std::string range = doc.getTextRange(0, 0, 0, 5);
    ASSERT_EQ(range, "Hello");
}

TEST(GetTextRange_MultiLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    std::string range = doc.getTextRange(0, 2, 2, 3);
    ASSERT_EQ(range, "rst\nSecond\nThi");
}

TEST(DeleteRange_SingleLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello World", 0);
    
    std::string deleted = doc.deleteRange(0, 6, 0, 11);
    ASSERT_EQ(deleted, "World");
    ASSERT_EQ(doc.getLineByIndex(0).text, "Hello ");
}

TEST(DeleteRange_MultiLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    doc.deleteRange(0, 2, 2, 3);
    ASSERT_EQ(doc.getLineCount(), 1);
    ASSERT_EQ(doc.getLineByIndex(0).text, "Fird");
}

// =============================================================================
// Line Numbering Tests
// =============================================================================

TEST(Renumber) {
    SourceDocument doc;
    doc.setLineByNumber(5, "First");
    doc.setLineByNumber(7, "Second");
    doc.setLineByNumber(9, "Third");
    
    doc.renumber(10, 10);
    
    ASSERT_TRUE(doc.hasLineNumber(10));
    ASSERT_TRUE(doc.hasLineNumber(20));
    ASSERT_TRUE(doc.hasLineNumber(30));
    ASSERT_FALSE(doc.hasLineNumber(5));
}

TEST(AutoNumbering) {
    SourceDocument doc;
    doc.setAutoNumbering(true, 100, 50);
    
    int num1 = doc.getNextAutoNumber();
    int num2 = doc.getNextAutoNumber();
    int num3 = doc.getNextAutoNumber();
    
    ASSERT_EQ(num1, 100);
    ASSERT_EQ(num2, 150);
    ASSERT_EQ(num3, 200);
}

TEST(StripLineNumbers) {
    SourceDocument doc;
    doc.setLineByNumber(10, "First");
    doc.setLineByNumber(20, "Second");
    
    doc.stripLineNumbers();
    
    ASSERT_FALSE(doc.hasLineNumbers());
    ASSERT_EQ(doc.getLineByIndex(0).lineNumber, 0);
    ASSERT_EQ(doc.getLineByIndex(1).lineNumber, 0);
}

TEST(AssignLineNumbers) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    doc.assignLineNumbers(10, 5);
    
    ASSERT_TRUE(doc.isFullyNumbered());
    ASSERT_EQ(doc.getLineByIndex(0).lineNumber, 10);
    ASSERT_EQ(doc.getLineByIndex(1).lineNumber, 15);
    ASSERT_EQ(doc.getLineByIndex(2).lineNumber, 20);
}

TEST(IsMixedMode) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Numbered", 10);
    doc.insertLineAtIndex(1, "Unnumbered", 0);
    
    ASSERT_TRUE(doc.isMixedMode());
    ASSERT_TRUE(doc.hasLineNumbers());
    ASSERT_FALSE(doc.isFullyNumbered());
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST(SetText) {
    SourceDocument doc;
    doc.setText("Line 1\nLine 2\nLine 3");
    
    ASSERT_EQ(doc.getLineCount(), 3);
    ASSERT_EQ(doc.getLineByIndex(0).text, "Line 1");
    ASSERT_EQ(doc.getLineByIndex(1).text, "Line 2");
    ASSERT_EQ(doc.getLineByIndex(2).text, "Line 3");
}

TEST(GetText) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    std::string text = doc.getText();
    ASSERT_EQ(text, "First\nSecond\nThird");
}

TEST(GenerateSourceForCompiler) {
    SourceDocument doc;
    doc.setLineByNumber(10, "PRINT \"Hello\"");
    doc.setLineByNumber(20, "END");
    
    std::string source = doc.generateSourceForCompiler();
    ASSERT_EQ(source, "10 PRINT \"Hello\"\n20 END");
}

TEST(GetTextRangeByNumber) {
    SourceDocument doc;
    doc.setLineByNumber(10, "First");
    doc.setLineByNumber(20, "Second");
    doc.setLineByNumber(30, "Third");
    doc.setLineByNumber(40, "Fourth");
    
    std::string range = doc.getTextRangeByNumber(20, 30);
    ASSERT_EQ(range, "20 Second\n30 Third\n");
}

// =============================================================================
// Undo/Redo Tests
// =============================================================================

TEST(UndoRedo_Basic) {
    SourceDocument doc;
    doc.pushUndoState();
    
    doc.setLineByNumber(10, "Test");
    
    ASSERT_TRUE(doc.hasLineNumber(10));
    
    ASSERT_TRUE(doc.undo());
    ASSERT_EQ(doc.getLineCount(), 0);
    
    ASSERT_TRUE(doc.redo());
    ASSERT_TRUE(doc.hasLineNumber(10));
}

TEST(UndoRedo_MultipleSteps) {
    SourceDocument doc;
    doc.pushUndoState();
    
    doc.setLineByNumber(10, "First");
    doc.pushUndoState();
    
    doc.setLineByNumber(20, "Second");
    doc.pushUndoState();
    
    doc.setLineByNumber(30, "Third");
    
    ASSERT_EQ(doc.getLineCount(), 3);
    
    doc.undo();
    ASSERT_EQ(doc.getLineCount(), 2);
    
    doc.undo();
    ASSERT_EQ(doc.getLineCount(), 1);
    
    doc.redo();
    ASSERT_EQ(doc.getLineCount(), 2);
}

TEST(CanUndoRedo) {
    SourceDocument doc;
    
    ASSERT_FALSE(doc.canUndo());
    ASSERT_FALSE(doc.canRedo());
    
    doc.pushUndoState();
    doc.setLineByNumber(10, "Test");
    
    ASSERT_TRUE(doc.canUndo());
    ASSERT_FALSE(doc.canRedo());
    
    doc.undo();
    ASSERT_TRUE(doc.canRedo());
}

// =============================================================================
// Dirty State Tests
// =============================================================================

TEST(DirtyState_Modification) {
    SourceDocument doc;
    doc.markClean();
    ASSERT_FALSE(doc.isDirty());
    
    doc.setLineByNumber(10, "Test");
    ASSERT_TRUE(doc.isDirty());
}

TEST(DirtyLines) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 0);
    doc.insertLineAtIndex(1, "Second", 0);
    doc.insertLineAtIndex(2, "Third", 0);
    
    doc.markLinesClean();
    
    doc.replaceLineAtIndex(1, "Modified");
    
    auto dirtyLines = doc.getDirtyLines();
    ASSERT_EQ(dirtyLines.size(), 1);
    ASSERT_EQ(dirtyLines[0], 1);
}

TEST(VersionIncrement) {
    SourceDocument doc;
    uint64_t v1 = doc.getVersion();
    
    doc.setLineByNumber(10, "Test");
    uint64_t v2 = doc.getVersion();
    
    ASSERT_TRUE(v2 > v1);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST(Statistics) {
    SourceDocument doc;
    doc.setLineByNumber(10, "First line");
    doc.setLineByNumber(20, "Second");
    doc.insertLineAtIndex(2, "Unnumbered", 0);
    
    auto stats = doc.getStatistics();
    
    ASSERT_EQ(stats.lineCount, 3);
    ASSERT_EQ(stats.numberedLines, 2);
    ASSERT_EQ(stats.unnumberedLines, 1);
    ASSERT_TRUE(stats.hasLineNumbers);
    ASSERT_TRUE(stats.hasMixedNumbering);
    ASSERT_EQ(stats.minLineNumber, 10);
    ASSERT_EQ(stats.maxLineNumber, 20);
}

// =============================================================================
// Search Tests
// =============================================================================

TEST(Find_CaseSensitive) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello World", 0);
    doc.insertLineAtIndex(1, "hello world", 0);
    
    auto results = doc.find("Hello", true);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0].lineIndex, 0);
    ASSERT_EQ(results[0].column, 0);
}

TEST(Find_CaseInsensitive) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello World", 0);
    doc.insertLineAtIndex(1, "hello world", 0);
    
    auto results = doc.find("hello", false);
    ASSERT_EQ(results.size(), 2);
}

TEST(ReplaceAll) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "foo bar foo", 0);
    doc.insertLineAtIndex(1, "foo baz", 0);
    
    size_t count = doc.replaceAll("foo", "qux");
    ASSERT_EQ(count, 3);
    ASSERT_EQ(doc.getLineByIndex(0).text, "qux bar qux");
    ASSERT_EQ(doc.getLineByIndex(1).text, "qux baz");
}

// =============================================================================
// Utility Tests
// =============================================================================

TEST(Clear) {
    SourceDocument doc;
    doc.setLineByNumber(10, "Test");
    doc.setLineByNumber(20, "Test");
    
    doc.clear();
    
    ASSERT_EQ(doc.getLineCount(), 0);
    ASSERT_FALSE(doc.hasLineNumbers());
}

TEST(IsValidPosition) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello", 0);
    
    ASSERT_TRUE(doc.isValidPosition(0, 0));
    ASSERT_TRUE(doc.isValidPosition(0, 5));
    ASSERT_FALSE(doc.isValidPosition(0, 6));
    ASSERT_FALSE(doc.isValidPosition(1, 0));
}

TEST(ClampPosition) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "Hello", 0);
    
    size_t line = 10;
    size_t col = 100;
    
    doc.clampPosition(line, col);
    
    ASSERT_EQ(line, 0);
    ASSERT_EQ(col, 5);
}

TEST(SplitLines) {
    auto lines = SourceDocument::splitLines("Line1\nLine2\r\nLine3\rLine4");
    ASSERT_EQ(lines.size(), 4);
    ASSERT_EQ(lines[0], "Line1");
    ASSERT_EQ(lines[1], "Line2");
    ASSERT_EQ(lines[2], "Line3");
    ASSERT_EQ(lines[3], "Line4");
}

TEST(ForEachLine) {
    SourceDocument doc;
    doc.clear();
    doc.insertLineAtIndex(0, "First", 10);
    doc.insertLineAtIndex(1, "Second", 20);
    doc.insertLineAtIndex(2, "Third", 30);
    
    int count = 0;
    doc.forEachLine([&count](const SourceLine& line, size_t index) {
        count++;
        ASSERT_EQ(line.lineNumber, (index + 1) * 10);
    });
    
    ASSERT_EQ(count, 3);
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST(EmptyDocument) {
    SourceDocument doc;
    doc.clear();
    
    ASSERT_EQ(doc.getLineCount(), 0);  // Truly empty
    ASSERT_TRUE(doc.isEmpty());
}

TEST(VeryLongLine) {
    SourceDocument doc;
    std::string longLine(10000, 'x');
    doc.clear();
    doc.insertLineAtIndex(0, longLine, 0);
    
    ASSERT_EQ(doc.getLineByIndex(0).text.length(), 10000);
}

TEST(ManyLines) {
    SourceDocument doc;
    doc.clear();
    
    for (int i = 0; i < 1000; ++i) {
        doc.insertLineAtIndex(i, "Line " + std::to_string(i), 0);
    }
    
    ASSERT_EQ(doc.getLineCount(), 1000);
}

TEST(LineNumberOrdering) {
    SourceDocument doc;
    // Insert in random order
    doc.setLineByNumber(50, "50");
    doc.setLineByNumber(10, "10");
    doc.setLineByNumber(30, "30");
    doc.setLineByNumber(20, "20");
    doc.setLineByNumber(40, "40");
    
    // Should be sorted by line number
    ASSERT_EQ(doc.getLineByIndex(0).lineNumber, 10);
    ASSERT_EQ(doc.getLineByIndex(1).lineNumber, 20);
    ASSERT_EQ(doc.getLineByIndex(2).lineNumber, 30);
    ASSERT_EQ(doc.getLineByIndex(3).lineNumber, 40);
    ASSERT_EQ(doc.getLineByIndex(4).lineNumber, 50);
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "SourceDocument Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Tests run automatically via static initialization
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << g_testsPassed << std::endl;
    std::cout << "Failed: " << g_testsFailed << std::endl;
    std::cout << "Total:  " << (g_testsPassed + g_testsFailed) << std::endl;
    
    if (g_testsFailed == 0) {
        std::cout << std::endl;
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << std::endl;
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}