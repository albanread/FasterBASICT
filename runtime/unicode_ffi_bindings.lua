--
-- unicode_ffi_bindings.lua
-- FasterBASIC - Unicode Runtime FFI Bindings
--
-- Provides Lua interface to C++ Unicode runtime library
-- Used when OPTION UNICODE is enabled
--

local ffi = require('ffi')

-- =============================================================================
-- C Function Declarations
-- =============================================================================

ffi.cdef [[
    // UTF-8 / UTF-32 Conversion
    int32_t* unicode_from_utf8(const char* utf8_str, int32_t* out_len);
    char* unicode_to_utf8(const int32_t* codepoints, int32_t len, int32_t* out_len);
    void unicode_free(void* ptr);

    // Unicode Case Conversion
    void unicode_upper(int32_t* codepoints, int32_t len);
    void unicode_lower(int32_t* codepoints, int32_t len);
    int32_t* unicode_upper_new(const int32_t* codepoints, int32_t len);
    int32_t* unicode_lower_new(const int32_t* codepoints, int32_t len);

    // Validation and Utilities
    int unicode_is_valid_codepoint(int32_t codepoint);
    int unicode_codepoint_to_utf8_bytes(int32_t codepoint);
    int unicode_category(int32_t codepoint);
    int unicode_is_space(int32_t codepoint);
    int unicode_is_letter(int32_t codepoint);
    int unicode_is_digit(int32_t codepoint);

    // Version Information
    const char* unicode_version();
    const char* unicode_standard_version();
]]

-- =============================================================================
-- Load Library
-- =============================================================================

local unicode_lib = nil
local available = false

-- Try to load the Unicode runtime library
-- Look in several locations: build directory, current directory, system paths
local function try_load_unicode()
    local lib_names = {
        -- macOS
        'libunicode_runtime.dylib',
        './libunicode_runtime.dylib',
        '../build/libunicode_runtime.dylib',
        'build/libunicode_runtime.dylib',
        -- Linux
        'libunicode_runtime.so',
        './libunicode_runtime.so',
        '../build/libunicode_runtime.so',
        'build/libunicode_runtime.so',
        -- Windows
        'unicode_runtime.dll',
        './unicode_runtime.dll',
        '../build/unicode_runtime.dll',
        'build/unicode_runtime.dll',
    }

    for _, lib_name in ipairs(lib_names) do
        local ok, lib = pcall(ffi.load, lib_name)
        if ok then
            return lib
        end
    end

    return nil
end

unicode_lib = try_load_unicode()

if unicode_lib then
    available = true
else
    print("Warning: Unicode runtime library not available")
    print("  Tried loading libunicode_runtime from multiple locations")
    print("  OPTION UNICODE will not work correctly")
    available = false
end

-- =============================================================================
-- Lua API Wrapper
-- =============================================================================

local M = {
    available = available,
    lib = unicode_lib
}

-- Convert UTF-8 string to Lua table of codepoints
function M.from_utf8(utf8_str)
    if not available then
        error("Unicode runtime not available")
    end

    if type(utf8_str) ~= "string" then
        error("unicode.from_utf8 expects a string")
    end

    local len_ptr = ffi.new("int32_t[1]")
    local codepoints_ptr = unicode_lib.unicode_from_utf8(utf8_str, len_ptr)

    if codepoints_ptr == nil then
        error("Failed to convert UTF-8 string to codepoints")
    end

    local len = len_ptr[0]
    local result = {}

    -- Copy codepoints to Lua table
    for i = 0, len - 1 do
        result[i + 1] = codepoints_ptr[i]
    end

    -- Free C memory
    unicode_lib.unicode_free(codepoints_ptr)

    return result
end

