/* calc_app.c - A simple Calculator application for zOS.
   Enter an expression of the form "a op b" (e.g., "3 + 4")
   and the app computes and displays the result.
*/

extern void print_string(const char *str);
extern void print_char(char c);
extern void read_line(char *buffer, int max_length);
extern void clear_screen(void);

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void calc_main(void) {
    clear_screen();
    print_string("Simple Calculator\n");
    print_string("Enter expression (a op b), e.g., 3 + 4:\n> ");
    char input[64];
    read_line(input, 64);
    int i = 0, a = 0, b = 0;
    // Parse first number
    while (input[i] >= '0' && input[i] <= '9') {
        a = a * 10 + (input[i] - '0');
        i++;
    }
    while (input[i] == ' ') i++;
    char op = input[i++];
    while (input[i] == ' ') i++;
    // Parse second number
    while (input[i] >= '0' && input[i] <= '9') {
        b = b * 10 + (input[i] - '0');
        i++;
    }
    int result = 0;
    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b == 0) { print_string("Divide by zero error.\n"); return; }
        result = a / b;
    } else { print_string("Unknown operator.\n"); return; }
    print_string("Result: ");
    // Convert result to string (simple conversion)
    char buf[16];
    int pos = 0;
    int temp = result;
    if (temp < 0) { buf[pos++] = '-'; temp = -temp; }
    char digits[16];
    int d = 0;
    if (temp == 0) digits[d++] = '0';
    while (temp > 0) {
        digits[d++] = '0' + (temp % 10);
        temp /= 10;
    }
    for (int j = d - 1; j >= 0; j--) { buf[pos++] = digits[j]; }
    buf[pos] = '\0';
    print_string(buf);
    print_string("\nPress Enter to exit.");
    char dummy[16];
    read_line(dummy, 16);
}

void kmain(void) {
    calc_main();
    while (1) { }
}
