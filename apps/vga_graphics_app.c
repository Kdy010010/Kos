/* vga_graphics_app.c - A simple VGA graphics application for zOS.
   This app switches to VGA mode 13h, draws a color gradient, waits a bit,
   and then restores text mode (03h). */

void set_mode_13h(void) {
    asm volatile (
        "mov $0x0013, %%ax\n"  // 0x13 = 320x200 256-color mode
        "int $0x10\n"
        :
        :
        : "ax"
    );
}

void set_mode_03h(void) {
    asm volatile (
        "mov $0x0003, %%ax\n"  // 0x03 = 80x25 text mode
        "int $0x10\n"
        :
        :
        : "ax"
    );
}

void draw_graphics(void) {
    // VGA mode 13h framebuffer starts at physical address 0xA0000.
    unsigned char *vga = (unsigned char *)0xA0000;
    int x, y;
    for (y = 0; y < 200; y++) {
        for (x = 0; x < 320; x++) {
            // Create a simple gradient based on x and y.
            // This will cycle through colors.
            vga[y * 320 + x] = (unsigned char)((x + y) & 0xFF);
        }
    }
}

void delay(void) {
    // A simple delay loop (adjust the count for a longer delay)
    volatile unsigned long count = 50000000;
    while (count--) { }
}

void kmain(void) {
    set_mode_13h();  // Switch to graphics mode
    draw_graphics(); // Draw a color gradient
    delay();         // Pause so you can see the graphics
    set_mode_03h();  // Switch back to text mode
    while (1) { }    // Halt
}
