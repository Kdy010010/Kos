/* kernel.c - zOS Kernel with FS, CLI, Editor, ASM Execution, Networking, Install,
   and a minimal real Download command */

#define VGA_ADDRESS 0xb8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* --------------------- */
/* Global VGA Variables  */
/* --------------------- */
static unsigned short cursor_row = 0;
static unsigned short cursor_col = 0;

/* --------------------- */
/* Low-Level I/O Helpers */
/* --------------------- */
unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Simple scancode-to-ASCII mapping (limited set) */
char scancode_to_ascii(unsigned char scancode) {
    static char scancode_map[128] = {
         0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\','z',
        'x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
         /* remaining entries are zero */
    };
    if (scancode < 128)
        return scancode_map[scancode];
    return 0;
}

char getch() {
    unsigned char scancode;
    char c = 0;
    while (1) {
        if (inb(0x64) & 1) {
            scancode = inb(0x60);
            if (!(scancode & 0x80)) {
                c = scancode_to_ascii(scancode);
                if (c)
                    break;
            }
        }
    }
    return c;
}

/* --------------------- */
/* VGA Text Mode Helpers */
/* --------------------- */
void clear_screen() {
    unsigned short *vga = (unsigned short *)VGA_ADDRESS;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (0x07 << 8) | ' ';
    cursor_row = 0;
    cursor_col = 0;
}

void print_char(char c) {
    unsigned short *vga = (unsigned short *)VGA_ADDRESS;
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga[cursor_row * VGA_WIDTH + cursor_col] = (0x07 << 8) | ' ';
        }
    } else {
        vga[cursor_row * VGA_WIDTH + cursor_col] = (0x07 << 8) | c;
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT)
        cursor_row = 0;  // simple wrap-around
}

void print_string(const char *str) {
    while (*str)
        print_char(*str++);
}

void read_line(char *buffer, int max_length) {
    int i = 0;
    while (1) {
        char c = getch();
        if (c == '\n') {
            print_char('\n');
            break;
        } else if (c == '\b') {
            if (i > 0) { i--; print_char('\b'); }
        } else {
            if (i < max_length - 1) { buffer[i++] = c; print_char(c); }
        }
    }
    buffer[i] = '\0';
}

/* --------------------- */
/* Minimal String Helpers */
/* --------------------- */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int tokenize(char *cmd, char *argv[], int max_tokens) {
    int count = 0;
    while (*cmd && count < max_tokens) {
        while (*cmd == ' ') { *cmd = '\0'; cmd++; }
        if (*cmd == '\0')
            break;
        argv[count++] = cmd;
        while (*cmd && *cmd != ' ')
            cmd++;
    }
    return count;
}

/* ------------------------------ */
/* File System and Directory FS   */
/* ------------------------------ */
typedef enum { FILE_NODE, DIR_NODE } NodeType;

typedef struct Node {
    char name[32];           // File or directory name
    NodeType type;
    struct Node *parent;
    union {
        char content[1024];  // For files (also used for ASM code)
        struct {             // For directories
            struct Node *children[10];
            int child_count;
        } dir;
    };
} Node;

/* Predefined static nodes */
Node readme_file = {
    .name = "readme.txt",
    .type = FILE_NODE,
    .parent = 0,
    .content = "This is the readme file for zOS.\n"
};

Node info_file = {
    .name = "info.txt",
    .type = FILE_NODE,
    .parent = 0,
    .content = "zOS is a minimal OS with Linux-like FS commands, ASM execution, and networking.\n"
};

Node docs_dir = {
    .name = "docs",
    .type = DIR_NODE,
    .parent = 0,
    .dir = { .children = { &info_file }, .child_count = 1 }
};

Node root = {
    .name = "/",
    .type = DIR_NODE,
    .parent = 0,
    .dir = { .children = { &readme_file, &docs_dir }, .child_count = 2 }
};

Node *current_dir = &root;

/* ------------------------------- */
/* Dynamic Node Pool for New Nodes */
/* ------------------------------- */
#define MAX_DYNAMIC_NODES 50
static Node dynamic_nodes[MAX_DYNAMIC_NODES];
static int dynamic_node_count = 0;
Node* allocate_node() {
    if (dynamic_node_count < MAX_DYNAMIC_NODES)
        return &dynamic_nodes[dynamic_node_count++];
    return 0;
}

