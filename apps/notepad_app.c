/* notepad_app.c - A simple Notepad application for zOS.
   Type text lines. Type ".exit" on a new line to finish editing.
   The final text is displayed on screen.
   
   This app uses the kernel's console I/O routines:
     - print_string(const char *str)
     - print_char(char c)
     - read_line(char *buffer, int max_length)
     - clear_screen(void)
     
   It assumes these functions are available as externs.
*/

extern void print_string(const char *str);
extern void print_char(char c);
extern void read_line(char *buffer, int max_length);
extern void clear_screen(void);

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void notepad_main(void) {
    char text[5120];  // A 5KB text buffer
    int pos = 0;
    text[0] = '\0';
    clear_screen();
    print_string("zOS Notepad Application\n");
    print_string("Enter text lines. Type \".exit\" to finish editing.\n");
    while (1) {
        print_string("> ");
        char line[256];
        read_line(line, 256);
        if (strcmp(line, ".exit") == 0)
            break;
        // Append the line into our text buffer
        int i = 0;
        while (line[i] && pos < 5119) {
            text[pos++] = line[i++];
        }
        if (pos < 5119) {
            text[pos++] = '\n';
        }
        text[pos] = '\0';
    }
    clear_screen();
    print_string("Notepad Content:\n");
    print_string(text);
    print_string("\nPress Enter to exit.");
    char dummy[16];
    read_line(dummy, 16);
}

void kmain(void) {
    notepad_main();
    while (1) { }
}
