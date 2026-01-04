//
// fasterbasic_semantic.h
// FasterBASIC - Semantic Analyzer
//
// Validates AST, builds symbol tables, performs type checking, and prepares
// the program for execution. This is Phase 3 of the compilation pipeline.
//

#ifndef FASTERBASIC_SEMANTIC_H
#define FASTERBASIC_SEMANTIC_H

#include "fasterbasic_ast.h"
#include "fasterbasic_token.h"
#include "fasterbasic_options.h"
#include "../runtime/ConstantsManager.h"
#include "modular_commands.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <stack>
#include <memory>
#include <sstream>

namespace FasterBASIC {

// =============================================================================
// Type System
// =============================================================================

enum class VariableType {
    INT,        // Integer (%)
    FLOAT,      // Single precision (! or default)
    DOUBLE,     // Double precision (#)
    STRING,     // String ($) - byte-based
    UNICODE,    // Unicode string ($) - codepoint array (OPTION UNICODE mode)
    VOID,       // No return value (for SUB)
    UNKNOWN     // Not yet determined
};

inline const char* typeToString(VariableType type) {
    switch (type) {
        case VariableType::INT: return "INTEGER";
        case VariableType::FLOAT: return "FLOAT";
        case VariableType::DOUBLE: return "DOUBLE";
        case VariableType::STRING: return "STRING";
        case VariableType::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// =============================================================================
// Symbol Tables
// =============================================================================

// Variable symbol
struct VariableSymbol {
    std::string name;
    VariableType type;
    bool isDeclared;        // Explicit declaration vs implicit
    bool isUsed;
    SourceLocation firstUse;

    VariableSymbol()
        : type(VariableType::UNKNOWN), isDeclared(false), isUsed(false) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << name << " : " << typeToString(type);
        if (!isDeclared) oss << " [implicit]";
        if (!isUsed) oss << " [unused]";
        return oss.str();
    }
};

// Array symbol
struct ArraySymbol {
    std::string name;
    VariableType type;
    std::vector<int> dimensions;
    bool isDeclared;
    SourceLocation declaration;
    int totalSize;          // Product of all dimensions

    ArraySymbol()
        : type(VariableType::UNKNOWN), isDeclared(false), totalSize(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << name << "(";
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << dimensions[i];
        }
        oss << ") : " << typeToString(type);
        oss << " [" << totalSize << " elements]";
        return oss.str();
    }
};

// Function symbol (DEF FN)
struct FunctionSymbol {
    std::string name;
    std::vector<std::string> parameters;
    VariableType returnType;
    SourceLocation definition;
    const Expression* body;     // Pointer to AST node (not owned)

    FunctionSymbol()
        : returnType(VariableType::UNKNOWN), body(nullptr) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "FN " << name << "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << parameters[i];
        }
        oss << ") : " << typeToString(returnType);
        return oss.str();
    }
};

// Line number symbol
struct LineNumberSymbol {
    int lineNumber;
    size_t programLineIndex;    // Index in Program::lines
    std::vector<SourceLocation> references;  // Where referenced (GOTO, GOSUB, etc.)

    LineNumberSymbol() : lineNumber(0), programLineIndex(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "Line " << lineNumber << " (index " << programLineIndex << ")";
        if (!references.empty()) {
            oss << " - referenced " << references.size() << " time(s)";
        }
        return oss.str();
    }
};

// Label symbol (for :label)
struct LabelSymbol {
    std::string name;
    int labelId;                // Unique numeric ID for code generation
    size_t programLineIndex;    // Index in Program::lines where defined
    SourceLocation definition;
    std::vector<SourceLocation> references;  // Where referenced (GOTO, GOSUB)

    LabelSymbol() : labelId(0), programLineIndex(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "Label :" << name << " (ID " << labelId << ", index " << programLineIndex << ")";
        if (!references.empty()) {
            oss << " - referenced " << references.size() << " time(s)";
        }
        return oss.str();
    }
};

// Data segment (for DATA/READ/RESTORE)
struct DataSegment {
    std::vector<std::string> values;
    size_t readPointer;
    std::unordered_map<int, size_t> restorePoints;  // Line number -> position
    std::unordered_map<std::string, size_t> labelRestorePoints;  // Label name -> position