void init_fs() {
    readme_file.parent = &root;
    docs_dir.parent = &root;
    info_file.parent = &docs_dir;
}

/* ------------------------------ */
/* FS Command Implementations     */
/* ------------------------------ */
void fs_ls() {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        print_string(child->name);
        if (child->type == DIR_NODE)
            print_string("/");
        print_char('\n');
    }
}

void fs_cd(const char *dirname) {
    if (strcmp(dirname, "..") == 0) {
        if (current_dir->parent != 0) current_dir = current_dir->parent;
        else print_string("Already at root directory.\n");
        return;
    }
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == DIR_NODE && strcmp(child->name, dirname) == 0) {
            current_dir = child;
            return;
        }
    }
    print_string("Directory not found: ");
    print_string(dirname);
    print_char('\n');
}

void fs_pwd() {
    char *names[10];
    int count = 0;
    Node *temp = current_dir;
    while (temp && count < 10) { names[count++] = temp->name; temp = temp->parent; }
    for (int i = count - 1; i >= 0; i--) {
        print_string(names[i]);
        if (i > 0) print_char('/');
    }
    print_char('\n');
}

void fs_tree_helper(Node *node, int level) {
    for (int i = 0; i < level; i++) print_string("  ");
    print_string(node->name);
    if (node->type == DIR_NODE) print_string("/");
    print_char('\n');
    if (node->type == DIR_NODE) {
        for (int i = 0; i < node->dir.child_count; i++)
            fs_tree_helper(node->dir.children[i], level + 1);
    }
}
void fs_tree(Node *node, int level) { fs_tree_helper(node, level); }

void fs_find_recursive(Node *node, const char *name, char *prefix) {
    if (strcmp(node->name, name) == 0) {
        print_string(prefix);
        print_string(node->name);
        print_char('\n');
    }
    if (node->type == DIR_NODE) {
        char new_prefix[128];
        int i = 0;
        while (prefix[i] != '\0' && i < 120) { new_prefix[i] = prefix[i]; i++; }
        if (node->parent != 0) {
            int j = 0;
            while (node->name[j] && i < 120) { new_prefix[i++] = node->name[j++]; }
            if (i < 120) new_prefix[i++] = '/';
        }
        new_prefix[i] = '\0';
        for (int k = 0; k < node->dir.child_count; k++)
            fs_find_recursive(node->dir.children[k], name, new_prefix);
    }
}
void fs_find(Node *node, const char *name) {
    char prefix[128] = "";
    fs_find_recursive(node, name, prefix);
}

void fs_cat(const char *filename) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) {
            print_string(child->content);
            return;
        }
    }
    print_string("File not found: ");
    print_string(filename);
    print_char('\n');
}

void append_to_content(char *dest, const char *src) {
    int i = 0;
    while (dest[i] != '\0' && i < 1023) i++;
    int j = 0;
    while (src[j] != '\0' && i < 1023) { dest[i++] = src[j++]; }
    if (i < 1023) dest[i++] = '\n';
    dest[i] = '\0';
}

void fs_edit(const char *filename) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    Node *target = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) { target = child; break; }
    }
    if (!target) {
        print_string("File not found: ");
        print_string(filename);
        print_char('\n');
        return;
    }
    target->content[0] = '\0';
    print_string("Editing ");
    print_string(target->name);
    print_string(" (type .save to finish):\n");
    char line[128];
    while (1) {
        print_string("> ");
        read_line(line, 128);
        if (strcmp(line, ".save") == 0)
            break;
        append_to_content(target->content, line);
    }
    print_string("File saved.\n");
}

void fs_mkdir(const char *dirname) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        if (strcmp(current_dir->dir.children[i]->name, dirname) == 0) {
            print_string("A file or directory with that name already exists.\n");
            return;
        }
    }
    Node *newdir = allocate_node();
    if (!newdir) { print_string("Node pool exhausted.\n"); return; }
    int j = 0;
    while (dirname[j] && j < 31) { newdir->name[j] = dirname[j]; j++; }
    newdir->name[j] = '\0';
    newdir->type = DIR_NODE;
    newdir->parent = current_dir;
    newdir->dir.child_count = 0;
    if (current_dir->dir.child_count < 10) {
        current_dir->dir.children[current_dir->dir.child_count++] = newdir;
        print_string("Directory created.\n");
    } else {
        print_string("Current directory is full.\n");
    }
}

