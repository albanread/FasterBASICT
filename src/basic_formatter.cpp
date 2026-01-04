//
// basic_formatter.cpp
// FasterBASIC - BASIC Code Formatter and Renumberer (Standalone Tool)
//
// Command-line tool for formatting BASIC code with proper indentation
// and renumbering lines while adjusting GOTO/GOSUB/RESTORE references.
//
// This is a thin wrapper around basic_formatter_lib for standalone use.
//

#include "basic_formatter_lib.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char* argv[]) {
    using namespace FasterBASIC;
    
    if (argc < 2) {
        std::cerr << "Usage: basic_formatter <input.bas> [output.bas] [start_line] [step]\n";
        std::cerr << "  input.bas   - BASIC source file to format\n";
        std::cerr << "  output.bas  - Output file (default: stdout)\n";
        std::cerr << "  start_line  - Starting line number (default: 1000)\n";
        std::cerr << "  step        - Line number increment (default: 10)\n";
        std::cerr << "\n";
        std::cerr << "Examples:\n";
        std::cerr << "  basic_formatter program.bas                    # Output to stdout\n";
        std::cerr << "  basic_formatter program.bas formatted.bas      # Output to file\n";
        std::cerr << "  basic_formatter program.bas out.bas 100 10     # Start at 100, step 10\n";
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "";
    int start_line = (argc >= 4) ? std::atoi(argv[3]) : 1000;
    int step = (argc >= 5) ? std::atoi(argv[4]) : 10;
    
    // Validate parameters
    if (start_line < 1) {
        std::cerr << "Error: start_line must be >= 1\n";
        return 1;
    }
    if (step < 1) {
        std::cerr << "Error: step must be >= 1\n";
        return 1;
    }
    
    // Read input file
    std::ifstream infile(input_file);
    if (!infile) {
        std::cerr << "Error: Cannot open input file: " << input_file << "\n";
        return 1;
    }
    
    std::ostringstream buffer;
    buffer << infile.rdbuf();
    std::string source = buffer.str();
    infile.close();
    
    if (source.empty()) {
        std::cerr << "Error: Input file is empty\n";
        return 1;
    }
    
    // Set up formatting options
    FormatterOptions options;
    options.add_indentation = true;
    options.indent_spaces = 2;
    options.update_references = true;
    options.start_line = start_line;
    options.step = step;
    
    // Format the code using the library
    FormatterResult result = formatBasicCode(source, options);
    
    if (!result.success) {
        std::cerr << "Error: Failed to format BASIC code\n";
        if (!result.error_message.empty()) {
            std::cerr << "Details: " << result.error_message << "\n";
        }
        return 1;
    }
    
    // Write output
    if (output_file.empty()) {
        // Output to stdout
        std::cout << result.formatted_code;
    } else {
        // Output to file
        std::ofstream outfile(output_file);
        if (!outfile) {
            std::cerr << "Error: Cannot open output file: " << output_file << "\n";
            return 1;
        }
        outfile << result.formatted_code;
        outfile.close();
        
        // Print summary
        std::cout << "Formatted program written to: " << output_file << "\n";
        std::cout << "Lines processed: " << result.lines_processed << "\n";
        
        if (result.lines_processed > 0) {
            int last_line = start_line + (result.lines_processed - 1) * step;
            std::cout << "Line numbers: " << start_line << " to " 
                      << last_line << " (step " << step << ")\n";
        }
    }
    
    return 0;
}