    DataSegment() : readPointer(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "DATA segment: " << values.size() << " values";
        if (!restorePoints.empty()) {
            oss << ", " << restorePoints.size() << " line RESTORE points";
        }
        if (!labelRestorePoints.empty()) {
            oss << ", " << labelRestorePoints.size() << " label RESTORE points";
        }
        return oss.str();
    }
};

// Constant value (compile-time evaluated)
struct ConstantSymbol {
    enum class Type { INTEGER, DOUBLE, STRING } type;
    union {
        int64_t intValue;
        double doubleValue;
    };
    std::string stringValue;
    int index;  // Index in C++ ConstantsManager for efficient lookup

    ConstantSymbol() : type(Type::INTEGER), intValue(0), index(-1) {}
    explicit ConstantSymbol(int64_t val) : type(Type::INTEGER), intValue(val), index(-1) {}
    explicit ConstantSymbol(double val) : type(Type::DOUBLE), doubleValue(val), index(-1) {}
    explicit ConstantSymbol(const std::string& val) : type(Type::STRING), stringValue(val), index(-1) {}
};

// Complete symbol table
struct SymbolTable {
    std::unordered_map<std::string, VariableSymbol> variables;
    std::unordered_map<std::string, ArraySymbol> arrays;
    std::unordered_map<std::string, FunctionSymbol> functions;
    std::unordered_map<int, LineNumberSymbol> lineNumbers;
    std::unordered_map<std::string, LabelSymbol> labels;  // Symbolic labels
    std::unordered_map<std::string, ConstantSymbol> constants;  // Compile-time constants
    DataSegment dataSegment;
    int nextLabelId = 10000;  // Start label IDs at 10000 to avoid conflicts with line numbers
    int arrayBase = 1;  // OPTION BASE: 0 or 1 (default 1 to match Lua arrays)
    bool unicodeMode = false;  // OPTION UNICODE: if true, strings are represented as codepoint arrays
    bool errorTracking = true;  // OPTION ERROR: if true, emit _LINE tracking for error messages
    bool cancellableLoops = true;  // OPTION CANCELLABLE: if true, inject script cancellation checks in loops
    bool eventsUsed = false;  // EVENT DETECTION: if true, program uses ON EVENT statements and needs event processing code

    std::string toString() const;
};

// =============================================================================
// Errors and Warnings
// =============================================================================

enum class SemanticErrorType {
    UNDEFINED_LINE,
    UNDEFINED_LABEL,
    DUPLICATE_LABEL,
    UNDEFINED_VARIABLE,
    UNDEFINED_ARRAY,
    UNDEFINED_FUNCTION,
    ARRAY_NOT_DECLARED,
    ARRAY_REDECLARED,
    FUNCTION_REDECLARED,
    TYPE_MISMATCH,
    WRONG_DIMENSION_COUNT,
    INVALID_ARRAY_INDEX,
    CONTROL_FLOW_MISMATCH,
    NEXT_WITHOUT_FOR,
    WEND_WITHOUT_WHILE,
    UNTIL_WITHOUT_REPEAT,
    LOOP_WITHOUT_DO,
    FOR_WITHOUT_NEXT,
    WHILE_WITHOUT_WEND,
    DO_WITHOUT_LOOP,
    REPEAT_WITHOUT_UNTIL,
    RETURN_WITHOUT_GOSUB,
    DUPLICATE_LINE_NUMBER
};

struct SemanticError {
    SemanticErrorType type;
    std::string message;
    SourceLocation location;

    SemanticError(SemanticErrorType t, const std::string& msg, const SourceLocation& loc)
        : type(t), message(msg), location(loc) {}

    std::string toString() const {
        return "Semantic Error at " + location.toString() + ": " + message;
    }
};

struct SemanticWarning {
    std::string message;
    SourceLocation location;

    SemanticWarning(const std::string& msg, const SourceLocation& loc)
        : message(msg), location(loc) {}

    std::string toString() const {
        return "Warning at " + location.toString() + ": " + message;
    }
};

// =============================================================================
// Semantic Analyzer
// =============================================================================

