/* vga_circle_app.c - VGA Graphics App that draws a circle */

void set_mode_13h(void) {
    asm volatile (
        "mov $0x0013, %%ax\n"  // Mode 13h: 320x200 256-color
        "int $0x10\n"
        : : : "ax"
    );
}

void set_mode_03h(void) {
    asm volatile (
        "mov $0x0003, %%ax\n"  // Mode 03h: 80x25 text mode
        "int $0x10\n"
        : : : "ax"
    );
}

void delay(void) {
    volatile unsigned long count = 50000000;
    while (count--) { }
}

void draw_circle(int center_x, int center_y, int radius, unsigned char color) {
    // In VGA mode 13h, the framebuffer starts at physical address 0xA0000.
    unsigned char *vga = (unsigned char *)0xA0000;
    int x, y;
    // Pre-calculate the square of the radius and a small band around it.
    int inner_sq = (radius - 1) * (radius - 1);
    int outer_sq = (radius + 1) * (radius + 1);
    
    // Loop over all pixels in the 320x200 screen.
    for (y = 0; y < 200; y++) {
        for (x = 0; x < 320; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            int dist_sq = dx * dx + dy * dy;
            // If the pixel's distance squared is within a small band around the radius,
            // draw it as part of the circle's outline.
            if (dist_sq >= inner_sq && dist_sq <= outer_sq) {
                vga[y * 320 + x] = color;
            }
        }
    }
}

void kmain(void) {
    set_mode_13h();  // Switch to VGA graphics mode (320x200, 256-color)

    // Clear the screen (fill with color 0)
    unsigned char *vga = (unsigned char *)0xA0000;
    for (int i = 0; i < 320 * 200; i++) {
        vga[i] = 0;
    }
    
    // Draw a red circle at the center of the screen (160,100) with a radius of 50 pixels.
    // In VGA mode 13h, color 4 is typically a red color.
    draw_circle(160, 100, 50, 4);
    
    delay();         // Pause so the circle can be seen
    set_mode_03h();  // Switch back to text mode
    while (1);       // Halt
}
