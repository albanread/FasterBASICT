//
// fasterbasic_semantic.cpp
// FasterBASIC - Semantic Analyzer Implementation
//
// Implements two-pass semantic analysis:
// Pass 1: Collect all declarations (line numbers, DIM, DEF FN, DATA)
// Pass 2: Validate usage, type check, control flow validation
//

#include "fasterbasic_semantic.h"
#include "fasterbasic_events.h"
#include <algorithm>
#include <sstream>
#include <cmath>

#ifdef FBRUNNER3_BUILD
#include "../../FBRunner3/register_voice.h"
#endif

namespace FasterBASIC {

// =============================================================================
// SymbolTable toString
// =============================================================================

std::string SymbolTable::toString() const {
    std::ostringstream oss;
    
    oss << "=== SYMBOL TABLE ===\n\n";
    
    // Line numbers
    if (!lineNumbers.empty()) {
        oss << "Line Numbers (" << lineNumbers.size() << "):\n";
        std::vector<int> sortedLines;
        for (const auto& pair : lineNumbers) {
            sortedLines.push_back(pair.first);
        }
        std::sort(sortedLines.begin(), sortedLines.end());
        for (int line : sortedLines) {
            const auto& sym = lineNumbers.at(line);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Labels
    if (!labels.empty()) {
        oss << "Labels (" << labels.size() << "):\n";
        std::vector<std::string> sortedLabels;
        for (const auto& pair : labels) {
            sortedLabels.push_back(pair.first);
        }
        std::sort(sortedLabels.begin(), sortedLabels.end());
        for (const auto& name : sortedLabels) {
            const auto& sym = labels.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Variables
    if (!variables.empty()) {
        oss << "Variables (" << variables.size() << "):\n";
        std::vector<std::string> sortedVars;
        for (const auto& pair : variables) {
            sortedVars.push_back(pair.first);
        }
        std::sort(sortedVars.begin(), sortedVars.end());
        for (const auto& name : sortedVars) {
            const auto& sym = variables.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Arrays
    if (!arrays.empty()) {
        oss << "Arrays (" << arrays.size() << "):\n";
        std::vector<std::string> sortedArrays;
        for (const auto& pair : arrays) {
            sortedArrays.push_back(pair.first);
        }
        std::sort(sortedArrays.begin(), sortedArrays.end());
        for (const auto& name : sortedArrays) {
            const auto& sym = arrays.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Functions
    if (!functions.empty()) {
        oss << "Functions (" << functions.size() << "):\n";
        std::vector<std::string> sortedFuncs;
        for (const auto& pair : functions) {
            sortedFuncs.push_back(pair.first);
        }
        std::sort(sortedFuncs.begin(), sortedFuncs.end());
        for (const auto& name : sortedFuncs) {
            const auto& sym = functions.at(name);
            oss << "  " << sym.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Data segment
    if (!dataSegment.values.empty()) {
        oss << "Data Segment:\n";
        oss << "  " << dataSegment.toString() << "\n";
        oss << "  Values: ";
        for (size_t i = 0; i < std::min(dataSegment.values.size(), size_t(10)); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << dataSegment.values[i] << "\"";
        }
        if (dataSegment.values.size() > 10) {
            oss << ", ... (" << (dataSegment.values.size() - 10) << " more)";
        }
        oss << "\n\n";
    }
    
    oss << "=== END SYMBOL TABLE ===\n";
    
    return oss.str();
}

// =============================================================================
// Constructor/Destructor
// =============================================================================

SemanticAnalyzer::SemanticAnalyzer()
    : m_strictMode(false)
    , m_warnUnused(true)
    , m_requireExplicitDim(true)
    , m_cancellableLoops(true)
    , m_program(nullptr)
    , m_currentLineNumber(0)
{
    initializeBuiltinFunctions();
    
    // Load additional functions from the global command registry
    loadFromCommandRegistry(ModularCommands::getGlobalCommandRegistry());
    
    m_constantsManager.addPredefinedConstants();
    
    // Register voice waveform constants (WAVE_SINE, WAVE_SQUARE, etc.)
#ifdef FBRUNNER3_BUILD
    FBRunner3::VoiceRegistration::registerVoiceConstants(m_constantsManager);
#endif
    
    // Register ALL predefined constants from ConstantsManager into symbol table
    // This allows them to be resolved like user-defined constants during compilation
    // Dynamically loads all constants - no hardcoded list needed!
    std::vector<std::string> predefinedNames = m_constantsManager.getAllConstantNames();
    
    for (const auto& name : predefinedNames) {
        int index = m_constantsManager.getConstantIndex(name);
        if (index >= 0) {
            ConstantValue val = m_constantsManager.getConstant(index);
            ConstantSymbol sym;
            if (std::holds_alternative<int64_t>(val)) {
                sym = ConstantSymbol(std::get<int64_t>(val));
            } else if (std::holds_alternative<double>(val)) {
                sym = ConstantSymbol(std::get<double>(val));
            } else if (std::holds_alternative<std::string>(val)) {
                sym = ConstantSymbol(std::get<std::string>(val));
            }
            sym.index = index;
            m_symbolTable.constants[name] = sym;
        }
    }
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

// =============================================================================
// Runtime Constant Injection
// =============================================================================

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, int64_t value) {
    // Add to ConstantsManager and get index
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[name] = sym;
}

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, double value) {
    // Add to ConstantsManager and get index
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[name] = sym;
}

void SemanticAnalyzer::injectRuntimeConstant(const std::string& name, const std::string& value) {
    // Add to ConstantsManager and get index
    int index = m_constantsManager.addConstant(name, value);
    
    // Create symbol and add to symbol table
    ConstantSymbol sym(value);
    sym.index = index;
    m_symbolTable.constants[name] = sym;
}

// =============================================================================
// DATA Label Registration
// =============================================================================

void SemanticAnalyzer::registerDataLabels(const std::map<std::string, int>& dataLabels) {
    // Register labels from DATA preprocessing so RESTORE can find them
    for (const auto& [labelName, lineNumber] : dataLabels) {
        // Create a label symbol for this DATA label
        LabelSymbol sym;
        sym.name = labelName;
        sym.labelId = m_symbolTable.nextLabelId++;
        sym.programLineIndex = 0; // DATA labels don't have a program line index
        sym.definition.line = lineNumber;
        sym.definition.column = 0;
        
        m_symbolTable.labels[labelName] = sym;
    }
}

// =============================================================================
// Main Analysis Entry Point
// =============================================================================

bool SemanticAnalyzer::analyze(Program& program, const CompilerOptions& options) {
    m_program = &program;
    m_errors.clear();
    m_warnings.clear();
    
    // Preserve predefined constants before resetting symbol table
    auto savedConstants = m_symbolTable.constants;
    
    m_symbolTable = SymbolTable();
    
    // Restore predefined constants
    m_symbolTable.constants = savedConstants;
    
    // Apply compiler options to symbol table
    m_symbolTable.arrayBase = options.arrayBase;
    m_symbolTable.unicodeMode = options.unicodeMode;
    m_symbolTable.errorTracking = options.errorTracking;
    m_symbolTable.cancellableLoops = options.cancellableLoops;
    m_cancellableLoops = options.cancellableLoops;
    
    // Clear control flow stacks
    while (!m_forStack.empty()) m_forStack.pop();
    while (!m_whileStack.empty()) m_whileStack.pop();
    while (!m_repeatStack.empty()) m_repeatStack.pop();
    
    // Two-pass analysis
    pass1_collectDeclarations(program);
    pass2_validate(program);
    
    // Final validation
    validateControlFlow(program);
    
    if (m_warnUnused) {
        checkUnusedVariables();
    }
    
    return m_errors.empty();
}

// =============================================================================
// Pass 1: Declaration Collection
// =============================================================================

void SemanticAnalyzer::pass1_collectDeclarations(Program& program) {
    collectLineNumbers(program);
    collectLabels(program);
    // NOTE: collectOptionStatements removed - options are now collected by parser
    collectDimStatements(program);
    collectDefStatements(program);
    collectFunctionAndSubStatements(program);
    collectDataStatements(program);
    collectConstantStatements(program);
}

void SemanticAnalyzer::collectLineNumbers(Program& program) {
    for (size_t i = 0; i < program.lines.size(); ++i) {
        const auto& line = program.lines[i];
        if (line->lineNumber > 0) {
            // Check for duplicate line numbers
            if (m_symbolTable.lineNumbers.find(line->lineNumber) != m_symbolTable.lineNumbers.end()) {
                error(SemanticErrorType::DUPLICATE_LINE_NUMBER,
                      "Duplicate line number: " + std::to_string(line->lineNumber),
                      line->location);
                continue;
            }
            
            LineNumberSymbol sym;
            sym.lineNumber = line->lineNumber;
            sym.programLineIndex = i;
            m_symbolTable.lineNumbers[line->lineNumber] = sym;
        }
    }
}

void SemanticAnalyzer::collectLabels(Program& program) {
    for (size_t i = 0; i < program.lines.size(); ++i) {
        const auto& line = program.lines[i];
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_LABEL) {
                const auto& labelStmt = static_cast<const LabelStatement&>(*stmt);
                declareLabel(labelStmt.labelName, i, stmt->location);
            }
        }
    }
}

void SemanticAnalyzer::collectOptionStatements(Program& program) {
    // NOTE: This function is now deprecated. OPTION statements are collected
    // by the parser before AST generation and passed as CompilerOptions.
    // This function is kept for backward compatibility but does nothing.
    // OPTION statements should not appear in the AST anymore.
}

void SemanticAnalyzer::collectDimStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_DIM) {
                processDimStatement(static_cast<const DimStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectDefStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_DEF) {
                processDefStatement(static_cast<const DefStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectConstantStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_CONSTANT) {
                processConstantStatement(static_cast<const ConstantStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::collectFunctionAndSubStatements(Program& program) {
    for (const auto& line : program.lines) {
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_FUNCTION) {
                processFunctionStatement(static_cast<const FunctionStatement&>(*stmt));
            } else if (stmt->getType() == ASTNodeType::STMT_SUB) {
                processSubStatement(static_cast<const SubStatement&>(*stmt));
            }
        }
    }
}

void SemanticAnalyzer::processFunctionStatement(const FunctionStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.functionName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Function " + stmt.functionName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.functionName;
    sym.parameters = stmt.parameters;
    sym.returnType = inferTypeFromSuffix(stmt.returnTypeSuffix);
    
    m_symbolTable.functions[stmt.functionName] = sym;
}

void SemanticAnalyzer::processSubStatement(const SubStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.subName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Subroutine " + stmt.subName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.subName;
    sym.parameters = stmt.parameters;
    sym.returnType = VariableType::VOID;
    
    m_symbolTable.functions[stmt.subName] = sym;
}

void SemanticAnalyzer::collectDataStatements(Program& program) {
    // Early pass - collect ONLY DATA statements
    // Track both line numbers and labels that appear on DATA lines
    // Also track labels on preceding lines (label followed by DATA on next line)
    
    std::string pendingLabel;  // Label from previous line waiting for DATA
    
    for (const auto& line : program.lines) {
        int lineNumber = line->lineNumber;
        std::string dataLabel;  // Label on this line (if any)
        bool hasData = false;
        bool hasLabel = false;
        
        // First pass: check if this line has DATA and/or collect any label
        for (const auto& stmt : line->statements) {
            if (stmt->getType() == ASTNodeType::STMT_LABEL) {
                // Found a label on this line
                const auto* labelStmt = static_cast<const LabelStatement*>(stmt.get());
                dataLabel = labelStmt->labelName;
                hasLabel = true;
                // DEBUG
                fprintf(stderr, "[collectDataStatements] Found label '%s' on line %d\n", 
                       dataLabel.c_str(), lineNumber);
            } else if (stmt->getType() == ASTNodeType::STMT_DATA) {
                hasData = true;
                // DEBUG
                fprintf(stderr, "[collectDataStatements] Found DATA on line %d\n", lineNumber);
            }
        }
        
        // Second pass: if this line has DATA, process it with label info
        if (hasData) {
            // Use label from current line, or pending label from previous line
            std::string effectiveLabel = dataLabel.empty() ? pendingLabel : dataLabel;
            
            // DEBUG
            fprintf(stderr, "[collectDataStatements] Processing DATA on line %d with label '%s'\n", 
                   lineNumber, effectiveLabel.c_str());
            
            for (const auto& stmt : line->statements) {
                if (stmt->getType() == ASTNodeType::STMT_DATA) {
                    processDataStatement(static_cast<const DataStatement&>(*stmt), 
                                       lineNumber, effectiveLabel);
                }
            }
            
            // Clear pending label after using it
            pendingLabel.clear();
        } else if (hasLabel) {
            // Label without DATA on this line - save it for next DATA line
            pendingLabel = dataLabel;
        } else {
            // Line with neither label nor DATA - clear pending label
            pendingLabel.clear();
        }
    }
}

void SemanticAnalyzer::processDimStatement(const DimStatement& stmt) {
    for (const auto& arrayDim : stmt.arrays) {
        // Check if already declared
        if (m_symbolTable.arrays.find(arrayDim.name) != m_symbolTable.arrays.end()) {
            error(SemanticErrorType::ARRAY_REDECLARED,
                  "Array '" + arrayDim.name + "' already declared",
                  stmt.location);
            continue;
        }
        
        // Calculate dimensions
        std::vector<int> dimensions;
        int totalSize = 1;
        for (const auto& dimExpr : arrayDim.dimensions) {
            // For now, we only support constant dimension sizes
            // In a full implementation, we'd evaluate constant expressions
            if (dimExpr->getType() == ASTNodeType::EXPR_NUMBER) {
                const auto& numExpr = static_cast<const NumberExpression&>(*dimExpr);
                int size = static_cast<int>(numExpr.value);
                if (size <= 0) {
                    error(SemanticErrorType::INVALID_ARRAY_INDEX,
                          "Array dimension must be positive",
                          stmt.location);
                    size = 1;
                }
                // BASIC arrays: DIM A(N) creates array with indices 0 to N (inclusive)
                // Store N+1 as the dimension size to allow N+1 elements
                dimensions.push_back(size + 1);
                totalSize *= (size + 1);
            } else {
                // Non-constant dimension (not supported in most BASIC dialects)
                // Default to 10, which allows indices 0-10 (11 elements)
                dimensions.push_back(11);  // Default: 10+1
                totalSize *= 11;
                warning("Non-constant array dimension; assuming 10", stmt.location);
            }
        }
        
        ArraySymbol sym;
        sym.name = arrayDim.name;
        sym.type = inferTypeFromSuffix(arrayDim.typeSuffix);
        if (sym.type == VariableType::UNKNOWN) {
            sym.type = inferTypeFromName(arrayDim.name);
        }
        sym.dimensions = dimensions;
        sym.isDeclared = true;
        sym.declaration = stmt.location;
        sym.totalSize = totalSize;
        
        m_symbolTable.arrays[arrayDim.name] = sym;
    }
}

void SemanticAnalyzer::processDefStatement(const DefStatement& stmt) {
    // Check if already declared
    if (m_symbolTable.functions.find(stmt.functionName) != m_symbolTable.functions.end()) {
        error(SemanticErrorType::FUNCTION_REDECLARED,
              "Function FN" + stmt.functionName + " already declared",
              stmt.location);
        return;
    }
    
    FunctionSymbol sym;
    sym.name = stmt.functionName;
    sym.parameters = stmt.parameters;
    sym.body = stmt.body.get();
    sym.definition = stmt.location;
    
    // Infer return type from function name
    sym.returnType = inferTypeFromName(stmt.functionName);
    
    m_symbolTable.functions[stmt.functionName] = sym;
}

void SemanticAnalyzer::processConstantStatement(const ConstantStatement& stmt) {
    // Check if constant already declared
    if (m_symbolTable.constants.find(stmt.name) != m_symbolTable.constants.end()) {
        error(SemanticErrorType::DUPLICATE_LABEL,  // Reusing error type for constants
              "Constant " + stmt.name + " already declared",
              stmt.location);
        return;
    }
    
    // Evaluate constant expression at compile time (supports full expressions now)
    FasterBASIC::ConstantValue evalResult = evaluateConstantExpression(*stmt.value);
    
    // Convert ConstantValue to ConstantSymbol
    ConstantSymbol constValue;
    if (std::holds_alternative<int64_t>(evalResult)) {
        constValue = ConstantSymbol(std::get<int64_t>(evalResult));
    } else if (std::holds_alternative<double>(evalResult)) {
        constValue = ConstantSymbol(std::get<double>(evalResult));
    } else if (std::holds_alternative<std::string>(evalResult)) {
        constValue = ConstantSymbol(std::get<std::string>(evalResult));
    }
    
    // Add to C++ ConstantsManager and get index
    int index = -1;
    if (std::holds_alternative<int64_t>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<int64_t>(evalResult));
    } else if (std::holds_alternative<double>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<double>(evalResult));
    } else if (std::holds_alternative<std::string>(evalResult)) {
        index = m_constantsManager.addConstant(stmt.name, std::get<std::string>(evalResult));
    }
    
    constValue.index = index;
    m_symbolTable.constants[stmt.name] = constValue;
}

void SemanticAnalyzer::processDataStatement(const DataStatement& stmt, int lineNumber,
                                            const std::string& dataLabel) {
    // Get current index (where this DATA starts)
    size_t currentIndex = m_symbolTable.dataSegment.values.size();
    
    // Record restore point by line number (if present)
    if (lineNumber > 0) {
        m_symbolTable.dataSegment.restorePoints[lineNumber] = currentIndex;
        // DEBUG
        fprintf(stderr, "[processDataStatement] Recorded line %d -> index %zu\n", 
               lineNumber, currentIndex);
    }
    
    // Record restore point by label (if present on this DATA line)
    if (!dataLabel.empty()) {
        m_symbolTable.dataSegment.labelRestorePoints[dataLabel] = currentIndex;
        // DEBUG
        fprintf(stderr, "[processDataStatement] Recorded label '%s' -> index %zu\n", 
               dataLabel.c_str(), currentIndex);
    }
    
    // Add values to data segment
    for (const auto& value : stmt.values) {
        m_symbolTable.dataSegment.values.push_back(value);
    }
}

// =============================================================================
// Pass 2: Validation
// =============================================================================

void SemanticAnalyzer::pass2_validate(Program& program) {
    for (const auto& line : program.lines) {
        validateProgramLine(*line);
    }
}

void SemanticAnalyzer::validateProgramLine(const ProgramLine& line) {
    m_currentLineNumber = line.lineNumber;
    
    for (const auto& stmt : line.statements) {
        validateStatement(*stmt);
    }
}

void SemanticAnalyzer::validateStatement(const Statement& stmt) {
    switch (stmt.getType()) {
        case ASTNodeType::STMT_PRINT:
            validatePrintStatement(static_cast<const PrintStatement&>(stmt));
            break;
        case ASTNodeType::STMT_CONSOLE:
            validateConsoleStatement(static_cast<const ConsoleStatement&>(stmt));
            break;
        case ASTNodeType::STMT_INPUT:
            validateInputStatement(static_cast<const InputStatement&>(stmt));
            break;
        case ASTNodeType::STMT_LET:
            validateLetStatement(static_cast<const LetStatement&>(stmt));
            break;
        case ASTNodeType::STMT_GOTO:
            validateGotoStatement(static_cast<const GotoStatement&>(stmt));
            break;
        case ASTNodeType::STMT_GOSUB:
            validateGosubStatement(static_cast<const GosubStatement&>(stmt));
            break;
        case ASTNodeType::STMT_IF:
            validateIfStatement(static_cast<const IfStatement&>(stmt));
            break;
        case ASTNodeType::STMT_FOR:
            validateForStatement(static_cast<const ForStatement&>(stmt));
            break;
        case ASTNodeType::STMT_FOR_IN:
            validateForInStatement(static_cast<const ForInStatement&>(stmt));
            break;
        case ASTNodeType::STMT_NEXT:
            validateNextStatement(static_cast<const NextStatement&>(stmt));
            break;
        case ASTNodeType::STMT_WHILE:
            validateWhileStatement(static_cast<const WhileStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_WEND:
            validateWendStatement(static_cast<const WendStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_REPEAT:
            validateRepeatStatement(static_cast<const RepeatStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_UNTIL:
            validateUntilStatement(static_cast<const UntilStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_DO:
            validateDoStatement(static_cast<const DoStatement&>(stmt));
            break;
        
        case ASTNodeType::STMT_LOOP:
            validateLoopStatement(static_cast<const LoopStatement&>(stmt));
            break;
        case ASTNodeType::STMT_READ:
            validateReadStatement(static_cast<const ReadStatement&>(stmt));
            break;
        case ASTNodeType::STMT_RESTORE:
            validateRestoreStatement(static_cast<const RestoreStatement&>(stmt));
            break;
        case ASTNodeType::STMT_ON_EVENT:
            validateOnEventStatement(static_cast<const OnEventStatement&>(stmt));
            break;
        case ASTNodeType::STMT_COLOR:
        case ASTNodeType::STMT_WAIT:
        case ASTNodeType::STMT_WAIT_MS:
        case ASTNodeType::STMT_PSET:
        case ASTNodeType::STMT_LINE:
        case ASTNodeType::STMT_RECT:
        case ASTNodeType::STMT_CIRCLE:
        case ASTNodeType::STMT_CIRCLEF:
            validateExpressionStatement(static_cast<const ExpressionStatement&>(stmt));
            break;
        default:
            // Other statements don't need special validation
            break;
    }
}

void SemanticAnalyzer::validatePrintStatement(const PrintStatement& stmt) {
    for (const auto& item : stmt.items) {
        validateExpression(*item.expr);
    }
}

void SemanticAnalyzer::validateConsoleStatement(const ConsoleStatement& stmt) {
    for (const auto& item : stmt.items) {
        validateExpression(*item.expr);
    }
}

void SemanticAnalyzer::validateInputStatement(const InputStatement& stmt) {
    for (const auto& varName : stmt.variables) {
        useVariable(varName, stmt.location);
    }
}

void SemanticAnalyzer::validateLetStatement(const LetStatement& stmt) {
    // Validate array indices if present
    for (const auto& index : stmt.indices) {
        validateExpression(*index);
        VariableType indexType = inferExpressionType(*index);
        if (!isNumericType(indexType)) {
            error(SemanticErrorType::INVALID_ARRAY_INDEX,
                  "Array index must be numeric",
                  stmt.location);
        }
    }
    
    // Check if array assignment
    if (!stmt.indices.empty()) {
        useArray(stmt.variable, stmt.indices.size(), stmt.location);
    } else {
        useVariable(stmt.variable, stmt.location);
    }
    
    // Validate value expression
    validateExpression(*stmt.value);
    
    // Type check
    VariableType targetType;
    if (!stmt.indices.empty()) {
        auto* arraySym = lookupArray(stmt.variable);
        targetType = arraySym ? arraySym->type : VariableType::UNKNOWN;
    } else {
        auto* varSym = lookupVariable(stmt.variable);
        targetType = varSym ? varSym->type : VariableType::UNKNOWN;
    }
    
    VariableType valueType = inferExpressionType(*stmt.value);
    checkTypeCompatibility(targetType, valueType, stmt.location, "assignment");
}

void SemanticAnalyzer::validateGotoStatement(const GotoStatement& stmt) {
    if (stmt.isLabel) {
        // Symbolic label - resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (!labelSym) {
            error(SemanticErrorType::UNDEFINED_LABEL,
                  "GOTO target label :" + stmt.label + " does not exist",
                  stmt.location);
        } else {
            labelSym->references.push_back(stmt.location);
        }
    } else {
        // Line number
        auto* lineSym = lookupLine(stmt.lineNumber);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "GOTO target line " + std::to_string(stmt.lineNumber) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    }
}

void SemanticAnalyzer::validateGosubStatement(const GosubStatement& stmt) {
    if (stmt.isLabel) {
        // Symbolic label - resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (!labelSym) {
            error(SemanticErrorType::UNDEFINED_LABEL,
                  "GOSUB target label :" + stmt.label + " does not exist",
                  stmt.location);
        } else {
            labelSym->references.push_back(stmt.location);
        }
    } else {
        // Line number
        auto* lineSym = lookupLine(stmt.lineNumber);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "GOSUB target line " + std::to_string(stmt.lineNumber) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    }
}

void SemanticAnalyzer::validateIfStatement(const IfStatement& stmt) {
    validateExpression(*stmt.condition);
    
    if (stmt.hasGoto) {
        auto* lineSym = lookupLine(stmt.gotoLine);
        if (!lineSym) {
            error(SemanticErrorType::UNDEFINED_LINE,
                  "IF THEN target line " + std::to_string(stmt.gotoLine) + " does not exist",
                  stmt.location);
        } else {
            lineSym->references.push_back(stmt.location);
        }
    } else {
        for (const auto& thenStmt : stmt.thenStatements) {
            validateStatement(*thenStmt);
        }
    }
    
    for (const auto& elseStmt : stmt.elseStatements) {
        validateStatement(*elseStmt);
    }
}

void SemanticAnalyzer::validateForStatement(const ForStatement& stmt) {
    // Declare/use loop variable
    useVariable(stmt.variable, stmt.location);
    
    // Validate expressions
    validateExpression(*stmt.start);
    validateExpression(*stmt.end);
    if (stmt.step) {
        validateExpression(*stmt.step);
    }
    
    // Type check
    VariableType varType = inferTypeFromName(stmt.variable);
    VariableType startType = inferExpressionType(*stmt.start);
    VariableType endType = inferExpressionType(*stmt.end);
    
    if (!isNumericType(startType) || !isNumericType(endType)) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "FOR loop bounds must be numeric",
              stmt.location);
    }
    
    // Push to control flow stack
    ForContext ctx;
    ctx.variable = stmt.variable;
    ctx.location = stmt.location;
    m_forStack.push(ctx);
}

void SemanticAnalyzer::validateForInStatement(const ForInStatement& stmt) {
    // Declare/use loop variable
    useVariable(stmt.variable, stmt.location);
    
    // Declare/use optional index variable
    if (!stmt.indexVariable.empty()) {
        useVariable(stmt.indexVariable, stmt.location);
    }
    
    // Validate array expression
    validateExpression(*stmt.array);
    
    // Type check - array expression should be an array access
    VariableType arrayType = inferExpressionType(*stmt.array);
    
    // The array should be a valid array reference
    // For now, we'll allow any type but could add stricter checking later
    
    // Push to control flow stack (reuse ForContext for simplicity)
    ForContext ctx;
    ctx.variable = stmt.variable;
    ctx.location = stmt.location;
    m_forStack.push(ctx);
}

void SemanticAnalyzer::validateNextStatement(const NextStatement& stmt) {
    if (m_forStack.empty()) {
        error(SemanticErrorType::NEXT_WITHOUT_FOR,
              "NEXT without matching FOR",
              stmt.location);
    } else {
        const auto& forCtx = m_forStack.top();
        
        // Check variable match if specified
        if (!stmt.variable.empty() && stmt.variable != forCtx.variable) {
            error(SemanticErrorType::CONTROL_FLOW_MISMATCH,
                  "NEXT variable '" + stmt.variable + "' does not match FOR variable '" + 
                  forCtx.variable + "'",
                  stmt.location);
        }
        
        m_forStack.pop();
    }
}

void SemanticAnalyzer::validateWhileStatement(const WhileStatement& stmt) {
    validateExpression(*stmt.condition);
    m_whileStack.push(stmt.location);
}

void SemanticAnalyzer::validateWendStatement(const WendStatement& stmt) {
    if (m_whileStack.empty()) {
        error(SemanticErrorType::WEND_WITHOUT_WHILE,
              "WEND without matching WHILE",
              stmt.location);
    } else {
        m_whileStack.pop();
    }
}

void SemanticAnalyzer::validateRepeatStatement(const RepeatStatement& stmt) {
    m_repeatStack.push(stmt.location);
}

void SemanticAnalyzer::validateUntilStatement(const UntilStatement& stmt) {
    if (m_repeatStack.empty()) {
        error(SemanticErrorType::UNTIL_WITHOUT_REPEAT,
              "UNTIL without matching REPEAT",
              stmt.location);
    } else {
        m_repeatStack.pop();
    }
    
    validateExpression(*stmt.condition);
}

void SemanticAnalyzer::validateDoStatement(const DoStatement& stmt) {
    // Validate condition if present (DO WHILE or DO UNTIL)
    if (stmt.condition) {
        validateExpression(*stmt.condition);
    }
    m_doStack.push(stmt.location);
}

void SemanticAnalyzer::validateLoopStatement(const LoopStatement& stmt) {
    if (m_doStack.empty()) {
        error(SemanticErrorType::LOOP_WITHOUT_DO,
              "LOOP without matching DO",
              stmt.location);
    } else {
        m_doStack.pop();
    }
    
    // Validate condition if present (LOOP WHILE or LOOP UNTIL)
    if (stmt.condition) {
        validateExpression(*stmt.condition);
    }
}

void SemanticAnalyzer::validateReadStatement(const ReadStatement& stmt) {
    for (const auto& varName : stmt.variables) {
        useVariable(varName, stmt.location);
    }
}

void SemanticAnalyzer::validateRestoreStatement(const RestoreStatement& stmt) {
    // RESTORE targets can be:
    // 1. Regular labels/lines in the program (checked here)
    // 2. DATA labels/lines (handled by DataManager at runtime)
    // So we don't error if not found - just record the reference if it exists
    
    if (stmt.isLabel) {
        // Symbolic label - try to resolve it
        auto* labelSym = lookupLabel(stmt.label);
        if (labelSym) {
            // Found in symbol table - record reference
            labelSym->references.push_back(stmt.location);
        }
        // If not found, assume it's a DATA label - will be resolved at runtime
    } else if (stmt.lineNumber > 0) {
        auto* lineSym = lookupLine(stmt.lineNumber);
        // If not found, assume it's a DATA line - will be resolved at runtime
        // No error needed - DataManager will handle it
    }
}

void SemanticAnalyzer::validateExpressionStatement(const ExpressionStatement& stmt) {
    for (const auto& arg : stmt.arguments) {
        validateExpression(*arg);
    }
}

void SemanticAnalyzer::validateOnEventStatement(const OnEventStatement& stmt) {
    // Mark that this program uses events
    m_symbolTable.eventsUsed = true;
    
    // Validate event name (check against known events from events.h)
    if (!FasterBASIC::isValidEventName(stmt.eventName)) {
        error(SemanticErrorType::UNDEFINED_VARIABLE, 
              "Unknown event name: " + stmt.eventName, 
              stmt.location);
        return;
    }
    
    // Validate target based on handler type
    switch (stmt.handlerType) {
        case EventHandlerType::CALL:
            // Target should be a function name
            if (m_symbolTable.functions.find(stmt.target) == m_symbolTable.functions.end()) {
                // Function not found - could be forward reference, so just issue warning
                warning("Function '" + stmt.target + "' not found for event handler. "
                       "Ensure function is defined before program runs.", stmt.location);
            }
            break;
            
        case EventHandlerType::GOTO:
        case EventHandlerType::GOSUB:
            // Target should be a line number or label
            if (stmt.isLineNumber) {
                try {
                    int lineNum = std::stoi(stmt.target);
                    if (m_symbolTable.lineNumbers.find(lineNum) == m_symbolTable.lineNumbers.end()) {
                        error(SemanticErrorType::UNDEFINED_LINE,
                              "Line number " + stmt.target + " not found for event handler",
                              stmt.location);
                    }
                } catch (const std::exception&) {
                    error(SemanticErrorType::UNDEFINED_LINE,
                          "Invalid line number: " + stmt.target,
                          stmt.location);
                }
            } else {
                // It's a label
                if (m_symbolTable.labels.find(stmt.target) == m_symbolTable.labels.end()) {
                    error(SemanticErrorType::UNDEFINED_LABEL,
                          "Label '" + stmt.target + "' not found for event handler",
                          stmt.location);
                }
            }
            break;
    }
}

// =============================================================================
// Expression Validation and Type Inference
// =============================================================================

void SemanticAnalyzer::validateExpression(const Expression& expr) {
    // This also performs type inference as a side effect
    inferExpressionType(expr);
}

VariableType SemanticAnalyzer::inferExpressionType(const Expression& expr) {
    switch (expr.getType()) {
        case ASTNodeType::EXPR_NUMBER:
            return VariableType::FLOAT;
        
        case ASTNodeType::EXPR_STRING:
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        
        case ASTNodeType::EXPR_VARIABLE:
            return inferVariableType(static_cast<const VariableExpression&>(expr));
        
        case ASTNodeType::EXPR_ARRAY_ACCESS:
            return inferArrayAccessType(static_cast<const ArrayAccessExpression&>(expr));
        
        case ASTNodeType::EXPR_FUNCTION_CALL:
            // Check if this is actually a RegistryFunctionExpression
            if (auto* regFunc = dynamic_cast<const RegistryFunctionExpression*>(&expr)) {
                return inferRegistryFunctionType(*regFunc);
            } else {
                return inferFunctionCallType(static_cast<const FunctionCallExpression&>(expr));
            }
        
        case ASTNodeType::EXPR_BINARY:
            return inferBinaryExpressionType(static_cast<const BinaryExpression&>(expr));
        
        case ASTNodeType::EXPR_UNARY:
            return inferUnaryExpressionType(static_cast<const UnaryExpression&>(expr));
        
        default:
            return VariableType::UNKNOWN;
    }
}

VariableType SemanticAnalyzer::inferBinaryExpressionType(const BinaryExpression& expr) {
    VariableType leftType = inferExpressionType(*expr.left);
    VariableType rightType = inferExpressionType(*expr.right);
    
    // String concatenation
    if (leftType == VariableType::STRING || rightType == VariableType::STRING ||
        leftType == VariableType::UNICODE || rightType == VariableType::UNICODE) {
        if (expr.op == TokenType::PLUS) {
            // If either is UNICODE, result is UNICODE
            if (leftType == VariableType::UNICODE || rightType == VariableType::UNICODE) {
                return VariableType::UNICODE;
            }
            return VariableType::STRING;
        }
    }
    
    // Comparison operators return numeric
    if (expr.op >= TokenType::EQUAL && expr.op <= TokenType::GREATER_EQUAL) {
        return VariableType::FLOAT;
    }
    
    // Logical operators return numeric
    if (expr.op == TokenType::AND || expr.op == TokenType::OR) {
        return VariableType::FLOAT;
    }
    
    // Arithmetic operators
    return promoteTypes(leftType, rightType);
}

VariableType SemanticAnalyzer::inferUnaryExpressionType(const UnaryExpression& expr) {
    VariableType exprType = inferExpressionType(*expr.expr);
    
    if (expr.op == TokenType::NOT) {
        return VariableType::FLOAT;
    }
    
    // Unary + or -
    return exprType;
}

VariableType SemanticAnalyzer::inferVariableType(const VariableExpression& expr) {
    useVariable(expr.name, expr.location);
    
    auto* sym = lookupVariable(expr.name);
    if (sym) {
        return sym->type;
    }
    
    return VariableType::UNKNOWN;
}

VariableType SemanticAnalyzer::inferArrayAccessType(const ArrayAccessExpression& expr) {
    // Check if this is a function/sub call first
    if (m_symbolTable.functions.find(expr.name) != m_symbolTable.functions.end()) {
        // It's a function or sub call - validate arguments but don't treat as array
        const auto& funcSym = m_symbolTable.functions.at(expr.name);
        for (const auto& arg : expr.indices) {
            validateExpression(*arg);
        }
        return funcSym.returnType;
    }
    
    // Check symbol table - if it's a declared array, treat as array access
    auto* arraySym = lookupArray(expr.name);
    if (arraySym) {
        // This is a declared array - validate as array access
        useArray(expr.name, expr.indices.size(), expr.location);
        
        // Validate indices
        for (const auto& index : expr.indices) {
            validateExpression(*index);
            VariableType indexType = inferExpressionType(*index);
            if (!isNumericType(indexType)) {
                error(SemanticErrorType::INVALID_ARRAY_INDEX,
                      "Array index must be numeric",
                      expr.location);
            }
        }
        
        return arraySym->type;
    }
    
    // Not a declared array - check if it's a built-in function call
    if (isBuiltinFunction(expr.name)) {
        // Validate argument count
        int expectedArgs = getBuiltinArgCount(expr.name);
        if (expectedArgs >= 0 && static_cast<int>(expr.indices.size()) != expectedArgs) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Built-in function " + expr.name + " expects " + 
                  std::to_string(expectedArgs) + " argument(s), got " + 
                  std::to_string(expr.indices.size()),
                  expr.location);
        }
        
        // Validate arguments
        for (const auto& index : expr.indices) {
            validateExpression(*index);
        }
        
        return getBuiltinReturnType(expr.name);
    }
    
    // Not an array and not a built-in function - treat as undeclared array
    // (useArray will create an implicit array symbol if needed)
    useArray(expr.name, expr.indices.size(), expr.location);
    
    // Validate indices for the implicit array
    for (const auto& index : expr.indices) {
        validateExpression(*index);
        VariableType indexType = inferExpressionType(*index);
        if (!isNumericType(indexType)) {
            error(SemanticErrorType::INVALID_ARRAY_INDEX,
                  "Array index must be numeric",
                  expr.location);
        }
    }
    
    // Return type for implicit array (lookup again after useArray)
    arraySym = lookupArray(expr.name);
    if (arraySym) {
        return arraySym->type;
    }
    return VariableType::UNKNOWN;
}

VariableType SemanticAnalyzer::inferFunctionCallType(const FunctionCallExpression& expr) {
    // Validate arguments
    for (const auto& arg : expr.arguments) {
        validateExpression(*arg);
    }
    
    if (expr.isFN) {
        // User-defined function
        auto* sym = lookupFunction(expr.name);
        if (sym) {
            return sym->returnType;
        } else {
            error(SemanticErrorType::UNDEFINED_FUNCTION,
                  "Undefined function FN" + expr.name,
                  expr.location);
            return VariableType::UNKNOWN;
        }
    } else {
        // Built-in function - most return FLOAT
        return VariableType::FLOAT;
    }
}

VariableType SemanticAnalyzer::inferRegistryFunctionType(const RegistryFunctionExpression& expr) {
    // Validate arguments
    for (const auto& arg : expr.arguments) {
        validateExpression(*arg);
    }
    
    // Convert ModularCommands::ReturnType to VariableType
    switch (expr.returnType) {
        case FasterBASIC::ModularCommands::ReturnType::INT:
            return VariableType::INT;
        case FasterBASIC::ModularCommands::ReturnType::FLOAT:
            return VariableType::FLOAT;
        case FasterBASIC::ModularCommands::ReturnType::STRING:
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        case FasterBASIC::ModularCommands::ReturnType::BOOL:
            return VariableType::INT; // BASIC treats booleans as integers
        case FasterBASIC::ModularCommands::ReturnType::VOID:
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Registry function " + expr.name + " has invalid return type",
                  expr.location);
            return VariableType::UNKNOWN;
    }
}

// =============================================================================
// Type Checking
// =============================================================================

void SemanticAnalyzer::checkTypeCompatibility(VariableType expected, VariableType actual,
                                              const SourceLocation& loc, const std::string& context) {
    if (expected == VariableType::UNKNOWN || actual == VariableType::UNKNOWN) {
        return;  // Can't check
    }
    
    // String to numeric or vice versa is an error
    bool expectedString = (expected == VariableType::STRING || expected == VariableType::UNICODE);
    bool actualString = (actual == VariableType::STRING || actual == VariableType::UNICODE);
    
    if (expectedString != actualString) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "Type mismatch in " + context + ": cannot assign " +
              std::string(typeToString(actual)) + " to " + std::string(typeToString(expected)),
              loc);
    }
}

VariableType SemanticAnalyzer::promoteTypes(VariableType left, VariableType right) {
    // String/Unicode takes precedence
    if (left == VariableType::UNICODE || right == VariableType::UNICODE) {
        return VariableType::UNICODE;
    }
    if (left == VariableType::STRING || right == VariableType::STRING) {
        return VariableType::STRING;
    }
    
    // Numeric promotion
    if (left == VariableType::DOUBLE || right == VariableType::DOUBLE) {
        return VariableType::DOUBLE;
    }
    if (left == VariableType::FLOAT || right == VariableType::FLOAT) {
        return VariableType::FLOAT;
    }
    if (left == VariableType::INT || right == VariableType::INT) {
        return VariableType::INT;
    }
    
    return VariableType::FLOAT;
}

bool SemanticAnalyzer::isNumericType(VariableType type) {
    return type == VariableType::INT || 
           type == VariableType::FLOAT || 
           type == VariableType::DOUBLE;
}

// =============================================================================
// Symbol Table Management
// =============================================================================

VariableSymbol* SemanticAnalyzer::declareVariable(const std::string& name, VariableType type,
                                                  const SourceLocation& loc, bool isDeclared) {
    auto it = m_symbolTable.variables.find(name);
    if (it != m_symbolTable.variables.end()) {
        return &it->second;
    }
    
    VariableSymbol sym;
    sym.name = name;
    sym.type = type;
    sym.isDeclared = isDeclared;
    sym.isUsed = false;
    sym.firstUse = loc;
    
    m_symbolTable.variables[name] = sym;
    return &m_symbolTable.variables[name];
}

VariableSymbol* SemanticAnalyzer::lookupVariable(const std::string& name) {
    auto it = m_symbolTable.variables.find(name);
    if (it != m_symbolTable.variables.end()) {
        return &it->second;
    }
    
    // Also check arrays table - DIM x$ AS STRING creates a 0-dimensional array (scalar)
    // We need to treat it as a variable for assignment purposes
    auto arrIt = m_symbolTable.arrays.find(name);
    if (arrIt != m_symbolTable.arrays.end() && arrIt->second.dimensions.empty()) {
        // Found a scalar array - create a corresponding variable entry
        VariableSymbol sym;
        sym.name = name;
        sym.type = arrIt->second.type;
        sym.isDeclared = true;
        sym.firstUse = arrIt->second.declaration;
        m_symbolTable.variables[name] = sym;
        return &m_symbolTable.variables[name];
    }
    
    return nullptr;
}

ArraySymbol* SemanticAnalyzer::lookupArray(const std::string& name) {
    auto it = m_symbolTable.arrays.find(name);
    if (it != m_symbolTable.arrays.end()) {
        return &it->second;
    }
    return nullptr;
}

FunctionSymbol* SemanticAnalyzer::lookupFunction(const std::string& name) {
    auto it = m_symbolTable.functions.find(name);
    if (it != m_symbolTable.functions.end()) {
        return &it->second;
    }
    return nullptr;
}

LineNumberSymbol* SemanticAnalyzer::lookupLine(int lineNumber) {
    auto it = m_symbolTable.lineNumbers.find(lineNumber);
    if (it != m_symbolTable.lineNumbers.end()) {
        return &it->second;
    }
    return nullptr;
}

LabelSymbol* SemanticAnalyzer::declareLabel(const std::string& name, size_t programLineIndex,
                                            const SourceLocation& loc) {
    // Check for duplicate labels
    if (m_symbolTable.labels.find(name) != m_symbolTable.labels.end()) {
        error(SemanticErrorType::DUPLICATE_LABEL,
              "Label :" + name + " already defined",
              loc);
        return nullptr;
    }
    
    LabelSymbol sym;
    sym.name = name;
    sym.labelId = m_symbolTable.nextLabelId++;
    sym.programLineIndex = programLineIndex;
    sym.definition = loc;
    m_symbolTable.labels[name] = sym;
    
    return &m_symbolTable.labels[name];
}

LabelSymbol* SemanticAnalyzer::lookupLabel(const std::string& name) {
    auto it = m_symbolTable.labels.find(name);
    if (it != m_symbolTable.labels.end()) {
        return &it->second;
    }
    return nullptr;
}

int SemanticAnalyzer::resolveLabelToId(const std::string& name, const SourceLocation& loc) {
    auto* sym = lookupLabel(name);
    if (!sym) {
        error(SemanticErrorType::UNDEFINED_LABEL,
              "Undefined label: " + name,
              loc);
        return -1;  // Return invalid ID on error
    }
    
    // Track this reference
    sym->references.push_back(loc);
    return sym->labelId;
}

void SemanticAnalyzer::useVariable(const std::string& name, const SourceLocation& loc) {
    auto* sym = lookupVariable(name);
    if (!sym) {
        // Implicitly declare
        VariableType type = inferTypeFromName(name);
        sym = declareVariable(name, type, loc, false);
    }
    sym->isUsed = true;
}

void SemanticAnalyzer::useArray(const std::string& name, size_t dimensionCount, 
                                const SourceLocation& loc) {
    // Check if this is actually a function/sub call, not an array access
    if (m_symbolTable.functions.find(name) != m_symbolTable.functions.end()) {
        // It's a function or sub, not an array - skip array validation
        return;
    }
    
    auto* sym = lookupArray(name);
    if (!sym) {
        if (m_requireExplicitDim) {
            error(SemanticErrorType::ARRAY_NOT_DECLARED,
                  "Array '" + name + "' used without DIM declaration",
                  loc);
        }
        return;
    }
    
    // Check dimension count
    if (dimensionCount != sym->dimensions.size()) {
        error(SemanticErrorType::WRONG_DIMENSION_COUNT,
              "Array '" + name + "' expects " + std::to_string(sym->dimensions.size()) +
              " dimensions, got " + std::to_string(dimensionCount),
              loc);
    }
}

// =============================================================================
// Type Inference from Name/Suffix
// =============================================================================

VariableType SemanticAnalyzer::inferTypeFromSuffix(TokenType suffix) {
    switch (suffix) {
        case TokenType::TYPE_INT:    return VariableType::INT;
        case TokenType::TYPE_FLOAT:  return VariableType::FLOAT;
        case TokenType::TYPE_DOUBLE: return VariableType::DOUBLE;
        case TokenType::TYPE_STRING: 
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        default:                     return VariableType::UNKNOWN;
    }
}

VariableType SemanticAnalyzer::inferTypeFromName(const std::string& name) {
    if (name.empty()) return VariableType::FLOAT;
    
    // Check for normalized suffixes first (e.g., A_STRING, B_INT, C_DOUBLE)
    if (name.length() > 7 && name.substr(name.length() - 7) == "_STRING") {
        return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
    }
    if (name.length() > 4 && name.substr(name.length() - 4) == "_INT") {
        return VariableType::INT;
    }
    if (name.length() > 7 && name.substr(name.length() - 7) == "_DOUBLE") {
        return VariableType::DOUBLE;
    }
    
    // Check for original BASIC suffixes ($, %, !, #)
    char lastChar = name.back();
    switch (lastChar) {
        case '$': 
            // Return UNICODE type if in Unicode mode
            return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
        case '%': return VariableType::INT;
        case '!': return VariableType::FLOAT;
        case '#': return VariableType::DOUBLE;
        default:  return VariableType::FLOAT;  // Default in BASIC
    }
}

// =============================================================================
// Control Flow and Final Validation
// =============================================================================

void SemanticAnalyzer::validateControlFlow(Program& program) {
    // Check for unclosed loops
    if (!m_forStack.empty()) {
        const auto& ctx = m_forStack.top();
        error(SemanticErrorType::FOR_WITHOUT_NEXT,
              "FOR loop starting at " + ctx.location.toString() + " has no matching NEXT",
              ctx.location);
    }
    
    if (!m_whileStack.empty()) {
        const auto& loc = m_whileStack.top();
        error(SemanticErrorType::WHILE_WITHOUT_WEND,
              "WHILE loop starting at " + loc.toString() + " has no matching WEND",
              loc);
    }
    
    if (!m_repeatStack.empty()) {
        const auto& loc = m_repeatStack.top();
        error(SemanticErrorType::REPEAT_WITHOUT_UNTIL,
              "REPEAT loop starting at " + loc.toString() + " has no matching UNTIL",
              loc);
    }
}

void SemanticAnalyzer::checkUnusedVariables() {
    for (const auto& pair : m_symbolTable.variables) {
        const auto& sym = pair.second;
        if (!sym.isUsed && sym.isDeclared) {
            warning("Variable '" + sym.name + "' declared but never used", sym.firstUse);
        }
    }
}

// =============================================================================
// Error Reporting
// =============================================================================

void SemanticAnalyzer::error(SemanticErrorType type, const std::string& message,
                             const SourceLocation& loc) {
    m_errors.emplace_back(type, message, loc);
}

void SemanticAnalyzer::warning(const std::string& message, const SourceLocation& loc) {
    m_warnings.emplace_back(message, loc);
}

// =============================================================================
// Report Generation
// =============================================================================

std::string SemanticAnalyzer::generateReport() const {
    std::ostringstream oss;
    
    oss << "=== SEMANTIC ANALYSIS REPORT ===\n\n";
    
    // Summary
    oss << "Status: ";
    if (m_errors.empty()) {
        oss << "✓ PASSED\n";
    } else {
        oss << "✗ FAILED (" << m_errors.size() << " error(s))\n";
    }
    
    oss << "Errors: " << m_errors.size() << "\n";
    oss << "Warnings: " << m_warnings.size() << "\n";
    oss << "\n";
    
    // Symbol table summary
    oss << "Symbol Table Summary:\n";
    oss << "  Line Numbers: " << m_symbolTable.lineNumbers.size() << "\n";
    oss << "  Variables: " << m_symbolTable.variables.size() << "\n";
    oss << "  Arrays: " << m_symbolTable.arrays.size() << "\n";
    oss << "  Functions: " << m_symbolTable.functions.size() << "\n";
    oss << "  Data Values: " << m_symbolTable.dataSegment.values.size() << "\n";
    oss << "\n";
    
    // Errors
    if (!m_errors.empty()) {
        oss << "Errors:\n";
        for (const auto& err : m_errors) {
            oss << "  " << err.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Warnings
    if (!m_warnings.empty()) {
        oss << "Warnings:\n";
        for (const auto& warn : m_warnings) {
            oss << "  " << warn.toString() << "\n";
        }
        oss << "\n";
    }
    
    // Full symbol table
    oss << m_symbolTable.toString();
    
    oss << "=== END SEMANTIC ANALYSIS REPORT ===\n";
    
    return oss.str();
}

// =============================================================================
// Built-in Function Support
// =============================================================================

void SemanticAnalyzer::initializeBuiltinFunctions() {
    // Math functions (all take 1 argument, return FLOAT)
    m_builtinFunctions["ABS"] = 1;
    m_builtinFunctions["SIN"] = 1;
    m_builtinFunctions["COS"] = 1;
    m_builtinFunctions["TAN"] = 1;
    m_builtinFunctions["ATN"] = 1;
    m_builtinFunctions["SQR"] = 1;
    m_builtinFunctions["INT"] = 1;
    m_builtinFunctions["SGN"] = 1;
    m_builtinFunctions["LOG"] = 1;
    m_builtinFunctions["EXP"] = 1;
    
    // RND takes 0 or 1 argument
    m_builtinFunctions["RND"] = -1;  // -1 = variable arg count
    
    // TIMER takes 0 arguments
    m_builtinFunctions["TIMER"] = 0;
    
    // String functions
    m_builtinFunctions["LEN"] = 1;    // Returns INT
    m_builtinFunctions["ASC"] = 1;    // Returns INT
    m_builtinFunctions["CHR$"] = 1;   // Returns STRING
    m_builtinFunctions["STR$"] = 1;   // Returns STRING
    m_builtinFunctions["VAL"] = 1;    // Returns FLOAT
    m_builtinFunctions["LEFT$"] = 2;  // Returns STRING
    m_builtinFunctions["RIGHT$"] = 2; // Returns STRING
    m_builtinFunctions["MID$"] = 3;   // Returns STRING (string, start, length)
    m_builtinFunctions["INSTR"] = -1;  // Returns INT - 2 args: (haystack$, needle$) or 3 args: (start, haystack$, needle$)
    m_builtinFunctions["STRING$"] = 2; // Returns STRING (count, char$ or ascii) - repeat character
    m_builtinFunctions["SPACE$"] = 1; // Returns STRING (count) - generate spaces
    m_builtinFunctions["LCASE$"] = 1; // Returns STRING (lowercase)
    m_builtinFunctions["UCASE$"] = 1; // Returns STRING (uppercase)
    m_builtinFunctions["LTRIM$"] = 1; // Returns STRING (remove leading spaces)
    m_builtinFunctions["RTRIM$"] = 1; // Returns STRING (remove trailing spaces)
    m_builtinFunctions["TRIM$"] = 1;  // Returns STRING (remove leading and trailing spaces)
    m_builtinFunctions["REVERSE$"] = 1; // Returns STRING (reverse string)
    
    // File I/O functions
    m_builtinFunctions["EOF"] = 1;    // (file_number) Returns INT (bool)
    m_builtinFunctions["LOC"] = 1;    // (file_number) Returns INT (position)
    m_builtinFunctions["LOF"] = 1;    // (file_number) Returns INT (length)
    
    // =============================================================================
    // SuperTerminal API - Phase 1: Core Display & Frame Sync
    // =============================================================================
    
    // Text Layer
    m_builtinFunctions["TEXT_CLEAR"] = 0;           // void
    m_builtinFunctions["TEXT_CLEAR_REGION"] = 4;   // (x, y, w, h) void
    m_builtinFunctions["TEXT_PUT"] = 5;            // (x, y, text$, fg, bg) void
    m_builtinFunctions["TEXT_PUTCHAR"] = 5;        // (x, y, chr, fg, bg) void
    m_builtinFunctions["TEXT_SCROLL"] = 1;         // (lines) void
    m_builtinFunctions["TEXT_SET_SIZE"] = 2;       // (width, height) void
    m_builtinFunctions["TEXT_GET_WIDTH"] = 0;      // Returns INT
    m_builtinFunctions["TEXT_GET_HEIGHT"] = 0;     // Returns INT
    
    // Chunky Graphics Layer (palette index + background color)
    m_builtinFunctions["CHUNKY_CLEAR"] = 1;        // (bg_color) void
    m_builtinFunctions["CHUNKY_PSET"] = 4;         // (x, y, color_idx, bg) void
    m_builtinFunctions["CHUNKY_LINE"] = 6;         // (x1, y1, x2, y2, color_idx, bg) void
    m_builtinFunctions["CHUNKY_RECT"] = 6;         // (x, y, w, h, color_idx, bg) void
    m_builtinFunctions["CHUNKY_FILLRECT"] = 6;     // (x, y, w, h, color_idx, bg) void
    m_builtinFunctions["CHUNKY_HLINE"] = 5;        // (x, y, length, color_idx, bg) void
    m_builtinFunctions["CHUNKY_VLINE"] = 5;        // (x, y, length, color_idx, bg) void
    m_builtinFunctions["CHUNKY_GET_WIDTH"] = 0;    // Returns INT
    m_builtinFunctions["CHUNKY_GET_HEIGHT"] = 0;   // Returns INT
    
    // Smooth Graphics Layer (STColor + thickness for outlines)
    m_builtinFunctions["GFX_CLEAR"] = 0;           // void
    m_builtinFunctions["GFX_LINE"] = 6;            // (x1, y1, x2, y2, color, thickness) void
    m_builtinFunctions["GFX_RECT"] = 5;            // (x, y, w, h, color) void
    m_builtinFunctions["GFX_RECT_OUTLINE"] = 6;    // (x, y, w, h, color, thickness) void
    m_builtinFunctions["GFX_CIRCLE"] = 4;          // (x, y, radius, color) void
    m_builtinFunctions["GFX_CIRCLE_OUTLINE"] = 5;  // (x, y, radius, color, thickness) void
    m_builtinFunctions["GFX_POINT"] = 3;           // (x, y, color) void
    
    // Color Utilities
    m_builtinFunctions["COLOR_RGB"] = 3;           // (r, g, b) Returns INT
    m_builtinFunctions["COLOR_RGBA"] = 4;          // (r, g, b, a) Returns INT
    m_builtinFunctions["COLOR_HSV"] = 3;           // (h, s, v) Returns INT
    
    // Frame Synchronization & Timing
    m_builtinFunctions["FRAME_WAIT"] = 0;          // void
    m_builtinFunctions["FRAME_COUNT"] = 0;         // Returns INT
    m_builtinFunctions["TIME"] = 0;                // Returns FLOAT
    m_builtinFunctions["DELTA_TIME"] = 0;          // Returns FLOAT
    
    // Random Utilities
    m_builtinFunctions["RANDOM"] = 0;              // Returns FLOAT
    m_builtinFunctions["RANDOM_INT"] = 2;          // (min, max) Returns INT
    m_builtinFunctions["RANDOM_SEED"] = 1;         // (seed) void
    
    // =============================================================================
    // SuperTerminal API - Phase 2: Input & Sprites
    // =============================================================================
    
    // Keyboard Input
    m_builtinFunctions["KEY_PRESSED"] = 1;         // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_JUST_PRESSED"] = 1;    // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_JUST_RELEASED"] = 1;   // (keycode) Returns INT (bool)
    m_builtinFunctions["KEY_GET_CHAR"] = 0;        // Returns INT (char code)
    m_builtinFunctions["KEY_CLEAR_BUFFER"] = 0;    // void
    
    // Mouse Input
    m_builtinFunctions["MOUSE_X"] = 0;             // Returns INT (pixel x)
    m_builtinFunctions["MOUSE_Y"] = 0;             // Returns INT (pixel y)
    m_builtinFunctions["MOUSE_GRID_X"] = 0;        // Returns INT (grid column)
    m_builtinFunctions["MOUSE_GRID_Y"] = 0;        // Returns INT (grid row)
    m_builtinFunctions["MOUSE_BUTTON"] = 1;        // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_BUTTON_PRESSED"] = 1;    // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_BUTTON_RELEASED"] = 1;   // (button) Returns INT (bool)
    m_builtinFunctions["MOUSE_WHEEL_X"] = 0;       // Returns FLOAT (wheel delta x)
    m_builtinFunctions["MOUSE_WHEEL_Y"] = 0;       // Returns FLOAT (wheel delta y)
    
    // Sprites
    m_builtinFunctions["SPRITE_LOAD"] = 1;         // (filename$) Returns INT (sprite ID)
    m_builtinFunctions["SPRITE_LOAD_BUILTIN"] = 1; // (name$) Returns INT (sprite ID)
    m_builtinFunctions["DRAWINTOSPRITE"] = 2;      // (width, height) Returns INT (sprite ID)
    m_builtinFunctions["ENDDRAWINTOSPRITE"] = 0;   // void
    m_builtinFunctions["DRAWTOFILE"] = 3;          // (filename$, width, height) Returns BOOL
    m_builtinFunctions["ENDDRAWTOFILE"] = 0;       // Returns BOOL
    m_builtinFunctions["DRAWTOTILESET"] = 4;       // (tile_width, tile_height, columns, rows) Returns INT
    m_builtinFunctions["DRAWTILE"] = 1;            // (tile_index) Returns BOOL
    m_builtinFunctions["ENDDRAWTOTILESET"] = 0;    // Returns BOOL
    m_builtinFunctions["SPRITE_SHOW"] = 3;         // (id, x, y) void
    m_builtinFunctions["SPRITE_HIDE"] = 1;         // (id) void
    m_builtinFunctions["SPRITE_TRANSFORM"] = 6;    // (id, x, y, rot, sx, sy) void
    m_builtinFunctions["SPRITE_TINT"] = 2;         // (id, color) void
    m_builtinFunctions["SPRITE_UNLOAD"] = 1;       // (id) void
    
    // Layers
    m_builtinFunctions["LAYER_SET_VISIBLE"] = 2;   // (layer, visible) void
    m_builtinFunctions["LAYER_SET_ALPHA"] = 2;     // (layer, alpha) void
    m_builtinFunctions["LAYER_SET_ORDER"] = 2;     // (layer, order) void
    
    // Display queries
    m_builtinFunctions["DISPLAY_WIDTH"] = 0;       // Returns INT
    m_builtinFunctions["DISPLAY_HEIGHT"] = 0;      // Returns INT
    m_builtinFunctions["CELL_WIDTH"] = 0;          // Returns INT
    m_builtinFunctions["CELL_HEIGHT"] = 0;         // Returns INT
    
    // =============================================================================
    // SuperTerminal API - Phase 3: Audio
    // =============================================================================
    
    // Sound Effects
    m_builtinFunctions["SOUND_LOAD"] = 1;          // (filename$) Returns INT (sound ID)
    m_builtinFunctions["SOUND_LOAD_BUILTIN"] = 1;  // (name$) Returns INT (sound ID)
    m_builtinFunctions["SOUND_PLAY"] = 2;          // (id, volume) void
    m_builtinFunctions["SOUND_STOP"] = 1;          // (id) void
    m_builtinFunctions["SOUND_UNLOAD"] = 1;        // (id) void
    
    // Music and Audio - loaded from command registry
    
    // Synthesis
    m_builtinFunctions["SYNTH_NOTE"] = 3;          // (note, duration, volume) void
    m_builtinFunctions["SYNTH_FREQUENCY"] = 3;     // (freq, duration, volume) void
    m_builtinFunctions["SYNTH_SET_INSTRUMENT"] = 1; // (instrument) void
    
    // =============================================================================
    // SuperTerminal API - Phase 5: Asset Management
    // =============================================================================
    
    // Initialization
    m_builtinFunctions["ASSET_INIT"] = 2;          // (db_path$, max_cache_size) Returns INT (bool)
    m_builtinFunctions["ASSET_SHUTDOWN"] = 0;      // void
    m_builtinFunctions["ASSET_IS_INITIALIZED"] = 0; // Returns INT (bool)
    
    // Loading / Unloading
    m_builtinFunctions["ASSET_LOAD"] = 1;          // (name$) Returns INT (asset ID)
    m_builtinFunctions["ASSET_LOAD_FILE"] = 2;     // (path$, type) Returns INT (asset ID)
    m_builtinFunctions["ASSET_LOAD_BUILTIN"] = 2;  // (name$, type) Returns INT (asset ID)
    m_builtinFunctions["ASSET_UNLOAD"] = 1;        // (id) void
    m_builtinFunctions["ASSET_IS_LOADED"] = 1;     // (name$) Returns INT (bool)
    
    // Import / Export
    m_builtinFunctions["ASSET_IMPORT"] = 3;        // (file_path$, asset_name$, type) Returns INT (bool)
    m_builtinFunctions["ASSET_IMPORT_DIR"] = 2;    // (directory$, recursive) Returns INT (count)
    m_builtinFunctions["ASSET_EXPORT"] = 2;        // (asset_name$, file_path$) Returns INT (bool)
    m_builtinFunctions["ASSET_DELETE"] = 1;        // (asset_name$) Returns INT (bool)
    
    // Data Access
    m_builtinFunctions["ASSET_GET_SIZE"] = 1;      // (id) Returns INT
    m_builtinFunctions["ASSET_GET_TYPE"] = 1;      // (id) Returns INT
    m_builtinFunctions["ASSET_GET_NAME"] = 1;      // (id) Returns STRING
    
    // Queries
    m_builtinFunctions["ASSET_EXISTS"] = 1;        // (name$) Returns INT (bool)
    m_builtinFunctions["ASSET_GET_COUNT"] = 1;     // (type) Returns INT
    
    // Cache Management
    m_builtinFunctions["ASSET_CLEAR_CACHE"] = 0;   // void
    m_builtinFunctions["ASSET_GET_CACHE_SIZE"] = 0; // Returns INT
    m_builtinFunctions["ASSET_GET_CACHED_COUNT"] = 0; // Returns INT
    m_builtinFunctions["ASSET_SET_MAX_CACHE"] = 1; // (max_size) void
    
    // Statistics
    m_builtinFunctions["ASSET_GET_HIT_RATE"] = 0;  // Returns FLOAT
    m_builtinFunctions["ASSET_GET_DB_SIZE"] = 0;   // Returns INT
    
    // Error Handling
    m_builtinFunctions["ASSET_GET_ERROR"] = 0;     // Returns STRING
    m_builtinFunctions["ASSET_CLEAR_ERROR"] = 0;   // void
    
    // =============================================================================
    // SuperTerminal API - Phase 4: Tilemaps & Particles
    // =============================================================================
    
    // Tilemap System
    m_builtinFunctions["TILEMAP_INIT"] = 2;        // (viewport_w, viewport_h) Returns INT (bool)
    m_builtinFunctions["TILEMAP_SHUTDOWN"] = 0;    // void
    m_builtinFunctions["TILEMAP_CREATE"] = 4;      // (w, h, tile_w, tile_h) Returns INT (ID)
    m_builtinFunctions["TILEMAP_DESTROY"] = 1;     // (id) void
    m_builtinFunctions["TILEMAP_GET_WIDTH"] = 1;   // (id) Returns INT
    m_builtinFunctions["TILEMAP_GET_HEIGHT"] = 1;  // (id) Returns INT
    
    // Tileset
    m_builtinFunctions["TILESET_LOAD"] = 5;        // (path$, tw, th, margin, spacing) Returns INT (ID)
    m_builtinFunctions["TILESET_DESTROY"] = 1;     // (id) void
    m_builtinFunctions["TILESET_GET_COUNT"] = 1;   // (id) Returns INT
    
    // Layer Management
    m_builtinFunctions["TILEMAP_CREATE_LAYER"] = 1;     // (name$) Returns INT (layer ID)
    m_builtinFunctions["TILEMAP_DESTROY_LAYER"] = 1;    // (layer_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_MAP"] = 2;    // (layer_id, map_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_TILESET"] = 2; // (layer_id, tileset_id) void
    m_builtinFunctions["TILEMAP_LAYER_SET_PARALLAX"] = 3; // (layer_id, px, py) void
    m_builtinFunctions["TILEMAP_LAYER_SET_VISIBLE"] = 2;  // (layer_id, visible) void
    m_builtinFunctions["TILEMAP_LAYER_SET_Z_ORDER"] = 2;  // (layer_id, z) void
    
    // Tile Operations
    m_builtinFunctions["TILEMAP_SET_TILE"] = 4;    // (layer_id, x, y, tile_id) void
    m_builtinFunctions["TILEMAP_GET_TILE"] = 3;    // (layer_id, x, y) Returns INT
    m_builtinFunctions["TILEMAP_FILL_RECT"] = 6;   // (layer_id, x, y, w, h, tile_id) void
    m_builtinFunctions["TILEMAP_CLEAR"] = 1;       // (layer_id) void
    
    // Camera Control
    m_builtinFunctions["TILEMAP_SET_CAMERA"] = 2;  // (x, y) void
    m_builtinFunctions["TILEMAP_MOVE_CAMERA"] = 2; // (dx, dy) void
    m_builtinFunctions["TILEMAP_GET_CAMERA_X"] = 0; // Returns FLOAT
    m_builtinFunctions["TILEMAP_GET_CAMERA_Y"] = 0; // Returns FLOAT
    m_builtinFunctions["TILEMAP_SET_ZOOM"] = 1;    // (zoom) void
    m_builtinFunctions["TILEMAP_CAMERA_SHAKE"] = 2; // (magnitude, duration) void
    
    // Update
    m_builtinFunctions["TILEMAP_UPDATE"] = 1;      // (delta_time) void
    
    // Particle System
    m_builtinFunctions["PARTICLE_INIT"] = 1;       // (max_particles) Returns INT (bool)
    m_builtinFunctions["PARTICLE_SHUTDOWN"] = 0;   // void
    m_builtinFunctions["PARTICLE_IS_READY"] = 0;   // Returns INT (bool)
    m_builtinFunctions["PARTICLE_EXPLODE"] = 4;    // (x, y, count, color) Returns INT (bool)
    m_builtinFunctions["PARTICLE_EXPLODE_ADV"] = 7; // (x, y, count, color, force, gravity, fade) Returns INT
    m_builtinFunctions["PARTICLE_CLEAR"] = 0;      // void
    m_builtinFunctions["PARTICLE_PAUSE"] = 0;      // void
    m_builtinFunctions["PARTICLE_RESUME"] = 0;     // void
    m_builtinFunctions["PARTICLE_GET_COUNT"] = 0;  // Returns INT
}

bool SemanticAnalyzer::isBuiltinFunction(const std::string& name) const {
    return m_builtinFunctions.find(name) != m_builtinFunctions.end();
}

VariableType SemanticAnalyzer::getBuiltinReturnType(const std::string& name) const {
    if (!isBuiltinFunction(name)) {
        return VariableType::UNKNOWN;
    }
    
    // String functions return STRING
    if (name.back() == '$') {
        // Return UNICODE type if in Unicode mode
        return m_symbolTable.unicodeMode ? VariableType::UNICODE : VariableType::STRING;
    }
    
    // LEN and ASC return INT
    if (name == "LEN" || name == "ASC") {
        return VariableType::INT;
    }
    
    // SuperTerminal API functions that return INT
    if (name == "TEXT_GET_WIDTH" || name == "TEXT_GET_HEIGHT" ||
        name == "CHUNKY_GET_WIDTH" || name == "CHUNKY_GET_HEIGHT" ||
        name == "COLOR_RGB" || name == "COLOR_RGBA" || name == "COLOR_HSV" ||
        name == "FRAME_COUNT" || name == "RANDOM_INT" ||
        name == "KEY_PRESSED" || name == "KEY_JUST_PRESSED" || name == "KEY_JUST_RELEASED" ||
        name == "KEY_GET_CHAR" || 
        name == "MOUSE_X" || name == "MOUSE_Y" || 
        name == "MOUSE_GRID_X" || name == "MOUSE_GRID_Y" ||
        name == "MOUSE_BUTTON" || name == "MOUSE_BUTTON_PRESSED" || name == "MOUSE_BUTTON_RELEASED" ||
        name == "SPRITE_LOAD" || name == "SPRITE_LOAD_BUILTIN" || name == "DRAWINTOSPRITE" ||
        name == "DRAWTOFILE" || name == "ENDDRAWTOFILE" ||
        name == "DRAWTOTILESET" || name == "DRAWTILE" || name == "ENDDRAWTOTILESET" ||
        name == "DISPLAY_WIDTH" || name == "DISPLAY_HEIGHT" ||
        name == "CELL_WIDTH" || name == "CELL_HEIGHT" ||
        name == "SOUND_LOAD" || name == "SOUND_LOAD_BUILTIN" ||
        name == "MUSIC_IS_PLAYING" ||
        name == "TILEMAP_INIT" || name == "TILEMAP_CREATE" ||
        name == "TILEMAP_GET_WIDTH" || name == "TILEMAP_GET_HEIGHT" ||
        name == "TILESET_LOAD" || name == "TILESET_GET_COUNT" ||
        name == "TILEMAP_CREATE_LAYER" || name == "TILEMAP_GET_TILE" ||
        name == "PARTICLE_INIT" || name == "PARTICLE_IS_READY" ||
        name == "PARTICLE_EXPLODE" || name == "PARTICLE_EXPLODE_ADV" ||
        name == "PARTICLE_GET_COUNT" ||
        name == "ASSET_INIT" || name == "ASSET_IS_INITIALIZED" ||
        name == "ASSET_LOAD" || name == "ASSET_LOAD_FILE" || name == "ASSET_LOAD_BUILTIN" ||
        name == "ASSET_IS_LOADED" || name == "ASSET_IMPORT" || name == "ASSET_IMPORT_DIR" ||
        name == "ASSET_EXPORT" || name == "ASSET_DELETE" ||
        name == "ASSET_GET_SIZE" || name == "ASSET_GET_TYPE" ||
        name == "ASSET_EXISTS" || name == "ASSET_GET_COUNT" ||
        name == "ASSET_GET_CACHE_SIZE" || name == "ASSET_GET_CACHED_COUNT" ||
        name == "ASSET_GET_DB_SIZE") {
        return VariableType::INT;
    }
    
    // SuperTerminal API functions that return FLOAT
    if (name == "TIME" || name == "DELTA_TIME" || name == "RANDOM" ||
        name == "MOUSE_WHEEL_X" || name == "MOUSE_WHEEL_Y" ||
        name == "TILEMAP_GET_CAMERA_X" || name == "TILEMAP_GET_CAMERA_Y" ||
        name == "ASSET_GET_HIT_RATE") {
        return VariableType::FLOAT;
    }
    
    // SuperTerminal API void functions (no return type)
    if (name.find("TEXT_") == 0 || name.find("CHUNKY_") == 0 || 
        name.find("GFX_") == 0 || name.find("SPRITE_") == 0 ||
        name.find("LAYER_") == 0 || name.find("SOUND_") == 0 ||
        name.find("MUSIC_") == 0 || name.find("SYNTH_") == 0 ||
        name.find("TILEMAP_") == 0 || name.find("TILESET_") == 0 ||
        name.find("PARTICLE_") == 0 || name.find("ASSET_") == 0 ||
        name == "FRAME_WAIT" || name == "RANDOM_SEED" || 
        name == "KEY_CLEAR_BUFFER") {
        // These are void functions, but we need to return something
        // We'll return INT as a placeholder (value will be ignored)
        return VariableType::INT;
    }
    
    // Asset functions that return STRING
    if (name == "ASSET_GET_NAME" || name == "ASSET_GET_ERROR") {
        // These always return byte strings, not Unicode
        return VariableType::STRING;
    }
    
    // All other functions return FLOAT
    return VariableType::FLOAT;
}

int SemanticAnalyzer::getBuiltinArgCount(const std::string& name) const {
    auto it = m_builtinFunctions.find(name);
    if (it != m_builtinFunctions.end()) {
        return it->second;
    }
    return 0;
}

void SemanticAnalyzer::loadFromCommandRegistry(const ModularCommands::CommandRegistry& registry) {
    // Get all commands and functions from the registry
    const auto& commands = registry.getAllCommands();
    
    for (const auto& pair : commands) {
        const std::string& name = pair.first;
        const ModularCommands::CommandDefinition& def = pair.second;
        
        // Add to builtin functions map with parameter count
        // Use required parameter count (commands may have optional parameters)
        int paramCount = static_cast<int>(def.getRequiredParameterCount());
        
        // Only add if not already present (don't override hardcoded core functions)
        if (m_builtinFunctions.find(name) == m_builtinFunctions.end()) {
            m_builtinFunctions[name] = paramCount;
        }
    }
}

// =============================================================================
// Constant Expression Evaluation (Compile-Time)
// =============================================================================

FasterBASIC::ConstantValue SemanticAnalyzer::evaluateConstantExpression(const Expression& expr) {
    switch (expr.getType()) {
        case ASTNodeType::EXPR_NUMBER: {
            const auto& number = static_cast<const NumberExpression&>(expr);
            double val = number.value;
            // Check if it's an integer
            if (val == std::floor(val) && val >= INT64_MIN && val <= INT64_MAX) {
                return static_cast<int64_t>(val);
            }
            return val;
        }
        
        case ASTNodeType::EXPR_STRING: {
            const auto& str = static_cast<const StringExpression&>(expr);
            return str.value;
        }
        
        case ASTNodeType::EXPR_BINARY:
            return evalConstantBinary(static_cast<const BinaryExpression&>(expr));
        
        case ASTNodeType::EXPR_UNARY:
            return evalConstantUnary(static_cast<const UnaryExpression&>(expr));
        
        case ASTNodeType::EXPR_FUNCTION_CALL:
            return evalConstantFunction(static_cast<const FunctionCallExpression&>(expr));
        
        case ASTNodeType::EXPR_VARIABLE:
            return evalConstantVariable(static_cast<const VariableExpression&>(expr));
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Expression type not supported in constant evaluation",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantBinary(const BinaryExpression& expr) {
    FasterBASIC::ConstantValue left = evaluateConstantExpression(*expr.left);
    FasterBASIC::ConstantValue right = evaluateConstantExpression(*expr.right);
    
    // String concatenation
    if (expr.op == TokenType::PLUS && 
        (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right))) {
        std::string leftStr = std::holds_alternative<std::string>(left) ? 
            std::get<std::string>(left) : std::to_string(getConstantAsDouble(left));
        std::string rightStr = std::holds_alternative<std::string>(right) ? 
            std::get<std::string>(right) : std::to_string(getConstantAsDouble(right));
        return leftStr + rightStr;
    }
    
    // Numeric operations
    if (!isConstantNumeric(left) || !isConstantNumeric(right)) {
        error(SemanticErrorType::TYPE_MISMATCH,
              "Constant expression requires numeric operands",
              expr.location);
        return static_cast<int64_t>(0);
    }
    
    bool isInteger = (std::holds_alternative<int64_t>(left) && 
                      std::holds_alternative<int64_t>(right));
    
    switch (expr.op) {
        case TokenType::PLUS:
            if (isInteger) {
                return std::get<int64_t>(left) + std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) + getConstantAsDouble(right);
        
        case TokenType::MINUS:
            if (isInteger) {
                return std::get<int64_t>(left) - std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) - getConstantAsDouble(right);
        
        case TokenType::MULTIPLY:
            if (isInteger) {
                return std::get<int64_t>(left) * std::get<int64_t>(right);
            }
            return getConstantAsDouble(left) * getConstantAsDouble(right);
        
        case TokenType::DIVIDE:
            return getConstantAsDouble(left) / getConstantAsDouble(right);
        
        case TokenType::POWER:
            return std::pow(getConstantAsDouble(left), getConstantAsDouble(right));
        
        case TokenType::MOD:
            if (isInteger) {
                return std::get<int64_t>(left) % std::get<int64_t>(right);
            }
            return std::fmod(getConstantAsDouble(left), getConstantAsDouble(right));
        
        case TokenType::INT_DIVIDE: // Integer division
            return getConstantAsInt(left) / getConstantAsInt(right);
        
        case TokenType::AND:
            return getConstantAsInt(left) & getConstantAsInt(right);
        
        case TokenType::OR:
            return getConstantAsInt(left) | getConstantAsInt(right);
        
        case TokenType::XOR:
            return getConstantAsInt(left) ^ getConstantAsInt(right);
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Operator not supported in constant expressions",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantUnary(const UnaryExpression& expr) {
    FasterBASIC::ConstantValue operand = evaluateConstantExpression(*expr.expr);
    
    switch (expr.op) {
        case TokenType::MINUS:
            if (std::holds_alternative<int64_t>(operand)) {
                return -std::get<int64_t>(operand);
            }
            return -std::get<double>(operand);
        
        case TokenType::PLUS:
            return operand;
        
        case TokenType::NOT:
            return ~getConstantAsInt(operand);
        
        default:
            error(SemanticErrorType::TYPE_MISMATCH,
                  "Unary operator not supported in constant expressions",
                  expr.location);
            return static_cast<int64_t>(0);
    }
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantFunction(const FunctionCallExpression& expr) {
    std::string funcName = expr.name;
    
    // Convert to uppercase for comparison
    for (auto& c : funcName) c = std::toupper(c);
    
    // Math functions (single argument)
    if (funcName == "ABS" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (std::holds_alternative<int64_t>(arg)) {
            return std::abs(std::get<int64_t>(arg));
        }
        return std::abs(std::get<double>(arg));
    }
    
    if (funcName == "SIN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::sin(getConstantAsDouble(arg));
    }
    
    if (funcName == "COS" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::cos(getConstantAsDouble(arg));
    }
    
    if (funcName == "TAN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::tan(getConstantAsDouble(arg));
    }
    
    if (funcName == "ATN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::atan(getConstantAsDouble(arg));
    }
    
    if (funcName == "EXP" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::exp(getConstantAsDouble(arg));
    }
    
    if (funcName == "LOG" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::log(getConstantAsDouble(arg));
    }
    
    if (funcName == "SQR" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return std::sqrt(getConstantAsDouble(arg));
    }
    
    if (funcName == "INT" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        return static_cast<int64_t>(std::floor(getConstantAsDouble(arg)));
    }
    
    if (funcName == "SGN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        double val = getConstantAsDouble(arg);
        return static_cast<int64_t>(val > 0 ? 1 : (val < 0 ? -1 : 0));
    }
    
    // String functions
    if (funcName == "LEN" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (!std::holds_alternative<std::string>(arg)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "LEN requires string argument",
                  expr.location);
            return static_cast<int64_t>(0);
        }
        return static_cast<int64_t>(std::get<std::string>(arg).length());
    }
    
    if ((funcName == "LEFT$" || funcName == "LEFT") && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "LEFT$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t n = getConstantAsInt(len);
        return std::get<std::string>(str).substr(0, std::max(int64_t(0), n));
    }
    
    if ((funcName == "RIGHT$" || funcName == "RIGHT") && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "RIGHT$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t n = getConstantAsInt(len);
        std::string strVal = std::get<std::string>(str);
        size_t strLen = strVal.length();
        if (n >= static_cast<int64_t>(strLen)) {
            return str;
        }
        return strVal.substr(strLen - n);
    }
    
    if ((funcName == "MID$" || funcName == "MID") && 
        (expr.arguments.size() == 2 || expr.arguments.size() == 3)) {
        FasterBASIC::ConstantValue str = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue start = evaluateConstantExpression(*expr.arguments[1]);
        if (!std::holds_alternative<std::string>(str)) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "MID$ requires string argument",
                  expr.location);
            return std::string("");
        }
        int64_t startPos = getConstantAsInt(start) - 1; // BASIC is 1-indexed
        if (startPos < 0) startPos = 0;
        
        std::string strVal = std::get<std::string>(str);
        if (expr.arguments.size() == 3) {
            FasterBASIC::ConstantValue len = evaluateConstantExpression(*expr.arguments[2]);
            int64_t length = getConstantAsInt(len);
            return strVal.substr(startPos, length);
        } else {
            return strVal.substr(startPos);
        }
    }
    
    if ((funcName == "CHR$" || funcName == "CHR") && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        int64_t code = getConstantAsInt(arg);
        if (code < 0 || code > 255) {
            error(SemanticErrorType::TYPE_MISMATCH,
                  "CHR$ argument must be 0-255",
                  expr.location);
            return std::string("");
        }
        return std::string(1, static_cast<char>(code));
    }
    
    if (funcName == "STR$" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (std::holds_alternative<int64_t>(arg)) {
            return std::to_string(std::get<int64_t>(arg));
        } else if (std::holds_alternative<double>(arg)) {
            return std::to_string(std::get<double>(arg));
        }
        return arg; // Already a string
    }
    
    if (funcName == "VAL" && expr.arguments.size() == 1) {
        FasterBASIC::ConstantValue arg = evaluateConstantExpression(*expr.arguments[0]);
        if (!std::holds_alternative<std::string>(arg)) {
            return arg; // Already numeric
        }
        try {
            std::string strVal = std::get<std::string>(arg);
            // Try to parse as integer first
            size_t pos;
            int64_t intVal = std::stoll(strVal, &pos);
            if (pos == strVal.length()) {
                return intVal;
            }
            // Otherwise parse as double
            double dblVal = std::stod(strVal);
            return dblVal;
        } catch (...) {
            return 0.0;
        }
    }
    
    // Two-argument math functions
    if (funcName == "MIN" && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue arg1 = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue arg2 = evaluateConstantExpression(*expr.arguments[1]);
        double v1 = getConstantAsDouble(arg1);
        double v2 = getConstantAsDouble(arg2);
        return std::min(v1, v2);
    }
    
    if (funcName == "MAX" && expr.arguments.size() == 2) {
        FasterBASIC::ConstantValue arg1 = evaluateConstantExpression(*expr.arguments[0]);
        FasterBASIC::ConstantValue arg2 = evaluateConstantExpression(*expr.arguments[1]);
        double v1 = getConstantAsDouble(arg1);
        double v2 = getConstantAsDouble(arg2);
        return std::max(v1, v2);
    }
    
    error(SemanticErrorType::UNDEFINED_FUNCTION,
          "Function " + funcName + " not supported in constant expressions or wrong number of arguments",
          expr.location);
    return static_cast<int64_t>(0);
}

FasterBASIC::ConstantValue SemanticAnalyzer::evalConstantVariable(const VariableExpression& expr) {
    // Look up constant by name
    auto it = m_symbolTable.constants.find(expr.name);
    if (it == m_symbolTable.constants.end()) {
        error(SemanticErrorType::UNDEFINED_VARIABLE,
              "Undefined constant: " + expr.name,
              expr.location);
        return static_cast<int64_t>(0);
    }
    
    const ConstantSymbol& sym = it->second;
    if (sym.type == ConstantSymbol::Type::INTEGER) {
        return sym.intValue;
    } else if (sym.type == ConstantSymbol::Type::DOUBLE) {
        return sym.doubleValue;
    } else {
        return sym.stringValue;
    }
}

bool SemanticAnalyzer::isConstantNumeric(const FasterBASIC::ConstantValue& val) {
    return std::holds_alternative<int64_t>(val) || std::holds_alternative<double>(val);
}

double SemanticAnalyzer::getConstantAsDouble(const FasterBASIC::ConstantValue& val) {
    if (std::holds_alternative<int64_t>(val)) {
        return static_cast<double>(std::get<int64_t>(val));
    } else if (std::holds_alternative<double>(val)) {
        return std::get<double>(val);
    }
    return 0.0;
}

int64_t SemanticAnalyzer::getConstantAsInt(const FasterBASIC::ConstantValue& val) {
    if (std::holds_alternative<int64_t>(val)) {
        return std::get<int64_t>(val);
    } else if (std::holds_alternative<double>(val)) {
        return static_cast<int64_t>(std::get<double>(val));
    }
    return 0;
}

} // namespace FasterBASIC