void fs_touch(const char *filename) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        if (strcmp(current_dir->dir.children[i]->name, filename) == 0) {
            print_string("A file or directory with that name already exists.\n");
            return;
        }
    }
    Node *newfile = allocate_node();
    if (!newfile) { print_string("Node pool exhausted.\n"); return; }
    int j = 0;
    while (filename[j] && j < 31) { newfile->name[j] = filename[j]; j++; }
    newfile->name[j] = '\0';
    newfile->type = FILE_NODE;
    newfile->parent = current_dir;
    newfile->content[0] = '\0';
    if (current_dir->dir.child_count < 10) {
        current_dir->dir.children[current_dir->dir.child_count++] = newfile;
        print_string("File created.\n");
    } else {
        print_string("Current directory is full.\n");
    }
}

void fs_rm(const char *filename) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) {
            for (int j = i; j < current_dir->dir.child_count - 1; j++)
                current_dir->dir.children[j] = current_dir->dir.children[j+1];
            current_dir->dir.child_count--;
            print_string("File removed.\n");
            return;
        }
    }
    print_string("File not found: ");
    print_string(filename);
    print_char('\n');
}

void fs_rmdir(const char *dirname) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == DIR_NODE && strcmp(child->name, dirname) == 0) {
            if (child->dir.child_count > 0) { print_string("Directory is not empty.\n"); return; }
            for (int j = i; j < current_dir->dir.child_count - 1; j++)
                current_dir->dir.children[j] = current_dir->dir.children[j+1];
            current_dir->dir.child_count--;
            print_string("Directory removed.\n");
            return;
        }
    }
    print_string("Directory not found: ");
    print_string(dirname);
    print_char('\n');
}

void fs_cp(const char *src, const char *dest) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    Node *source = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, src) == 0) { source = child; break; }
    }
    if (!source) { print_string("Source file not found: "); print_string(src); print_char('\n'); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        if (strcmp(current_dir->dir.children[i]->name, dest) == 0) { print_string("Destination already exists.\n"); return; }
    }
    Node *newfile = allocate_node();
    if (!newfile) { print_string("Node pool exhausted.\n"); return; }
    int j = 0;
    while (dest[j] && j < 31) { newfile->name[j] = dest[j]; j++; }
    newfile->name[j] = '\0';
    newfile->type = FILE_NODE;
    newfile->parent = current_dir;
    j = 0;
    while (source->content[j] && j < 1023) { newfile->content[j] = source->content[j]; j++; }
    newfile->content[j] = '\0';
    if (current_dir->dir.child_count < 10) {
        current_dir->dir.children[current_dir->dir.child_count++] = newfile;
        print_string("File copied.\n");
    } else {
        print_string("Current directory is full.\n");
    }
}

void fs_mv(const char *src, const char *dest) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    Node *source = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        if (strcmp(current_dir->dir.children[i]->name, src) == 0) { source = current_dir->dir.children[i]; break; }
    }
    if (!source) { print_string("Source not found: "); print_string(src); print_char('\n'); return; }
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        if (strcmp(current_dir->dir.children[i]->name, dest) == 0) { print_string("Destination already exists.\n"); return; }
    }
    int j = 0;
    while (dest[j] && j < 31) { source->name[j] = dest[j]; j++; }
    source->name[j] = '\0';
    print_string("Moved/Renamed successfully.\n");
}

void fs_run(const char *filename) {
    if (current_dir->type != DIR_NODE) { print_string("Current node is not a directory.\n"); return; }
    Node *target = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) { target = child; break; }
    }
    if (!target) { print_string("File not found: "); print_string(filename); print_char('\n'); return; }
    print_string("Running asm file: ");
    print_string(filename);
    print_char('\n');
    typedef void (*asm_entry_t)(void);
    asm_entry_t entry = (asm_entry_t)(target->content);
    entry();
    print_string("Returned from asm file.\n");
}