-- Convert Lua table of codepoints to UTF-8 string
function M.to_utf8(codepoints)
    if not available then
        error("Unicode runtime not available")
    end

    if type(codepoints) ~= "table" then
        error("unicode.to_utf8 expects a table of codepoints")
    end

    local len = #codepoints
    if len == 0 then
        return ""
    end

    -- Create C array from Lua table
    local codepoints_array = ffi.new("int32_t[?]", len)
    for i = 1, len do
        codepoints_array[i - 1] = codepoints[i]
    end

    local out_len_ptr = ffi.new("int32_t[1]")
    local utf8_ptr = unicode_lib.unicode_to_utf8(codepoints_array, len, out_len_ptr)

    if utf8_ptr == nil then
        error("Failed to convert codepoints to UTF-8 string")
    end

    -- Copy to Lua string
    local result = ffi.string(utf8_ptr, out_len_ptr[0])

    -- Free C memory
    unicode_lib.unicode_free(utf8_ptr)

    return result
end

-- Convert codepoints to uppercase (returns new table)
function M.upper(codepoints)
    if not available then
        error("Unicode runtime not available")
    end

    if type(codepoints) ~= "table" then
        error("unicode.upper expects a table of codepoints")
    end

    local len = #codepoints
    if len == 0 then
        return {}
    end

    -- Create C array from Lua table
    local codepoints_array = ffi.new("int32_t[?]", len)
    for i = 1, len do
        codepoints_array[i - 1] = codepoints[i]
    end

    -- Call conversion
    local result_ptr = unicode_lib.unicode_upper_new(codepoints_array, len)

    if result_ptr == nil then
        error("Failed to convert to uppercase")
    end

    -- Copy to Lua table
    local result = {}
    for i = 0, len - 1 do
        result[i + 1] = result_ptr[i]
    end

    -- Free C memory
    unicode_lib.unicode_free(result_ptr)

    return result
end

-- Convert codepoints to lowercase (returns new table)
function M.lower(codepoints)
    if not available then
        error("Unicode runtime not available")
    end

    if type(codepoints) ~= "table" then
        error("unicode.lower expects a table of codepoints")
    end

    local len = #codepoints
    if len == 0 then
        return {}
    end

    -- Create C array from Lua table
    local codepoints_array = ffi.new("int32_t[?]", len)
    for i = 1, len do
        codepoints_array[i - 1] = codepoints[i]
    end

    -- Call conversion
    local result_ptr = unicode_lib.unicode_lower_new(codepoints_array, len)

    if result_ptr == nil then
        error("Failed to convert to lowercase")
    end

    -- Copy to Lua table
    local result = {}
    for i = 0, len - 1 do
        result[i + 1] = result_ptr[i]
    end

    -- Free C memory
    unicode_lib.unicode_free(result_ptr)

    return result
end

-- Check if codepoint is valid
function M.is_valid_codepoint(codepoint)
    if not available then
        return false
    end
    return unicode_lib.unicode_is_valid_codepoint(codepoint) == 1
end

-- Check if codepoint is whitespace
function M.is_space(codepoint)
    if not available then
        return false
    end
    return unicode_lib.unicode_is_space(codepoint) == 1
end

-- Check if codepoint is a letter
function M.is_letter(codepoint)
    if not available then
        return false
    end
    return unicode_lib.unicode_is_letter(codepoint) == 1
end

-- Check if codepoint is a digit
function M.is_digit(codepoint)
    if not available then
        return false
    end
    return unicode_lib.unicode_is_digit(codepoint) == 1
end

-- Get Unicode version
function M.version()
    if not available then
        return "N/A"
    end
    return ffi.string(unicode_lib.unicode_version())
end

-- Get Unicode standard version
function M.standard_version()
    if not available then
        return "N/A"
    end
    return ffi.string(unicode_lib.unicode_standard_version())
end

-- =============================================================================
-- Helper Functions for BASIC String Operations
-- =============================================================================

-- These are implemented in pure Lua since tables already give us what we need

-- LEN - just use # operator on table
function M.len(codepoints)
    return #codepoints
