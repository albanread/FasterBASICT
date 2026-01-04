//
// dump_ir.cpp
// IR Dumper - Shows intermediate representation for debugging
//

#include "fasterbasic_lexer.h"
#include "fasterbasic_parser.h"
#include "fasterbasic_semantic.h"
#include "fasterbasic_cfg.h"
#include "fasterbasic_ircode.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace FasterBASIC;

const char* irOpcodeToString(IROpcode op) {
    switch (op) {
        case IROpcode::NOP: return "NOP";
        case IROpcode::PUSH_INT: return "PUSH_INT";
        case IROpcode::PUSH_FLOAT: return "PUSH_FLOAT";
        case IROpcode::PUSH_DOUBLE: return "PUSH_DOUBLE";
        case IROpcode::PUSH_STRING: return "PUSH_STRING";
        case IROpcode::POP: return "POP";
        case IROpcode::DUP: return "DUP";
        case IROpcode::ADD: return "ADD";
        case IROpcode::SUB: return "SUB";
        case IROpcode::MUL: return "MUL";
        case IROpcode::DIV: return "DIV";
        case IROpcode::IDIV: return "IDIV";
        case IROpcode::MOD: return "MOD";
        case IROpcode::POW: return "POW";
        case IROpcode::NEG: return "NEG";
        case IROpcode::NOT: return "NOT";
        case IROpcode::EQ: return "EQ";
        case IROpcode::NE: return "NE";
        case IROpcode::LT: return "LT";
        case IROpcode::LE: return "LE";
        case IROpcode::GT: return "GT";
        case IROpcode::GE: return "GE";
        case IROpcode::AND: return "AND";
        case IROpcode::OR: return "OR";
        case IROpcode::LOAD_VAR: return "LOAD_VAR";
        case IROpcode::STORE_VAR: return "STORE_VAR";
        case IROpcode::LOAD_CONST: return "LOAD_CONST";
        case IROpcode::LOAD_ARRAY: return "LOAD_ARRAY";
        case IROpcode::STORE_ARRAY: return "STORE_ARRAY";
        case IROpcode::DIM_ARRAY: return "DIM_ARRAY";
        case IROpcode::LABEL: return "LABEL";
        case IROpcode::JUMP: return "JUMP";
        case IROpcode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case IROpcode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case IROpcode::CALL_BUILTIN: return "CALL_BUILTIN";
        case IROpcode::CALL_USER_FN: return "CALL_USER_FN";
        case IROpcode::CALL_FUNCTION: return "CALL_FUNCTION";
        case IROpcode::CALL_SUB: return "CALL_SUB";
        case IROpcode::CALL_GOSUB: return "CALL_GOSUB";
        case IROpcode::RETURN_GOSUB: return "RETURN_GOSUB";
        case IROpcode::DEFINE_FUNCTION: return "DEFINE_FUNCTION";
        case IROpcode::DEFINE_SUB: return "DEFINE_SUB";
        case IROpcode::END_FUNCTION: return "END_FUNCTION";
        case IROpcode::END_SUB: return "END_SUB";
        case IROpcode::RETURN_VALUE: return "RETURN_VALUE";
        case IROpcode::RETURN_VOID: return "RETURN_VOID";
        case IROpcode::EXIT_FOR: return "EXIT_FOR";
        case IROpcode::EXIT_DO: return "EXIT_DO";
        case IROpcode::EXIT_WHILE: return "EXIT_WHILE";
        case IROpcode::EXIT_REPEAT: return "EXIT_REPEAT";
        case IROpcode::EXIT_FUNCTION: return "EXIT_FUNCTION";
        case IROpcode::EXIT_SUB: return "EXIT_SUB";
        case IROpcode::FOR_INIT: return "FOR_INIT";
        case IROpcode::FOR_CHECK: return "FOR_CHECK";
        case IROpcode::FOR_NEXT: return "FOR_NEXT";
        case IROpcode::WHILE_START: return "WHILE_START";
        case IROpcode::WHILE_END: return "WHILE_END";
        case IROpcode::REPEAT_START: return "REPEAT_START";
        case IROpcode::REPEAT_END: return "REPEAT_END";
        case IROpcode::DO_WHILE_START: return "DO_WHILE_START";
        case IROpcode::DO_UNTIL_START: return "DO_UNTIL_START";
        case IROpcode::DO_START: return "DO_START";
        case IROpcode::DO_LOOP_WHILE: return "DO_LOOP_WHILE";
        case IROpcode::DO_LOOP_UNTIL: return "DO_LOOP_UNTIL";
        case IROpcode::DO_LOOP_END: return "DO_LOOP_END";
        case IROpcode::PRINT: return "PRINT";
        case IROpcode::CONSOLE: return "CONSOLE";
        case IROpcode::PRINT_NEWLINE: return "PRINT_NEWLINE";
        case IROpcode::PRINT_TAB: return "PRINT_TAB";
        case IROpcode::PRINT_USING: return "PRINT_USING";
        case IROpcode::INPUT: return "INPUT";
        case IROpcode::INPUT_PROMPT: return "INPUT_PROMPT";
        case IROpcode::READ_DATA: return "READ_DATA";
        case IROpcode::RESTORE: return "RESTORE";
        case IROpcode::STR_CONCAT: return "STR_CONCAT";
        case IROpcode::UNICODE_CONCAT: return "UNICODE_CONCAT";
        case IROpcode::STR_LEFT: return "STR_LEFT";
        case IROpcode::STR_RIGHT: return "STR_RIGHT";
        case IROpcode::STR_MID: return "STR_MID";
        case IROpcode::CONV_TO_INT: return "CONV_TO_INT";
        case IROpcode::CONV_TO_FLOAT: return "CONV_TO_FLOAT";
        case IROpcode::CONV_TO_STRING: return "CONV_TO_STRING";
        case IROpcode::IF_START: return "IF_START";
        case IROpcode::ELSEIF_START: return "ELSEIF_START";
        case IROpcode::ELSE_START: return "ELSE_START";
        case IROpcode::IF_END: return "IF_END";
        case IROpcode::HALT: return "HALT";
        case IROpcode::END: return "END";
        default: return "UNKNOWN";
    }
}