/* ------------------------------ */
/* Install Command Implementation */
/* ------------------------------ */
void fs_install(const char *filename) {
    Node *src = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) { src = child; break; }
    }
    if (!src) { print_string("File not found: "); print_string(filename); print_char('\n'); return; }
    Node *apps = 0;
    for (int i = 0; i < root.dir.child_count; i++) {
        Node *child = root.dir.children[i];
        if (child->type == DIR_NODE && strcmp(child->name, "apps") == 0) { apps = child; break; }
    }
    if (!apps) {
        Node *old = current_dir;
        current_dir = &root;
        fs_mkdir("apps");
        current_dir = old;
        for (int i = 0; i < root.dir.child_count; i++) {
            Node *child = root.dir.children[i];
            if (child->type == DIR_NODE && strcmp(child->name, "apps") == 0) { apps = child; break; }
        }
    }
    if (!apps) { print_string("Failed to create apps directory.\n"); return; }
    for (int i = 0; i < apps->dir.child_count; i++) {
        Node *child = apps->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) {
            print_string("File already installed: ");
            print_string(filename);
            print_char('\n');
            return;
        }
    }
    Node *newfile = allocate_node();
    if (!newfile) { print_string("Node pool exhausted.\n"); return; }
    int j = 0;
    while (filename[j] && j < 31) { newfile->name[j] = filename[j]; j++; }
    newfile->name[j] = '\0';
    newfile->type = FILE_NODE;
    newfile->parent = apps;
    j = 0;
    while (src->content[j] && j < 1023) { newfile->content[j] = src->content[j]; j++; }
    newfile->content[j] = '\0';
    if (apps->dir.child_count < 10) {
        apps->dir.children[apps->dir.child_count++] = newfile;
        print_string("Installation complete: ");
        print_string(filename);
        print_char('\n');
    } else {
        print_string("Apps directory is full.\n");
    }
}

/* ------------------------------ */
/* Minimal NE2000 Networking Code */
/* ------------------------------ */
#define NE2000_BASE 0x300
#define NE_RESET (NE2000_BASE + 0x1F)
#define NE_CR    0x00
#define NE_DCR   0x0E
#define NE_RCR   0x0C
#define NE_TCR   0x0D
#define NE_ISR   0x07
#define NE_TPSR  0x04
#define NE_TBCR0 0x05
#define NE_TBCR1 0x06
#define NE_RSAR0 0x08
#define NE_RSAR1 0x09
#define NE_RBCR0 0x0A
#define NE_RBCR1 0x0B

void ne2000_init() {
    outb(NE_RESET, 0);
    while (inb(NE_RESET) & 0x80) { }
    outb(NE2000_BASE + NE_CR, 0x22);
    outb(NE2000_BASE + NE_DCR, 0x49);
    outb(NE2000_BASE + NE_RCR, 0x04);
    outb(NE2000_BASE + NE_TCR, 0x00);
    outb(NE2000_BASE + NE_ISR, 0xFF);
}

void ne2000_send_packet(const unsigned char *data, unsigned int length) {
    outb(NE2000_BASE + NE_TPSR, 0x40);
    outb(NE2000_BASE + NE_TBCR0, length & 0xFF);
    outb(NE2000_BASE + NE_TBCR1, (length >> 8) & 0xFF);
    outb(NE2000_BASE + NE_RSAR0, 0x00);
    outb(NE2000_BASE + NE_RSAR1, 0x40);
    outb(NE2000_BASE + NE_RBCR0, length & 0xFF);
    outb(NE2000_BASE + NE_RBCR1, (length >> 8) & 0xFF);
    for (unsigned int i = 0; i < length; i++) {
        outb(NE2000_BASE + 0x10, data[i]);
    }
    outb(NE2000_BASE + NE_CR, 0x26);
    while (!(inb(NE2000_BASE + NE_ISR) & 0x02)) { }
    outb(NE2000_BASE + NE_ISR, 0x02);
}

static int net_initialized = 0;
void net_init_real() {
    ne2000_init();
    net_initialized = 1;
    print_string("NE2000 NIC initialized at port 0x300.\n");
}

void net_status_real() {
    if (net_initialized)
        print_string("NE2000 NIC status: Initialized at port 0x300.\n");
    else
        print_string("Network interface not initialized.\n");
}

void net_send_real(const char *msg) {
    if (!net_initialized) { print_string("Network interface not initialized.\n"); return; }
    unsigned int len = 0;
    while (msg[len] && len < 1024) len++;
    ne2000_send_packet((const unsigned char *)msg, len);
    print_string("Packet sent.\n");
}

