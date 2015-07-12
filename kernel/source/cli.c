// This is a temporary hack so that we can interact with nox before
// we've fleshed out a user-mode and stuff
#include <types.h>
#include <kernel.h>
#include <keyboard.h>
#include <terminal.h>
#include <debug.h>
#include <arch/x86/cpu.h>
#include <string.h>
#include <fs.h>
#include <elf.h>

#define MAX_COMMAND_SIZE 1024
#define COMMAND_BUFFER_SIZE (MAX_COMMAND_SIZE + 1)
#define MAX_ARGS 32
#define INPUT_BUFFER_SIZE 256

// Forward declarations
static size_t read_line(char* buffer, size_t buffer_size);
static void dispatch_command(char** args, size_t arg_count);
static size_t parse_command(char* buffer, size_t buffer_size, char** args, size_t args_size);
static void cli_key_up(enum keys key);
static void cli_key_down(enum keys key);

// Globals
static size_t g_input_read_index;
static size_t g_input_write_index;
static char g_input_buffer[INPUT_BUFFER_SIZE];

static bool g_shift_pressed;

static struct kb_subscriber g_subscriber = {
    .up = cli_key_up,
    .down = cli_key_down
};

// Exports
void cli_init() {
    for(size_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
        g_input_buffer[i] = -1;
    }
}

void cli_run()
{
    kb_subscribe(&g_subscriber);

    char command_buffer[COMMAND_BUFFER_SIZE];

    while (true) {

        // Read a line, leaving one character for a space at the end
        size_t character_count = read_line(command_buffer, COMMAND_BUFFER_SIZE - 1);

        // Add a space at the end so the parser has less work to do
        command_buffer[character_count] = ' ';
        character_count += 1;

        char* args[MAX_ARGS];
        size_t arg_len = parse_command(command_buffer, character_count, args, MAX_ARGS);

        dispatch_command(args, arg_len);
    }
}

static void cli_key_down(enum keys key)
{
    if(key == keys_lshift || key == keys_rshift) {
        g_shift_pressed = true;
    }
}

static void cli_key_up(enum keys key)
{
    if(key == keys_lshift || key == keys_rshift) {
        g_shift_pressed = false;
        return;
    }

    // Is this key printable?
    char c = kb_key_to_ascii(key);
    if(c >= 0) {
        if(g_shift_pressed && c >= 'a' && c <= 'z') {
                c -= 0x20;
        }
        else if(g_shift_pressed && c == '\'') {
            c = '"';
        }

        // Add key to command buffer
        g_input_buffer[g_input_write_index++] = c;
        if(g_input_write_index == INPUT_BUFFER_SIZE) {
            g_input_write_index = 0;
        }
    }
}

static char read_character(bool eat)
{
    // do do do do do do
    while (g_input_buffer[g_input_read_index] < 0) {
    }

    char ch = g_input_buffer[g_input_read_index];

    if (!eat) {
        terminal_write_char(ch);
    }

    g_input_buffer[g_input_read_index] = -1;

    if (++g_input_read_index >= INPUT_BUFFER_SIZE) {
        g_input_read_index = 0;
    }

    return ch;
}

static size_t read_line(char* buffer, size_t buffer_size)
{
    terminal_write_string("nox> ");

    size_t len = 0;

    while (len < buffer_size) {
        char ch = read_character(false);

        if (ch == '\n') {
            return len;
        }
        else {
            buffer[len++] = ch;
        }
    }

    // The buffer is full, refuse to take any more
    // input
    while ('\n' != read_character(true)) {
    }

    return len;
}

static void print_invalid_command(char* args[], size_t arg_count)
{
    terminal_write_string("Unknown command '");
    terminal_write_string(args[0]);

    if(arg_count == 1)
    {
        terminal_write_string("'\n");
        return;
    }

    terminal_write_string("', with arguments:\n");

    for (int i = 1; i < arg_count; i++) {
        terminal_write_string("    [");
        terminal_write_uint32(i - 1);
        terminal_write_string("] ");
        terminal_write_string(args[i]);
        terminal_write_string("\n");
    }
}

static void dispatch_command(char* args[], size_t arg_count)
{
    if(arg_count == 0) {
        terminal_write_string("Uhm, not sure what you're trying to do there son...\n");
        return;
    }

    if(kstrcmp(args[0], "reset")) {
        cpu_reset();
    }
    else if(kstrcmp(args[0], "clear")) {
        terminal_clear();
    }
    else if(kstrcmp(args[0], "cat")) {
        if(arg_count < 2) {
            KERROR("Expected at least one argument!");
            return;
        }
        fs_cat(args[1]);
    }
    else if(kstrcmp(args[0], "elf")) {
        if(arg_count < 2) {
            KERROR("Expected at least one argument");
            return;
        }
        elf_info(args[1]);
    }
    else if(kstrcmp(args[0], "run")) {
        if(kstrcmp(args[1], "KERNEL  ELF")) {
            KWARN("That is a SERIOUSLY bad idea!");
            return;
        }
        elf_run(args[1]);
    }
    else if(kstrcmp(args[0], "help")) {
        terminal_write_string("These are the things you can do!\n");
        terminal_write_string("reset - Restarts the computer\n");
        terminal_write_string("clear - Clears the screen\n");
        terminal_write_string("cat <file> - Show file content\n");
        terminal_write_string("elf <file  - Prints file info\n");
        terminal_write_string("run <file> - Runs the given program\n");
    }
    else {
        print_invalid_command(args, arg_count);
    }
}

static size_t parse_command(char* buffer, size_t buffer_size, char** args, size_t args_size)
{
    // This isn't at all production code, ffs, don't use it people, we could
    // do a better job, but we've got no malloc yet, so just fuck off and stop
    // complaining
    char* arg_start = buffer;
    size_t arg_count = 0;
    bool in_str = false;

    // TODO: This currently does NOT deal with arguments lacking
    //       a terminating quote.
    //
    //       The string below will therefore result in 0 args
    //       "hello, World
    //
    for (int i = 0; i < buffer_size; i++) {
        switch(buffer[i]) {

            case '"': {
                if(!in_str) {
                    // Found the first quote
                    in_str = true;
                    arg_start++; // Skip first quote in result string
                    continue; // Move to the next char
                }
                else {
                    // We found the terminating quote
                    in_str = false;
                }
                break;
            }
            case ' ': {
                if(!in_str) {

                    // End of argument
                    char* arg_end = &buffer[i];
                    if(buffer[i - 1] == '"')
                        arg_end--; // Don't include the quote :)

                    if (arg_end == arg_start) {
                        arg_start++;
                        continue;
                    }

                    *arg_end = '\0';
                    args[arg_count++] = arg_start;

                    arg_start = arg_end + 1;

                }
                break;
            }
        }
    }

    return arg_count;
}
