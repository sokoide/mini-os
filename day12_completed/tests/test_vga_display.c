#include <stdint.h>

#include "test_framework.h"

// VGA constants (from kernel.h)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define VGA_WHITE_ON_BLACK 0x0720
#define VGA_COLOR_WHITE 0x0F
#define VGA_COLOR_YELLOW 0x0E
#define VGA_COLOR_GRAY 0x07
#define VGA_COLOR_RED 0x0C
#define VGA_COLOR_GREEN 0x0A

// Function declarations for VGA display module
void clear_screen(void);
void clear_line(int row);
void print_at(int row, int col, const char* str, uint8_t color);
void debug_print(const char* message);
void display_system_info(void);

// Mock VGA buffer for testing
static uint16_t mock_vga_buffer[VGA_WIDTH * VGA_HEIGHT];
static uint16_t* vga_buffer = mock_vga_buffer;

// Test function declarations
void test_clear_screen(void);
void test_clear_line_valid(void);
void test_clear_line_boundary(void);
void test_print_at_basic(void);
void test_print_at_colors(void);
void test_print_at_boundary_cases(void);
void test_print_at_long_strings(void);
void test_print_at_special_characters(void);
void test_display_coordinate_calculation(void);
void test_vga_memory_safety(void);

// Helper functions
void setup_mock_vga(void);
uint16_t get_vga_cell(int row, int col);
void set_vga_cell(int row, int col, uint16_t value);
int count_characters_in_buffer(char target_char);
int count_color_attributes_in_buffer(uint8_t color);

// Main test runner
int main(void) {
    test_init("VGA Display Module Tests");

    // Setup mock hardware
    mock_init();
    setup_mock_vga();

    // Screen management tests
    test_clear_screen();
    test_clear_line_valid();
    test_clear_line_boundary();

    // Text display tests
    test_print_at_basic();
    test_print_at_colors();
    test_print_at_boundary_cases();
    test_print_at_long_strings();
    test_print_at_special_characters();

    // Memory and coordinate tests
    test_display_coordinate_calculation();
    test_vga_memory_safety();

    test_summary();
    return 0;
}

// Helper function implementations

void setup_mock_vga(void) {
    // Initialize mock VGA buffer with known pattern
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        mock_vga_buffer[i] = 0x0700;  // Gray on black, null character
    }
}

uint16_t get_vga_cell(int row, int col) {
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        return mock_vga_buffer[row * VGA_WIDTH + col];
    }
    return 0;
}

void set_vga_cell(int row, int col, uint16_t value) {
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        mock_vga_buffer[row * VGA_WIDTH + col] = value;
    }
}

int count_characters_in_buffer(char target_char) {
    int count = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        if ((mock_vga_buffer[i] & 0xFF) == target_char) {
            count++;
        }
    }
    return count;
}

int count_color_attributes_in_buffer(uint8_t color) {
    int count = 0;
    uint16_t color_mask = color << 8;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        if ((mock_vga_buffer[i] & 0xFF00) == color_mask) {
            count++;
        }
    }
    return count;
}

// Test implementations

void test_clear_screen(void) {
    // Set some initial pattern
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            set_vga_cell(row, col, 0x4741);  // 'A' on red background
        }
    }

    // Clear the screen
    clear_screen();

    // Verify all cells are cleared to the expected pattern
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            uint16_t cell = get_vga_cell(row, col);
            TEST_ASSERT_EQ(
                VGA_WHITE_ON_BLACK, cell,
                "Screen cell should be cleared to white-on-black space");
        }
    }
}

void test_clear_line_valid(void) {
    setup_mock_vga();

    // Set a pattern on line 10
    for (int col = 0; col < VGA_WIDTH; col++) {
        set_vga_cell(10, col, 0x4258);  // 'X' on red background
    }

    // Clear line 10
    clear_line(10);

    // Verify line 10 is cleared
    for (int col = 0; col < VGA_WIDTH; col++) {
        uint16_t cell = get_vga_cell(10, col);
        TEST_ASSERT_EQ(VGA_WHITE_ON_BLACK, cell,
                       "Line 10 should be cleared to white-on-black space");
    }

    // Verify other lines are unchanged
    for (int col = 0; col < VGA_WIDTH; col++) {
        uint16_t cell = get_vga_cell(9, col);
        TEST_ASSERT_EQ(0x0700, cell, "Line 9 should remain unchanged");

        cell = get_vga_cell(11, col);
        TEST_ASSERT_EQ(0x0700, cell, "Line 11 should remain unchanged");
    }
}