class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    explicit SemanticAnalyzer(const ModularCommands::CommandRegistry* registry);
    ~SemanticAnalyzer();

    // Main analysis entry point
    // Takes compiler options from OPTION statements (already collected by parser)
    bool analyze(Program& program, const CompilerOptions& options);

    // Get results
    const SymbolTable& getSymbolTable() const { return m_symbolTable; }
    const std::vector<SemanticError>& getErrors() const { return m_errors; }
    const std::vector<SemanticWarning>& getWarnings() const { return m_warnings; }
    bool hasErrors() const { return !m_errors.empty(); }
    ConstantsManager& getConstantsManager() { return m_constantsManager; }

    // Load functions from command registry
    void loadFromCommandRegistry(const ModularCommands::CommandRegistry& registry);
    const ConstantsManager& getConstantsManager() const { return m_constantsManager; }

    // Configuration
    void setStrictMode(bool strict) { m_strictMode = strict; }
    void setWarnUnused(bool warn) { m_warnUnused = warn; }
    void setRequireExplicitDim(bool require) { m_requireExplicitDim = require; }

    // Register DATA labels (from preprocessor) so RESTORE can find them
    void registerDataLabels(const std::map<std::string, int>& dataLabels);

    // Inject runtime constants (from host environment like FBRunner3)
    // These appear as if they were CONSTANT statements in the source
    void injectRuntimeConstant(const std::string& name, int64_t value);
    void injectRuntimeConstant(const std::string& name, double value);
    void injectRuntimeConstant(const std::string& name, const std::string& value);

    // Report generation
    std::string generateReport() const;

    // Constant expression evaluation (compile-time)
    // Uses FasterBASIC::ConstantValue from ConstantsManager.h
    FasterBASIC::ConstantValue evaluateConstantExpression(const Expression& expr);

