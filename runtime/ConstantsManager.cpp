//
// ConstantsManager.cpp
// FasterBASIC Runtime - Constants Manager Implementation
//
// Manages compile-time constants with efficient integer-indexed storage.
//

#include "ConstantsManager.h"
#include <stdexcept>
#include <sstream>

namespace FasterBASIC {

ConstantsManager::ConstantsManager() {
    // Reserve space for typical number of constants
    m_constants.reserve(64);
}

int ConstantsManager::addConstant(const std::string& name, int64_t value) {
    // Check if constant already exists
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[name] = index;
    return index;
}

int ConstantsManager::addConstant(const std::string& name, double value) {
    // Check if constant already exists
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[name] = index;
    return index;
}

int ConstantsManager::addConstant(const std::string& name, const std::string& value) {
    // Check if constant already exists
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end()) {
        // Update existing constant
        m_constants[it->second] = value;
        return it->second;
    }

    // Add new constant
    int index = static_cast<int>(m_constants.size());
    m_constants.push_back(value);
    m_nameToIndex[name] = index;
    return index;
}

ConstantValue ConstantsManager::getConstant(int index) const {
    if (index < 0 || index >= static_cast<int>(m_constants.size())) {
        throw std::out_of_range("Constant index out of range");
    }
    return m_constants[index];
}

int64_t ConstantsManager::getConstantAsInt(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    } else if (std::holds_alternative<double>(value)) {
        return static_cast<int64_t>(std::get<double>(value));
    } else {
        // String to int conversion
        try {
            return std::stoll(std::get<std::string>(value));
        } catch (...) {
            return 0;
        }
    }
}