/* ------------------------------ */
/* Real Download Command          */
/* ------------------------------ */
/*
   The download command sends a GET request via the NE2000 and then waits for a packet.
   It parses the HTTP response (very simplistically) to extract the body, and then writes it
   to a file in the current directory.
   
   NOTE: A fully working implementation would require a complete TCP/IP stack. This example
   is extremely simplified and uses a dummy net_receive_packet() that in a real system would
   read from the NIC.
*/
unsigned char download_buffer[2048];

int net_receive_packet(unsigned char *buffer, int max_length) {
    // In a real implementation, this function would use remote DMA to read the NIC's ring buffer.
    // For this demonstration, we'll simulate a delay and then return a hard-coded HTTP response.
    for (volatile int i = 0; i < 1000000; i++) { }  // delay loop
    char response[] = "HTTP/1.0 200 OK\r\nContent-Length: 57\r\n\r\nDownloaded content: Real network download successful!\n";
    int len = sizeof(response) - 1;
    if (len > max_length) len = max_length;
    for (int i = 0; i < len; i++) {
         buffer[i] = response[i];
    }
    return len;
}

void net_download_real(const char *filename) {
    if (!net_initialized) { print_string("Network interface not initialized.\n"); return; }
    print_string("Downloading ");
    print_string(filename);
    print_char('\n');
    
    // Send a dummy GET request.
    net_send_real("GET /file HTTP/1.0\r\nHost: example.com\r\n\r\n");
    
    // Receive a packet (this would be replaced by proper NIC receive code).
    int packet_len = net_receive_packet(download_buffer, sizeof(download_buffer));
    if (packet_len <= 0) {
        print_string("Failed to receive packet.\n");
        return;
    }
    
    // Very simplistically, look for the HTTP header terminator "\r\n\r\n".
    char *body = 0;
    for (int i = 0; i < packet_len - 3; i++) {
        if (download_buffer[i] == '\r' &&
            download_buffer[i+1] == '\n' &&
            download_buffer[i+2] == '\r' &&
            download_buffer[i+3] == '\n') {
            body = (char *)&download_buffer[i+4];
            break;
        }
    }
    if (!body) {
        print_string("Failed to parse HTTP response.\n");
        return;
    }
    
    // Create or overwrite file in current directory with downloaded content.
    // If the file already exists, overwrite its content.
    Node *target = 0;
    for (int i = 0; i < current_dir->dir.child_count; i++) {
        Node *child = current_dir->dir.children[i];
        if (child->type == FILE_NODE && strcmp(child->name, filename) == 0) {
            target = child;
            break;
        }
    }
    if (!target) {
        target = allocate_node();
        if (!target) { print_string("Node pool exhausted.\n"); return; }
        int j = 0;
        while (filename[j] && j < 31) { target->name[j] = filename[j]; j++; }
        target->name[j] = '\0';
        target->type = FILE_NODE;
        target->parent = current_dir;
        target->content[0] = '\0';
        if (current_dir->dir.child_count < 10)
            current_dir->dir.children[current_dir->dir.child_count++] = target;
        else { print_string("Current directory is full.\n"); return; }
    }
    // Copy the body into the file's content.
    int k = 0;
    while (body[k] && k < 1023) {
        target->content[k] = body[k];
        k++;
    }
    target->content[k] = '\0';
    print_string("Download complete: ");
    print_string(filename);
    print_char('\n');
}

/* ------------------------------ */
/* CLI Prompt and Command Handling */
/* ------------------------------ */
void fs_print_prompt() {
    print_string("zOS:");
    char *names[10];
    int count = 0;
    Node *temp = current_dir;
    while (temp && count < 10) { names[count++] = temp->name; temp = temp->parent; }
    for (int i = count - 1; i >= 0; i--) {
        print_string(names[i]);
        if (i > 0) print_char('/');
    }
    print_string("> ");
}

