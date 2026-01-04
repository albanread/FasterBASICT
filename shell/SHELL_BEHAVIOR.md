# FasterBASIC Shell (fbsh) - Interactive Behavior Specification

This document describes the exact behavior of the FasterBASIC interactive shell, as implemented in `shell_core.cpp`. This is the canonical specification for how the interactive mode should work.

## Core Loop

The shell operates in a simple read-eval-print loop:

```cpp
while (m_running) {
    showPrompt();           // 1. Show prompt or line number
    std::string input = readInput();  // 2. Read user input
    if (!input.empty()) {
        executeCommand(input);  // 3. Execute the command/line
    }
}
```

**Key principle:** Prompt and input are SEPARATE. The prompt is OUTPUT (printed to screen), the input is what the user types AFTER the prompt.

## Prompt Behavior

### Normal Mode (Ready Prompt)
```
Ready.
█                    <- cursor on NEXT line, user types here
```

The prompt "Ready." is printed on its own line, then the cursor moves to the beginning of the NEXT line where the user types.

### AUTO Mode (Line Number Prompt)
```
10 █                 <- cursor immediately after line number and space
```

In AUTO mode, the line number and space are printed WITHOUT a newline, so the cursor is positioned immediately after the space on the SAME line. The user types their code right after the line number.

### After Command Execution

After most commands, the shell prints:
```
Ready.
```

After program line entry (numbered line), NO "Ready." is printed. The next prompt appears immediately.

## AUTO Mode Behavior

### Entering AUTO Mode
```
Ready.
AUTO                 <- user types
Automatic line numbering enabled
10 █                 <- first line number appears, cursor after it
```

### Typing Lines in AUTO Mode
```
10 REM First line   <- user types after "10 "
20 █                 <- next line number appears automatically
PRINT "Hello"        <- user types
30 █                 <- continues...
```

### Exiting AUTO Mode

**Empty line:** Press Enter on empty line (just the line number, nothing typed)
```
40                   <- user just presses Enter without typing
Ready.
█
```

**Command:** Type any shell command (LIST, RUN, etc.) - AUTO mode automatically exits
```
50 LIST             <- user types a command instead of code
10 REM First line
20 PRINT "Hello"
30 PRINT "World"
Ready.
█
```

**Manual line number:** Type your own line number
```
50 100 PRINT "Jump"  <- user types different line number
Ready.                <- AUTO mode exits
█
```

## AUTO-CONTINUATION Mode

This is DIFFERENT from AUTO mode. Auto-continuation happens automatically when you enter a numbered line WITHOUT being in AUTO mode.

### Entering AUTO-CONTINUATION
```
Ready.
1000 REM First line  <- user types a numbered line
1010 █               <- next line number appears automatically!
```

The shell calculates the next available line number (typically +10 from the last line entered) and shows it ready for input.

### Typing Lines in AUTO-CONTINUATION
```
1000 REM First line  <- user types
1010 PRINT "Hello"   <- types next line
1020 █               <- continues automatically
FOR I = 1 TO 10      <- types next line
1030 █               <- continues...
```

### Exiting AUTO-CONTINUATION

**Empty line:** Press Enter without typing anything
```
1040                 <- user just presses Enter
Ready.
█
```

**Command:** Type any command
```
1040 LIST            <- user types command
1000 REM First line
1010 PRINT "Hello"
1020 FOR I = 1 TO 10
Ready.
█
```

**Manual line number:** Type a different line number
```
1040 100 PRINT "A"   <- user types different line number
Ready.               <- exits auto-continuation
█
```

### AUTO vs AUTO-CONTINUATION

| Feature | AUTO Mode | AUTO-CONTINUATION Mode |
|---------|-----------|------------------------|
| Activation | Type `AUTO` command | Automatic after entering numbered line |
| Line numbers | Sequential (10, 20, 30...) | Calculated (+10 from last) |
| User control | Must manually enter AUTO | Happens automatically |
| Typical use | Writing new program from scratch | Adding lines to existing program |

## Command Execution Flow

### Program Line Entry (Numbered Line)

**WITH AUTO-CONTINUATION** (automatic after first numbered line):
```
Ready.
1000 PRINT "Hello"   <- user types
1010 █               <- next line number appears automatically, NO "Ready."
```