end

-- REVERSE$ - reverse table
function M.reverse(codepoints)
    local result = {}
    for i = #codepoints, 1, -1 do
        result[#result + 1] = codepoints[i]
    end
    return result
end

-- LEFT$ - first n codepoints
function M.left(codepoints, n)
    local result = {}
    for i = 1, math.min(n, #codepoints) do
        result[i] = codepoints[i]
    end
    return result
end

-- RIGHT$ - last n codepoints
function M.right(codepoints, n)
    local result = {}
    local start = math.max(1, #codepoints - n + 1)
    for i = start, #codepoints do
        result[#result + 1] = codepoints[i]
    end
    return result
end

-- MID$ - substring
function M.mid(codepoints, start, len)
    local result = {}
    local end_pos = math.min(start + len - 1, #codepoints)
    for i = start, end_pos do
        result[#result + 1] = codepoints[i]
    end
    return result
end

-- STRING$ - repeat codepoint n times
function M.string_repeat(count, codepoint)
    local result = {}
    for i = 1, count do
        result[i] = codepoint
    end
    return result
end

-- SPACE$ - repeat space codepoint (32)
function M.space(count)
    return M.string_repeat(count, 32)
end

-- Concatenate two codepoint arrays
function M.concat(cp1, cp2)
    local result = {}
    for i = 1, #cp1 do
        result[#result + 1] = cp1[i]
    end
    for i = 1, #cp2 do
        result[#result + 1] = cp2[i]
    end
    return result
end

-- INSTR - find needle in haystack (2-arg version)
function M.instr(haystack, needle)
    return M.instr_start(1, haystack, needle)
end

-- INSTR - find needle in haystack starting at position (3-arg version)
function M.instr_start(start, haystack, needle)
    if #needle == 0 then
        return 0
    end

    for i = start, #haystack - #needle + 1 do
        local match = true
        for j = 1, #needle do
            if haystack[i + j - 1] ~= needle[j] then
                match = false
                break
            end
        end
        if match then
            return i
        end
    end

    return 0
end

-- TRIM$ - remove leading and trailing spaces
function M.trim(codepoints)
    if #codepoints == 0 then
        return {}
    end

    -- Find first non-space
    local start = 1
    while start <= #codepoints and M.is_space(codepoints[start]) do
        start = start + 1
    end

    if start > #codepoints then
        return {} -- All spaces
    end

    -- Find last non-space
    local finish = #codepoints
    while finish >= 1 and M.is_space(codepoints[finish]) do
        finish = finish - 1
    end

    -- Extract trimmed portion
    local result = {}
    for i = start, finish do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- LTRIM$ - remove leading spaces
function M.ltrim(codepoints)
    if #codepoints == 0 then
        return {}
    end

    local start = 1
    while start <= #codepoints and M.is_space(codepoints[start]) do
        start = start + 1
    end

    if start > #codepoints then
        return {}
    end

    local result = {}
    for i = start, #codepoints do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- RTRIM$ - remove trailing spaces
function M.rtrim(codepoints)
    if #codepoints == 0 then
        return {}
    end

    local finish = #codepoints
    while finish >= 1 and M.is_space(codepoints[finish]) do
        finish = finish - 1
    end

    if finish < 1 then
        return {}
    end

    local result = {}
    for i = 1, finish do
        result[#result + 1] = codepoints[i]
    end

    return result
end

-- CHR$ - create single-codepoint array
function M.chr(codepoint)
    return { codepoint }
end

-- ASC - get first codepoint
function M.asc(codepoints)
    if #codepoints == 0 then
        return 0
    end
    return codepoints[1]
end

-- Print info about unicode module
function M.info()
    print("Unicode Runtime Module")
    print("  Available: " .. tostring(available))
    if available then
        print("  Version: " .. M.version())
        print("  Unicode Standard: " .. M.standard_version())
    end
end

return M