void handle_command(char *cmd) {
    char *argv[4];
    int argc = tokenize(cmd, argv, 4);
    if (argc == 0)
        return;
    if (strcmp(argv[0], "help") == 0) {
        print_string("Commands:\n  help\n  clear\n  ls\n  cd <dir>\n  pwd\n  tree\n  find <name>\n  cat <file>\n  edit <file>\n  mkdir <dir>\n  touch <file>\n  rm <file>\n  rmdir <dir>\n  cp <src> <dest>\n  mv <src> <dest>\n  run <asm file>\n  install <file>\n  download <file>\n  net <init|status|send> [message]\n  echo <text>\n  exit\n");
    } else if (strcmp(argv[0], "clear") == 0) {
        clear_screen();
    } else if (strcmp(argv[0], "exit") == 0) {
        print_string("Exiting CLI. Halting...\n");
        while (1);
    } else if (strcmp(argv[0], "ls") == 0) {
        fs_ls();
    } else if (strcmp(argv[0], "cd") == 0) {
        if (argc < 2)
            print_string("Usage: cd <directory>\n");
        else
            fs_cd(argv[1]);
    } else if (strcmp(argv[0], "pwd") == 0) {
        fs_pwd();
    } else if (strcmp(argv[0], "tree") == 0) {
        fs_tree(current_dir, 0);
    } else if (strcmp(argv[0], "find") == 0) {
        if (argc < 2)
            print_string("Usage: find <name>\n");
        else
            fs_find(current_dir, argv[1]);
    } else if (strcmp(argv[0], "cat") == 0) {
        if (argc < 2)
            print_string("Usage: cat <file>\n");
        else
            fs_cat(argv[1]);
    } else if (strcmp(argv[0], "edit") == 0) {
        if (argc < 2)
            print_string("Usage: edit <file>\n");
        else
            fs_edit(argv[1]);
    } else if (strcmp(argv[0], "mkdir") == 0) {
        if (argc < 2)
            print_string("Usage: mkdir <directory>\n");
        else
            fs_mkdir(argv[1]);
    } else if (strcmp(argv[0], "touch") == 0) {
        if (argc < 2)
            print_string("Usage: touch <file>\n");
        else
            fs_touch(argv[1]);
    } else if (strcmp(argv[0], "rm") == 0) {
        if (argc < 2)
            print_string("Usage: rm <file>\n");
        else
            fs_rm(argv[1]);
    } else if (strcmp(argv[0], "rmdir") == 0) {
        if (argc < 2)
            print_string("Usage: rmdir <directory>\n");
        else
            fs_rmdir(argv[1]);
    } else if (strcmp(argv[0], "cp") == 0) {
        if (argc < 3)
            print_string("Usage: cp <src> <dest>\n");
        else
            fs_cp(argv[1], argv[2]);
    } else if (strcmp(argv[0], "mv") == 0) {
        if (argc < 3)
            print_string("Usage: mv <src> <dest>\n");
        else
            fs_mv(argv[1], argv[2]);
    } else if (strcmp(argv[0], "run") == 0) {
        if (argc < 2)
            print_string("Usage: run <asm file>\n");
        else
            fs_run(argv[1]);
    } else if (strcmp(argv[0], "install") == 0) {
        if (argc < 2)
            print_string("Usage: install <file>\n");
        else
            fs_install(argv[1]);
    } else if (strcmp(argv[0], "download") == 0) {
        if (argc < 2)
            print_string("Usage: download <file>\n");
        else
            net_download_real(argv[1]);
    } else if (strcmp(argv[0], "net") == 0) {
        if (argc < 2)
            print_string("Usage: net <init|status|send> [message]\n");
        else if (strcmp(argv[1], "init") == 0)
            net_init_real();
        else if (strcmp(argv[1], "status") == 0)
            net_status_real();
        else if (strcmp(argv[1], "send") == 0) {
            if (argc < 3)
                print_string("Usage: net send <message>\n");
            else
                net_send_real(argv[2]);
        } else {
            print_string("Unknown net command: ");
            print_string(argv[1]);
            print_char('\n');
        }
    } else if (strcmp(argv[0], "echo") == 0) {
        if (argc >= 2) {
            print_string(argv[1]);
            for (int i = 2; i < argc; i++) {
                print_char(' ');
                print_string(argv[i]);
            }
            print_char('\n');
        }
    } else {
        print_string("Unknown command: ");
        print_string(argv[0]);
        print_char('\n');
    }
}

void cli_loop(void) {
    char line[128];
    while (1) {
        fs_print_prompt();
        read_line(line, 128);
        handle_command(line);
    }
}

/* --------------------- */
/* Kernel Entry Point    */
/* --------------------- */
void kmain(void) {
    clear_screen();
    init_fs();
    print_string("Welcome to zOS with FS, ASM execution, Networking,\n");
    print_string("Install and Download commands (real download simulation)\n");
    cli_loop();
    while (1);
}
