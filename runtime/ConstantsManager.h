//
// ConstantsManager.h
// FasterBASIC Runtime - Constants Manager
//
// Manages compile-time constants with efficient integer-indexed storage.
// Constants are stored in a vector and accessed by index for maximum performance.
//

#ifndef CONSTANTS_MANAGER_H
#define CONSTANTS_MANAGER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace FasterBASIC {

// Constant value type (can be int, double, or string)
using ConstantValue = std::variant<int64_t, double, std::string>;

class ConstantsManager {
public:
    ConstantsManager();
    ~ConstantsManager() = default;

    // Add a constant and return its index
    int addConstant(const std::string& name, int64_t value);
    int addConstant(const std::string& name, double value);
    int addConstant(const std::string& name, const std::string& value);

    // Get constant value by index
    ConstantValue getConstant(int index) const;

    // Get constant as specific type (with automatic conversion)
    int64_t getConstantAsInt(int index) const;
    double getConstantAsDouble(int index) const;
    std::string getConstantAsString(int index) const;

    // Check if constant exists
    bool hasConstant(const std::string& name) const;

    // Get constant index by name (returns -1 if not found)
    int getConstantIndex(const std::string& name) const;

    // Get number of constants
    size_t getConstantCount() const { return m_constants.size(); }

    // Clear all constants
    void clear();

    // Add predefined constants (GRAPHICS_WIDTH, TEXT_WIDTH, etc.)
    void addPredefinedConstants();

    // Copy all constants from another manager (preserves indices)
    void copyFrom(const ConstantsManager& other);

    // Get all constant names (for iterating over all constants)
    std::vector<std::string> getAllConstantNames() const;

private:
    std::vector<ConstantValue> m_constants;  // Indexed storage
    std::unordered_map<std::string, int> m_nameToIndex;  // Name to index mapping
};

} // namespace FasterBASIC

#endif // CONSTANTS_MANAGER_H
