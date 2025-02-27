/* file_browser.c - A simple text-based file browser for zOS */

typedef enum { FILE_NODE, DIR_NODE } NodeType;

typedef struct Node {
    char name[32];           // File or directory name
    NodeType type;
    struct Node *parent;
    union {
        char content[1024];  // For files (or executable code)
        struct {
            struct Node *children[10];
            int child_count;
        } dir;
    };
} Node;

/* 
   These functions are provided by the zOS kernel:
     - print_string: prints a null-terminated string to the screen.
     - print_char: prints a single character.
     - read_line: reads a line of text input.
     - clear_screen: clears the screen.
   They are available for use by applications.
*/
extern void print_string(const char *str);
extern void print_char(char c);
extern void read_line(char *buffer, int max_length);
extern void clear_screen(void);

/* The global file system pointer from the kernel.
   (This points to the current directory in the OS file system.)
*/
extern Node *current_dir;

/* A simple implementation of atoi (string-to-integer conversion) */
int simple_atoi(const char *s) {
    int num = 0;
    while (*s) {
        if (*s < '0' || *s > '9')
            break;
        num = num * 10 + (*s - '0');
        s++;
    }
    return num;
}

/* The file browser main loop.
   It uses its own local pointer "cur" to track the directory being browsed.
*/
void file_browser(void) {
    char input[32];
    Node *cur = current_dir;  // Start at the current directory
    while (1) {
        clear_screen();
        print_string("File Browser - Current Directory: ");
        print_string(cur->name);
        print_string("\n----------------------\n");

        // List entries with their indexes
        int i;
        for (i = 0; i < cur->dir.child_count; i++) {
            Node *child = cur->dir.children[i];
            print_string(" [");
            /* For simplicity, convert the index (0..9) to a character. */
            print_char('0' + i);
            print_string("] ");
            print_string(child->name);
            if (child->type == DIR_NODE)
                print_string(" (dir)");
            print_string("\n");
        }
        print_string("----------------------\n");
        print_string("Enter index to open file/dir, '..' to go up, 'q' to quit: ");
        read_line(input, 32);

        // Process input:
        if (input[0] == 'q') {
            break;
        } else if (input[0] == '.' && input[1] == '.') {
            // Go up one level if possible
            if (cur->parent)
                cur = cur->parent;
            else
                print_string("Already at root directory.\n");
        } else {
            // Assume the input is a number
            int index = simple_atoi(input);
            if (index < 0 || index >= cur->dir.child_count) {
                print_string("Invalid index.\n");
            } else {
                Node *child = cur->dir.children[index];
                if (child->type == DIR_NODE) {
                    cur = child;
                } else {
                    // For a file, display its content
                    clear_screen();
                    print_string("Viewing file: ");
                    print_string(child->name);
                    print_string("\n----------------------\n");
                    print_string(child->content);
                    print_string("\n----------------------\n");
                    print_string("Press Enter to return.");
                    read_line(input, 32);
                }
            }
        }
    }
}

/* The app's entry point.
   When the kernel loads this application, it jumps to kmain.
*/
void kmain(void) {
    file_browser();
    while (1) { }  // Halt when done.
}
