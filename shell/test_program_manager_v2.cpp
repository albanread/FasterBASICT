//
// test_program_manager_v2.cpp
// FasterBASIC Shell - ProgramManagerV2 Compatibility Test
//
// Test suite to verify that ProgramManagerV2 maintains full API compatibility
// with the original ProgramManager while using the new SourceDocument architecture.
//

#include "program_manager_v2.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace FasterBASIC;

// Test counter
int g_testsPassed = 0;
int g_testsFailed = 0;

#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        std::cout << "Running test: " #name "..." << std::flush; \
        try { \
            test_##name(); \
            std::cout << " PASSED" << std::endl; \
            g_testsPassed++; \
        } catch (const std::exception& e) { \
            std::cout << " FAILED: " << e.what() << std::endl; \
            g_testsFailed++; \
        } catch (...) { \
            std::cout << " FAILED: Unknown exception" << std::endl; \
            g_testsFailed++; \
        } \
    } \
    void test_##name()

#define ASSERT(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b); \
    }

// =============================================================================
// Basic Operations Tests
// =============================================================================

TEST(empty_program) {
    ProgramManagerV2 pm;
    ASSERT(pm.isEmpty());
    ASSERT_EQ(pm.getLineCount(), 0);
    ASSERT_EQ(pm.getFirstLineNumber(), -1);
    ASSERT_EQ(pm.getLastLineNumber(), -1);
}

TEST(set_and_get_line) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Hello\"");
    
    ASSERT(!pm.isEmpty());
    ASSERT_EQ(pm.getLineCount(), 1);
    ASSERT(pm.hasLine(10));
    ASSERT_EQ(pm.getLine(10), "PRINT \"Hello\"");
}

TEST(multiple_lines) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Line 10\"");
    pm.setLine(20, "PRINT \"Line 20\"");
    pm.setLine(30, "PRINT \"Line 30\"");
    
    ASSERT_EQ(pm.getLineCount(), 3);
    ASSERT_EQ(pm.getFirstLineNumber(), 10);
    ASSERT_EQ(pm.getLastLineNumber(), 30);
}

TEST(delete_line) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Line 10\"");
    pm.setLine(20, "PRINT \"Line 20\"");
    pm.setLine(30, "PRINT \"Line 30\"");
    
    pm.deleteLine(20);
    
    ASSERT_EQ(pm.getLineCount(), 2);
    ASSERT(!pm.hasLine(20));
    ASSERT(pm.hasLine(10));
    ASSERT(pm.hasLine(30));
}

TEST(clear_program) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Line 10\"");
    pm.setLine(20, "PRINT \"Line 20\"");
    
    pm.clear();
    
    ASSERT(pm.isEmpty());
    ASSERT_EQ(pm.getLineCount(), 0);
}

TEST(empty_line_deletion) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Test\"");
    
    // Setting empty line should delete it
    pm.setLine(10, "   ");
    
    ASSERT(!pm.hasLine(10));
    ASSERT(pm.isEmpty());
}

// =============================================================================
// Line Number Operations
// =============================================================================

TEST(get_line_numbers) {
    ProgramManagerV2 pm;
    pm.setLine(10, "A");
    pm.setLine(30, "C");
    pm.setLine(20, "B");
    
    auto lineNums = pm.getLineNumbers();
    
    ASSERT_EQ(lineNums.size(), 3);
    ASSERT_EQ(lineNums[0], 10);
    ASSERT_EQ(lineNums[1], 20);
    ASSERT_EQ(lineNums[2], 30);
}

TEST(next_line_number) {
    ProgramManagerV2 pm;
    pm.setLine(10, "A");
    pm.setLine(20, "B");
    pm.setLine(30, "C");
    
    ASSERT_EQ(pm.getNextLineNumber(10), 20);
    ASSERT_EQ(pm.getNextLineNumber(20), 30);
    ASSERT_EQ(pm.getNextLineNumber(30), -1);
    ASSERT_EQ(pm.getNextLineNumber(15), 20);
}

TEST(previous_line_number) {
    ProgramManagerV2 pm;
    pm.setLine(10, "A");
    pm.setLine(20, "B");
    pm.setLine(30, "C");
    
    ASSERT_EQ(pm.getPreviousLineNumber(30), 20);
    ASSERT_EQ(pm.getPreviousLineNumber(20), 10);
    ASSERT_EQ(pm.getPreviousLineNumber(10), -1);
    ASSERT_EQ(pm.getPreviousLineNumber(25), 20);
}

// =============================================================================
// Program Generation
// =============================================================================

TEST(generate_program) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Hello\"");
    pm.setLine(20, "PRINT \"World\"");
    
    std::string program = pm.generateProgram();
    
    ASSERT(program.find("10 PRINT \"Hello\"") != std::string::npos);
    ASSERT(program.find("20 PRINT \"World\"") != std::string::npos);
}

TEST(generate_program_range) {
    ProgramManagerV2 pm;
    pm.setLine(10, "A");
    pm.setLine(20, "B");
    pm.setLine(30, "C");
    pm.setLine(40, "D");
    
    std::string range = pm.generateProgramRange(20, 30);
    
    ASSERT(range.find("10") == std::string::npos);
    ASSERT(range.find("20 B") != std::string::npos);
    ASSERT(range.find("30 C") != std::string::npos);
    ASSERT(range.find("40") == std::string::npos);
}

// =============================================================================
// Listing Operations
// =============================================================================