std::string operandToString(const IROperand& operand) {
    if (std::holds_alternative<int>(operand)) {
        return std::to_string(std::get<int>(operand));
    } else if (std::holds_alternative<double>(operand)) {
        return std::to_string(std::get<double>(operand));
    } else if (std::holds_alternative<std::string>(operand)) {
        return "\"" + std::get<std::string>(operand) + "\"";
    }
    return "";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.bas>" << std::endl;
        return 1;
    }
    
    try {
        // Read source file
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << argv[1] << std::endl;
            return 1;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)), 
                          std::istreambuf_iterator<char>());
        file.close();
        
        // Compile to IR
        Lexer lexer;
        lexer.tokenize(source);
        auto tokens = lexer.getTokens();
        
        Parser parser;
        auto ast = parser.parse(tokens);
        
        const auto& compilerOptions = parser.getOptions();
        
        SemanticAnalyzer semantic;
        semantic.analyze(*ast, compilerOptions);
        
        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast, semantic.getSymbolTable());
        
        IRGenerator irGen;
        auto irCode = irGen.generate(*cfg, semantic.getSymbolTable());
        
        // Dump IR
        std::cout << "IR Code Dump for: " << argv[1] << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total instructions: " << irCode->instructions.size() << std::endl;
        std::cout << std::endl;
        
        for (size_t i = 0; i < irCode->instructions.size(); ++i) {
            const auto& instr = irCode->instructions[i];
            
            // Show source line number if available
            if (instr.sourceLineNumber > 0) {
                std::cout << "[L" << std::setw(4) << std::right << instr.sourceLineNumber << "] ";
            } else {
                std::cout << "       ";
            }
            
            std::cout << std::setw(4) << i << ": ";
            std::cout << std::setw(20) << std::left << irOpcodeToString(instr.opcode);
            
            std::string op1 = operandToString(instr.operand1);
            std::string op2 = operandToString(instr.operand2);
            
            if (!op1.empty()) {
                std::cout << " " << op1;
            }
            if (!op2.empty()) {
                std::cout << ", " << op2;
            }
            
            std::cout << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}