**After empty line** (exits auto-continuation):
```
1020                 <- user presses Enter without typing
Ready.               <- back to Ready prompt
█
```

### Shell Command
```
Ready.
LIST                 <- user types
10 PRINT "Hello"     <- output
Ready.               <- back to prompt
█
```

### Invalid Command
```
Ready.
FOOBAR               <- user types
Invalid command: FOOBAR
Ready.
█
```

## Key Commands

### LIST
```
Ready.
LIST
10 REM Program
20 PRINT "Hello"
30 END

Ready.
█
```

### RUN
```
Ready.
RUN
Hello                <- program output
Ready.               <- program finished
█
```

### NEW
```
Ready.
NEW
Program cleared
Ready.
█
```

### CLS
```
Ready.
CLS
█                    <- screen cleared, cursor at top
```

### LOAD "filename"
```
Ready.
LOAD "test.bas"
Program loaded
Ready.
█
```

### SAVE "filename"
```
Ready.
SAVE "test.bas"
Program saved
Ready.
█
```

## Line Editing During Input

While typing (before pressing Enter):
- **Backspace**: Delete character before cursor
- **Left/Right Arrow**: Move cursor within line
- **Up Arrow**: Previous command from history
- **Down Arrow**: Next command from history
- **Home**: Move to start of line
- **End**: Move to end of line

## History

The shell maintains command history for:
- Shell commands (LIST, RUN, etc.)
- Program lines are NOT in history

Navigate history with Up/Down arrows while at the prompt.

## Output vs Input

**Critical distinction:**
- **Output** includes: prompts, command results, error messages, program output
- **Input** is: what the user types on the line AFTER the prompt

In a GUI implementation:
- Output goes into a scrollback buffer
- Input is in a separate editable line
- Prompt is part of the output, not part of the input

## AUTO Mode State Management

The ProgramManager tracks AUTO mode state:
```cpp
bool isAutoMode()              // Check if AUTO is active
int getNextAutoLine()          // Get next line number to use
void incrementAutoLine()       // Move to next line number
void setAutoMode(bool enable, int start = 10, int step = 10)
```

When in AUTO mode:
1. `showPrompt()` prints the line number
2. User types code (just the code, not the line number)
3. `handleDirectLine()` receives the line with number already parsed
4. ProgramManager increments to next line number
5. Loop continues

## IMMEDIATE Mode (Not Supported in Compiled BASIC)

Traditional BASIC interpreters allow:
```
Ready.
PRINT 2+2
4
Ready.
```

**FasterBASIC does NOT support this** because it's a compiled BASIC, not an interpreter. All code must be in numbered lines and compiled as a program.

If user tries to execute BASIC statements without line numbers, show error:
```
Ready.
PRINT "Hello"
ERROR: Cannot execute BASIC statements directly
Use line numbers: 10 PRINT "Hello"
Ready.
```

## Error Handling

Errors are printed immediately, then "Ready." prompt appears:
```
Ready.
1000000 PRINT "Bad"   <- line number out of range
ERROR: Line number must be between 1 and 65535
Ready.
█
```

## Empty Input

Pressing Enter on empty line just shows the prompt again:
```
Ready.
                      <- user presses Enter
Ready.
█
```

## Implementation Notes for GUI

When implementing in a GUI (like FBRunner3):

1. **Separate output buffer from input line**
   - Output buffer: scrolling text area (readonly)
   - Input line: single editable line at bottom

2. **Prompt handling**
   - "Ready." goes into output buffer
   - In AUTO mode, line number goes into INPUT buffer, cursor after it
   - User types in input line
   - On Enter, input moves to output buffer, command executes

3. **Rendering**
   - Show output buffer (with scrolling)
   - Show input line with cursor
   - Cursor position matters in AUTO mode!

4. **CommandParser integration**
   - Use `CommandParser::parse()` to parse input
   - Handle all ShellCommandType cases
   - Don't try to execute IMMEDIATE mode

5. **ProgramManager integration**
   - All program lines go through ProgramManager
   - Use `setLine()`, `deleteLine()`, `getAllLines()`
   - Let ProgramManager handle AUTO mode state