double ConstantsManager::getConstantAsDouble(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    } else {
        // String to double conversion
        try {
            return std::stod(std::get<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
}

std::string ConstantsManager::getConstantAsString(int index) const {
    auto value = getConstant(index);

    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else {
        return std::to_string(std::get<double>(value));
    }
}

bool ConstantsManager::hasConstant(const std::string& name) const {
    return m_nameToIndex.find(name) != m_nameToIndex.end();
}

int ConstantsManager::getConstantIndex(const std::string& name) const {
    auto it = m_nameToIndex.find(name);
    if (it != m_nameToIndex.end()) {
        return it->second;
    }
    return -1;
}

void ConstantsManager::clear() {
    m_constants.clear();
    m_nameToIndex.clear();
}

void ConstantsManager::copyFrom(const ConstantsManager& other) {
    // Clear existing constants
    m_constants.clear();
    m_nameToIndex.clear();
    
    // Copy the entire vector (preserves indices)
    m_constants = other.m_constants;
    
    // Copy the name-to-index map
    m_nameToIndex = other.m_nameToIndex;
}

void ConstantsManager::addPredefinedConstants() {
    // NOTE: Graphics dimensions (GRAPHICS_WIDTH, GRAPHICS_HEIGHT) should be
    // queried from the runtime/window system, not hardcoded as constants.
    // Use runtime functions like WIDTH() or SCREEN_WIDTH() instead.
    
    // Mathematical constants
    addConstant("PI", 3.14159265358979323846);
    addConstant("E", 2.71828182845904523536);
    addConstant("SQRT2", 1.41421356237309504880);
    addConstant("SQRT3", 1.73205080756887729353);
    addConstant("GOLDEN_RATIO", 1.61803398874989484820);

    // Boolean constants
    addConstant("TRUE", static_cast<int64_t>(1));
    addConstant("FALSE", static_cast<int64_t>(0));

    // Display mode constants
    addConstant("TEXT", static_cast<int64_t>(0));    // TEXT mode (standard text grid)
    addConstant("LORES", static_cast<int64_t>(1));   // LORES mode (160×75 pixel buffer)
    addConstant("MIDRES", static_cast<int64_t>(2));  // MIDRES mode (320×150 pixel buffer)
    addConstant("HIRES", static_cast<int64_t>(3));   // HIRES mode (640×300 pixel buffer)
    addConstant("ULTRARES", static_cast<int64_t>(4)); // ULTRARES mode (1280×720 direct color ARGB4444)

    // Color constants (24-bit RGB values for compatibility)
    addConstant("BLACK", static_cast<int64_t>(0x000000));
    addConstant("WHITE", static_cast<int64_t>(0xFFFFFF));
    addConstant("RED", static_cast<int64_t>(0xFF0000));
    addConstant("GREEN", static_cast<int64_t>(0x00FF00));
    addConstant("BLUE", static_cast<int64_t>(0x0000FF));
    addConstant("YELLOW", static_cast<int64_t>(0xFFFF00));
    addConstant("CYAN", static_cast<int64_t>(0x00FFFF));
    addConstant("MAGENTA", static_cast<int64_t>(0xFF00FF));

    // RGBA color constants (32-bit with alpha channel - 0xRRGGBBAA)
    // SOLID_* variants are fully opaque (alpha = 0xFF)
    addConstant("SOLID_BLACK", static_cast<int64_t>(0x000000FF));
    addConstant("SOLID_WHITE", static_cast<int64_t>(0xFFFFFFFF));
    addConstant("SOLID_RED", static_cast<int64_t>(0xFF0000FF));
    addConstant("SOLID_GREEN", static_cast<int64_t>(0x00FF00FF));
    addConstant("SOLID_BLUE", static_cast<int64_t>(0x0000FFFF));
    addConstant("SOLID_YELLOW", static_cast<int64_t>(0xFFFF00FF));
    addConstant("SOLID_CYAN", static_cast<int64_t>(0x00FFFFFF));
    addConstant("SOLID_MAGENTA", static_cast<int64_t>(0xFF00FFFF));
    
    // CLEAR_BLACK is fully transparent (alpha = 0x00)
    addConstant("CLEAR_BLACK", static_cast<int64_t>(0x00000000));

    // C64 Color Palette (ARGB format: 0xAARRGGBB)
    // These are the classic Commodore 64 colors, perfect for retro graphics
    // and 16-color features like chunky pixels
    addConstant("COLOUR_0", static_cast<int64_t>(0xFF000000));  // Black
    addConstant("COLOUR_1", static_cast<int64_t>(0xFFFFFFFF));  // White
    addConstant("COLOUR_2", static_cast<int64_t>(0xFF880000));  // Red
    addConstant("COLOUR_3", static_cast<int64_t>(0xFFAAFFEE));  // Cyan
    addConstant("COLOUR_4", static_cast<int64_t>(0xFFCC44CC));  // Purple
    addConstant("COLOUR_5", static_cast<int64_t>(0xFF00CC55));  // Green
    addConstant("COLOUR_6", static_cast<int64_t>(0xFF0000AA));  // Blue
    addConstant("COLOUR_7", static_cast<int64_t>(0xFFEEEE77));  // Yellow
    addConstant("COLOUR_8", static_cast<int64_t>(0xFFDD8855));  // Orange
    addConstant("COLOUR_9", static_cast<int64_t>(0xFF664400));  // Brown
    addConstant("COLOUR_10", static_cast<int64_t>(0xFFFF7777)); // Light Red
    addConstant("COLOUR_11", static_cast<int64_t>(0xFF333333)); // Dark Grey
    addConstant("COLOUR_12", static_cast<int64_t>(0xFF777777)); // Grey
    addConstant("COLOUR_13", static_cast<int64_t>(0xFFAAFF66)); // Light Green
    addConstant("COLOUR_14", static_cast<int64_t>(0xFF0088FF)); // Light Blue
    addConstant("COLOUR_15", static_cast<int64_t>(0xFFBBBBBB)); // Light Grey

    // Voice/Audio Waveform Types
    addConstant("WAVE_SILENCE", static_cast<int64_t>(0));
    addConstant("WAVE_SINE", static_cast<int64_t>(1));
    addConstant("WAVE_SQUARE", static_cast<int64_t>(2));
    addConstant("WAVE_SAWTOOTH", static_cast<int64_t>(3));
    addConstant("WAVE_TRIANGLE", static_cast<int64_t>(4));
    addConstant("WAVE_NOISE", static_cast<int64_t>(5));
    addConstant("WAVE_PULSE", static_cast<int64_t>(6));
    addConstant("WAVE_PHYSICAL", static_cast<int64_t>(7));

    // Physical Model Types
    addConstant("MODEL_PLUCKED_STRING", static_cast<int64_t>(0));
    addConstant("MODEL_STRUCK_BAR", static_cast<int64_t>(1));
    addConstant("MODEL_BLOWN_TUBE", static_cast<int64_t>(2));
    addConstant("MODEL_DRUMHEAD", static_cast<int64_t>(3));
    addConstant("MODEL_GLASS", static_cast<int64_t>(4));

    // Filter Types
    addConstant("FILTER_NONE", static_cast<int64_t>(0));
    addConstant("FILTER_LOWPASS", static_cast<int64_t>(1));
    addConstant("FILTER_HIGHPASS", static_cast<int64_t>(2));
    addConstant("FILTER_BANDPASS", static_cast<int64_t>(3));
    addConstant("FILTER_NOTCH", static_cast<int64_t>(4));

    // LFO Waveform Types
    addConstant("LFO_SINE", static_cast<int64_t>(0));
    addConstant("LFO_TRIANGLE", static_cast<int64_t>(1));
    addConstant("LFO_SQUARE", static_cast<int64_t>(2));
    addConstant("LFO_SAWTOOTH", static_cast<int64_t>(3));
    addConstant("LFO_RANDOM", static_cast<int64_t>(4));

    // Rectangle Gradient Modes
    addConstant("ST_GRADIENT_SOLID", static_cast<int64_t>(0));
    addConstant("ST_GRADIENT_HORIZONTAL", static_cast<int64_t>(1));
    addConstant("ST_GRADIENT_VERTICAL", static_cast<int64_t>(2));
    addConstant("ST_GRADIENT_DIAGONAL_TL_BR", static_cast<int64_t>(3));
    addConstant("ST_GRADIENT_DIAGONAL_TR_BL", static_cast<int64_t>(4));
    addConstant("ST_GRADIENT_RADIAL", static_cast<int64_t>(5));
    addConstant("ST_GRADIENT_FOUR_CORNER", static_cast<int64_t>(6));
    addConstant("ST_GRADIENT_THREE_POINT", static_cast<int64_t>(7));

    // Rectangle Procedural Pattern Modes
    addConstant("ST_PATTERN_OUTLINE", static_cast<int64_t>(100));
    addConstant("ST_PATTERN_DASHED_OUTLINE", static_cast<int64_t>(101));
    addConstant("ST_PATTERN_HORIZONTAL_STRIPES", static_cast<int64_t>(102));
    addConstant("ST_PATTERN_VERTICAL_STRIPES", static_cast<int64_t>(103));
    addConstant("ST_PATTERN_DIAGONAL_STRIPES", static_cast<int64_t>(104));
    addConstant("ST_PATTERN_CHECKERBOARD", static_cast<int64_t>(105));
    addConstant("ST_PATTERN_DOTS", static_cast<int64_t>(106));
    addConstant("ST_PATTERN_CROSSHATCH", static_cast<int64_t>(107));
    addConstant("ST_PATTERN_ROUNDED_CORNERS", static_cast<int64_t>(108));
    addConstant("ST_PATTERN_GRID", static_cast<int64_t>(109));
}

std::vector<std::string> ConstantsManager::getAllConstantNames() const {
    std::vector<std::string> names;
    names.reserve(m_nameToIndex.size());
    
    for (const auto& pair : m_nameToIndex) {
        names.push_back(pair.first);
    }
    
    return names;
}

} // namespace FasterBASIC