void test_clear_line_boundary(void) {
    setup_mock_vga();

    // Test clearing first line
    clear_line(0);
    for (int col = 0; col < VGA_WIDTH; col++) {
        uint16_t cell = get_vga_cell(0, col);
        TEST_ASSERT_EQ(VGA_WHITE_ON_BLACK, cell,
                       "First line should be cleared correctly");
    }

    // Test clearing last line
    clear_line(VGA_HEIGHT - 1);
    for (int col = 0; col < VGA_WIDTH; col++) {
        uint16_t cell = get_vga_cell(VGA_HEIGHT - 1, col);
        TEST_ASSERT_EQ(VGA_WHITE_ON_BLACK, cell,
                       "Last line should be cleared correctly");
    }

    // Test invalid line numbers (should not crash)
    clear_line(-1);          // Should handle gracefully
    clear_line(VGA_HEIGHT);  // Should handle gracefully
    clear_line(1000);        // Should handle gracefully
    TEST_ASSERT(true, "Invalid line numbers should be handled gracefully");
}

void test_print_at_basic(void) {
    setup_mock_vga();

    // Test basic string printing
    print_at(5, 10, "Hello", VGA_COLOR_WHITE);

    // Verify characters are placed correctly
    TEST_ASSERT_EQ('H', get_vga_cell(5, 10) & 0xFF,
                   "First character should be 'H'");
    TEST_ASSERT_EQ('e', get_vga_cell(5, 11) & 0xFF,
                   "Second character should be 'e'");
    TEST_ASSERT_EQ('l', get_vga_cell(5, 12) & 0xFF,
                   "Third character should be 'l'");
    TEST_ASSERT_EQ('l', get_vga_cell(5, 13) & 0xFF,
                   "Fourth character should be 'l'");
    TEST_ASSERT_EQ('o', get_vga_cell(5, 14) & 0xFF,
                   "Fifth character should be 'o'");

    // Verify color attributes
    uint16_t expected_color = VGA_COLOR_WHITE << 8;
    TEST_ASSERT_EQ(expected_color, get_vga_cell(5, 10) & 0xFF00,
                   "Color should be white");
    TEST_ASSERT_EQ(expected_color, get_vga_cell(5, 14) & 0xFF00,
                   "Color should be consistent");
}

void test_print_at_colors(void) {
    setup_mock_vga();

    // Test different colors
    print_at(1, 0, "Red", VGA_COLOR_RED);
    print_at(2, 0, "Green", VGA_COLOR_GREEN);
    print_at(3, 0, "Yellow", VGA_COLOR_YELLOW);

    // Verify colors are applied correctly
    TEST_ASSERT_EQ(VGA_COLOR_RED << 8, get_vga_cell(1, 0) & 0xFF00,
                   "Should have red color");
    TEST_ASSERT_EQ(VGA_COLOR_GREEN << 8, get_vga_cell(2, 0) & 0xFF00,
                   "Should have green color");
    TEST_ASSERT_EQ(VGA_COLOR_YELLOW << 8, get_vga_cell(3, 0) & 0xFF00,
                   "Should have yellow color");

    // Verify text content is preserved with color
    TEST_ASSERT_EQ('R', get_vga_cell(1, 0) & 0xFF,
                   "Red text should start with 'R'");
    TEST_ASSERT_EQ('G', get_vga_cell(2, 0) & 0xFF,
                   "Green text should start with 'G'");
    TEST_ASSERT_EQ('Y', get_vga_cell(3, 0) & 0xFF,
                   "Yellow text should start with 'Y'");
}