private:
    // Two-pass analysis
    void pass1_collectDeclarations(Program& program);
    void pass2_validate(Program& program);

    // Pass 1: Declaration collection
    void collectLineNumbers(Program& program);
    void collectLabels(Program& program);
    void collectOptionStatements(Program& program);
    void collectDimStatements(Program& program);
    void collectDefStatements(Program& program);
    void collectFunctionAndSubStatements(Program& program);
    void collectDataStatements(Program& program);
    void collectConstantStatements(Program& program);

    void processDimStatement(const DimStatement& stmt);
    void processFunctionStatement(const FunctionStatement& stmt);
    void processSubStatement(const SubStatement& stmt);
    void processDefStatement(const DefStatement& stmt);
    void processDataStatement(const DataStatement& stmt, int lineNumber,
                             const std::string& dataLabel);
    void processConstantStatement(const ConstantStatement& stmt);

    // Pass 2: Validation
    void validateProgramLine(const ProgramLine& line);
    void validateStatement(const Statement& stmt);
    void validateExpression(const Expression& expr);

    // Statement validation
    void validatePrintStatement(const PrintStatement& stmt);
    void validateConsoleStatement(const ConsoleStatement& stmt);
    void validateInputStatement(const InputStatement& stmt);
    void validateLetStatement(const LetStatement& stmt);
    void validateGotoStatement(const GotoStatement& stmt);
    void validateGosubStatement(const GosubStatement& stmt);
    void validateIfStatement(const IfStatement& stmt);
    void validateForStatement(const ForStatement& stmt);
    void validateForInStatement(const ForInStatement& stmt);
    void validateNextStatement(const NextStatement& stmt);
    void validateWhileStatement(const WhileStatement& stmt);
    void validateWendStatement(const WendStatement& stmt);
    void validateRepeatStatement(const RepeatStatement& stmt);
    void validateUntilStatement(const UntilStatement& stmt);
    void validateDoStatement(const DoStatement& stmt);
    void validateLoopStatement(const LoopStatement& stmt);
    void validateReadStatement(const ReadStatement& stmt);
    void validateRestoreStatement(const RestoreStatement& stmt);
    void validateExpressionStatement(const ExpressionStatement& stmt);
    void validateOnEventStatement(const OnEventStatement& stmt);

    // Expression validation and type inference
    VariableType inferExpressionType(const Expression& expr);
    VariableType inferBinaryExpressionType(const BinaryExpression& expr);
    VariableType inferUnaryExpressionType(const UnaryExpression& expr);
    VariableType inferVariableType(const VariableExpression& expr);
    VariableType inferArrayAccessType(const ArrayAccessExpression& expr);
    VariableType inferFunctionCallType(const FunctionCallExpression& expr);
    VariableType inferRegistryFunctionType(const RegistryFunctionExpression& expr);

    // Type checking
    void checkTypeCompatibility(VariableType expected, VariableType actual,
                               const SourceLocation& loc, const std::string& context);
    VariableType promoteTypes(VariableType left, VariableType right);
    bool isNumericType(VariableType type);

    // Symbol table management
    VariableSymbol* declareVariable(const std::string& name, VariableType type,
                                   const SourceLocation& loc, bool isDeclared = false);
    ArraySymbol* declareArray(const std::string& name, VariableType type,
                            const std::vector<int>& dimensions,
                            const SourceLocation& loc);
    FunctionSymbol* declareFunction(const std::string& name,
                                   const std::vector<std::string>& params,
                                   const Expression* body,
                                   const SourceLocation& loc);

    VariableSymbol* lookupVariable(const std::string& name);
    ArraySymbol* lookupArray(const std::string& name);
    FunctionSymbol* lookupFunction(const std::string& name);
    LineNumberSymbol* lookupLine(int lineNumber);
    LabelSymbol* lookupLabel(const std::string& name);

    // Label management
    LabelSymbol* declareLabel(const std::string& name, size_t programLineIndex,
                             const SourceLocation& loc);
    int resolveLabelToId(const std::string& name, const SourceLocation& loc);

    // Variable/array usage tracking
    void useVariable(const std::string& name, const SourceLocation& loc);
    void useArray(const std::string& name, size_t dimensionCount, const SourceLocation& loc);

    // Type suffix handling
    VariableType inferTypeFromSuffix(TokenType suffix);
    VariableType inferTypeFromName(const std::string& name);

    // Built-in function support
    bool isBuiltinFunction(const std::string& name) const;
    VariableType getBuiltinReturnType(const std::string& name) const;
    int getBuiltinArgCount(const std::string& name) const;

    // Control flow validation
    void validateControlFlow(Program& program);
    void checkUnusedVariables();

    // Error reporting
    void error(SemanticErrorType type, const std::string& message,
              const SourceLocation& loc);
    void warning(const std::string& message, const SourceLocation& loc);

    // Constant expression evaluation helpers
    FasterBASIC::ConstantValue evalConstantBinary(const BinaryExpression& expr);
    FasterBASIC::ConstantValue evalConstantUnary(const UnaryExpression& expr);
    FasterBASIC::ConstantValue evalConstantFunction(const FunctionCallExpression& expr);
    FasterBASIC::ConstantValue evalConstantVariable(const VariableExpression& expr);

    // Type promotion for constant operations
    bool isConstantNumeric(const FasterBASIC::ConstantValue& val);
    double getConstantAsDouble(const FasterBASIC::ConstantValue& val);
    int64_t getConstantAsInt(const FasterBASIC::ConstantValue& val);

    // Data
    SymbolTable m_symbolTable;
    std::vector<SemanticError> m_errors;
    std::vector<SemanticWarning> m_warnings;
    ConstantsManager m_constantsManager;

    // Configuration
    bool m_strictMode;
    bool m_warnUnused;
    bool m_requireExplicitDim;
    bool m_cancellableLoops;

    // Control flow stacks (for validation)
    struct ForContext {
        std::string variable;
        SourceLocation location;
    };
    std::stack<ForContext> m_forStack;
    std::stack<SourceLocation> m_whileStack;
    std::stack<SourceLocation> m_repeatStack;
    std::stack<SourceLocation> m_doStack;

    // Current analysis context
    const Program* m_program;
    int m_currentLineNumber;

    // Built-in function registry
    std::unordered_map<std::string, int> m_builtinFunctions;  // name -> arg count
    void initializeBuiltinFunctions();
};

} // namespace FasterBASIC

#endif // FASTERBASIC_SEMANTIC_H