TEST(get_all_lines) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"A\"");
    pm.setLine(20, "PRINT \"B\"");
    pm.setLine(30, "PRINT \"C\"");
    
    auto lines = pm.getAllLines();
    
    ASSERT_EQ(lines.size(), 3);
    ASSERT_EQ(lines[0].first, 10);
    ASSERT_EQ(lines[0].second, "PRINT \"A\"");
    ASSERT_EQ(lines[1].first, 20);
    ASSERT_EQ(lines[2].first, 30);
}

TEST(get_lines_with_range) {
    ProgramManagerV2 pm;
    pm.setLine(10, "A");
    pm.setLine(20, "B");
    pm.setLine(30, "C");
    pm.setLine(40, "D");
    
    ProgramManagerV2::ListRange range(20, 30);
    auto lines = pm.getLines(range);
    
    ASSERT_EQ(lines.size(), 2);
    ASSERT_EQ(lines[0].first, 20);
    ASSERT_EQ(lines[1].first, 30);
}

// =============================================================================
// Modified Flag Tests
// =============================================================================

TEST(modified_flag) {
    ProgramManagerV2 pm;
    
    ASSERT(!pm.isModified());
    
    pm.setLine(10, "PRINT \"Test\"");
    ASSERT(pm.isModified());
    
    pm.setModified(false);
    ASSERT(!pm.isModified());
    
    pm.deleteLine(10);
    ASSERT(pm.isModified());
}

// =============================================================================
// Renumbering Tests
// =============================================================================

TEST(renumber_basic) {
    ProgramManagerV2 pm;
    pm.setLine(5, "PRINT \"A\"");
    pm.setLine(7, "PRINT \"B\"");
    pm.setLine(13, "PRINT \"C\"");
    
    pm.renumber(10, 10);
    
    ASSERT(pm.hasLine(10));
    ASSERT(pm.hasLine(20));
    ASSERT(pm.hasLine(30));
    ASSERT(!pm.hasLine(5));
    ASSERT(!pm.hasLine(7));
    ASSERT(!pm.hasLine(13));
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST(statistics) {
    ProgramManagerV2 pm;
    pm.setLine(10, "PRINT \"Hello\"");
    pm.setLine(30, "PRINT \"World\"");
    
    auto stats = pm.getStatistics();
    
    ASSERT_EQ(stats.lineCount, 2);
    ASSERT_EQ(stats.minLineNumber, 10);
    ASSERT_EQ(stats.maxLineNumber, 30);
    ASSERT(stats.hasGaps);  // Gap between 10 and 30
}

// =============================================================================
// Auto-Numbering Tests
// =============================================================================

TEST(auto_numbering) {
    ProgramManagerV2 pm;
    
    pm.setAutoMode(true, 100, 10);
    
    ASSERT(pm.isAutoMode());
    
    int line1 = pm.getNextAutoLine();
    ASSERT_EQ(line1, 100);
    pm.setLine(line1, "PRINT \"First\"");
    
    int line2 = pm.getNextAutoLine();
    ASSERT_EQ(line2, 110);
    pm.setLine(line2, "PRINT \"Second\"");
}

// =============================================================================
// Filename Tests
// =============================================================================

TEST(filename_operations) {
    ProgramManagerV2 pm;
    
    ASSERT(!pm.hasFilename());
    ASSERT_EQ(pm.getFilename(), "");
    
    pm.setFilename("test.bas");
    
    ASSERT(pm.hasFilename());
    ASSERT_EQ(pm.getFilename(), "test.bas");
    
    pm.clear();
    ASSERT(!pm.hasFilename());
}

// =============================================================================
// Undo/Redo Tests (New Functionality)
// =============================================================================

TEST(undo_redo) {
    ProgramManagerV2 pm;
    
    // Undo/redo requires explicit state management
    // This is new functionality not present in original ProgramManager
    
    // Initial state - no undo available yet
    ASSERT(!pm.canUndo());
    ASSERT(!pm.canRedo());
    
    // Make a change and check we can undo it
    pm.setLine(10, "PRINT \"Original\"");
    
    ASSERT(pm.canUndo());
    ASSERT(!pm.canRedo());
    
    pm.undo();
    
    ASSERT(!pm.hasLine(10));
    ASSERT(pm.canRedo());
    
    pm.redo();
    
    ASSERT(pm.hasLine(10));
    ASSERT_EQ(pm.getLine(10), "PRINT \"Original\"");
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "ProgramManagerV2 Compatibility Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Basic operations
    run_test_empty_program();
    run_test_set_and_get_line();
    run_test_multiple_lines();
    run_test_delete_line();
    run_test_clear_program();
    run_test_empty_line_deletion();
    
    // Line number operations
    run_test_get_line_numbers();
    run_test_next_line_number();
    run_test_previous_line_number();
    
    // Program generation
    run_test_generate_program();
    run_test_generate_program_range();
    
    // Listing operations
    run_test_get_all_lines();
    run_test_get_lines_with_range();
    
    // Modified flag
    run_test_modified_flag();
    
    // Renumbering
    run_test_renumber_basic();
    
    // Statistics
    run_test_statistics();
    
    // Auto-numbering
    run_test_auto_numbering();
    
    // Filename
    run_test_filename_operations();
    
    // Undo/redo
    run_test_undo_redo();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "  Passed: " << g_testsPassed << std::endl;
    std::cout << "  Failed: " << g_testsFailed << std::endl;
    std::cout << "========================================" << std::endl;
    
    return g_testsFailed > 0 ? 1 : 0;
}