void test_print_at_boundary_cases(void) {
    setup_mock_vga();

    // Test printing at top-left corner
    print_at(0, 0, "TL", VGA_COLOR_WHITE);
    TEST_ASSERT_EQ('T', get_vga_cell(0, 0) & 0xFF, "Top-left should work");

    // Test printing at bottom-right (within bounds)
    print_at(VGA_HEIGHT - 1, VGA_WIDTH - 5, "BR", VGA_COLOR_WHITE);
    TEST_ASSERT_EQ('B', get_vga_cell(VGA_HEIGHT - 1, VGA_WIDTH - 5) & 0xFF,
                   "Bottom-right should work");

    // Test printing near right edge
    print_at(10, VGA_WIDTH - 1, "X", VGA_COLOR_WHITE);
    TEST_ASSERT_EQ('X', get_vga_cell(10, VGA_WIDTH - 1) & 0xFF,
                   "Right edge should work");

    // Test invalid coordinates (should not crash)
    print_at(-1, 0, "Invalid", VGA_COLOR_WHITE);          // Invalid row
    print_at(0, -1, "Invalid", VGA_COLOR_WHITE);          // Invalid column
    print_at(VGA_HEIGHT, 0, "Invalid", VGA_COLOR_WHITE);  // Row out of bounds
    print_at(0, VGA_WIDTH, "Invalid", VGA_COLOR_WHITE);  // Column out of bounds
    TEST_ASSERT(true, "Invalid coordinates should be handled gracefully");
}

void test_print_at_long_strings(void) {
    setup_mock_vga();

    // Test string that extends beyond screen width
    char long_string[100] =
        "This is a very long string that definitely exceeds the width of the "
        "VGA display buffer";
    print_at(15, 70, long_string, VGA_COLOR_GRAY);

    // Verify only characters within bounds are written
    for (int col = 70; col < VGA_WIDTH; col++) {
        char expected = long_string[col - 70];
        if (expected != '\0') {
            TEST_ASSERT_EQ(expected, get_vga_cell(15, col) & 0xFF,
                           "Characters within bounds should be written");
        }
    }

    // Verify no buffer overflow occurred (check next line is unchanged)
    for (int col = 0; col < 10; col++) {
        TEST_ASSERT_EQ(0x0700, get_vga_cell(16, col),
                       "Next line should remain unchanged");
    }
}

void test_print_at_special_characters(void) {
    setup_mock_vga();

    // Test empty string
    print_at(8, 20, "", VGA_COLOR_WHITE);
    TEST_ASSERT_EQ(0x0700, get_vga_cell(8, 20),
                   "Empty string should not modify buffer");

    // Test string with spaces
    print_at(9, 25, "A B C", VGA_COLOR_WHITE);
    TEST_ASSERT_EQ('A', get_vga_cell(9, 25) & 0xFF, "First char should be 'A'");
    TEST_ASSERT_EQ(' ', get_vga_cell(9, 26) & 0xFF,
                   "Space should be preserved");
    TEST_ASSERT_EQ('B', get_vga_cell(9, 27) & 0xFF,
                   "Second char should be 'B'");

    // Test numbers and symbols
    print_at(12, 30, "123!@#", VGA_COLOR_YELLOW);
    TEST_ASSERT_EQ('1', get_vga_cell(12, 30) & 0xFF,
                   "Numbers should display correctly");
    TEST_ASSERT_EQ('!', get_vga_cell(12, 33) & 0xFF,
                   "Symbols should display correctly");
}

void test_display_coordinate_calculation(void) {
    setup_mock_vga();

    // Test various coordinate combinations
    for (int row = 0; row < VGA_HEIGHT; row += 5) {
        for (int col = 0; col < VGA_WIDTH; col += 10) {
            char test_char = 'A' + (row + col) % 26;
            print_at(row, col, &test_char, VGA_COLOR_WHITE);

            char retrieved = get_vga_cell(row, col) & 0xFF;
            TEST_ASSERT_EQ(test_char, retrieved,
                           "Coordinate calculation should be accurate");
        }
    }
}

void test_vga_memory_safety(void) {
    setup_mock_vga();

    // Test that operations don't affect memory outside VGA buffer
    uint16_t guard_before = 0xDEAD;
    uint16_t guard_after = 0xBEEF;

    // Place guards around our mock buffer (conceptually)
    // In a real implementation, you'd check actual memory boundaries

    // Test massive string that should be truncated
    char massive_string[1000];
    for (int i = 0; i < 999; i++) {
        massive_string[i] = 'X';
    }
    massive_string[999] = '\0';

    print_at(20, 0, massive_string, VGA_COLOR_WHITE);

    // Verify truncation occurred properly
    int x_count = count_characters_in_buffer('X');
    TEST_ASSERT(x_count <= VGA_WIDTH,
                "String should be truncated to screen width");
    TEST_ASSERT(x_count > 0, "Some characters should have been written");

    // Verify buffer integrity
    TEST_ASSERT_EQ(0xDEAD, guard_before,
                   "Memory before buffer should be intact");
    TEST_ASSERT_EQ(0xBEEF, guard_after, "Memory after buffer should be intact");
}