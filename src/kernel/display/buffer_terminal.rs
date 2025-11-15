// SPDX-License-Identifier: AGPL-3.0-or-later
//! Terminal display mode for VGA text buffer
//! 
//! Based on buffer.c (text editor mode), this adds terminal emulation:
//! - ANSI escape sequences support
//! - Cursor positioning
//! - Colors and attributes
//! - Terminal state machine

#![no_std]

const SCREEN_WIDTH: usize = 80;
const SCREEN_HEIGHT: usize = 25;
const VGA_BUFFER: *mut u8 = 0xB8000 as *mut u8;

/// Terminal modes
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TerminalMode {
    /// Normal text editor mode (from buffer.c)
    TextEditor,
    /// Terminal mode with ANSI escape sequences
    Terminal,
}

/// Terminal state for ANSI sequence parsing
struct TerminalState {
    mode: TerminalMode,
    cursor_x: usize,
    cursor_y: usize,
    color: u8,
    /// Buffer for parsing ANSI escape sequences
    escape_buffer: [u8; 32],
    escape_len: usize,
    /// True if we're in the middle of an ANSI sequence
    in_escape: bool,
}

impl TerminalState {
    fn new() -> Self {
        TerminalState {
            mode: TerminalMode::TextEditor,
            cursor_x: 0,
            cursor_y: 0,
            color: 0x07, // white on black
            escape_buffer: [0; 32],
            escape_len: 0,
            in_escape: false,
        }
    }

    /// Switch between terminal modes
    fn set_mode(&mut self, mode: TerminalMode) {
        self.mode = mode;
    }

    /// Handle ANSI escape sequence
    /// Examples: ESC[2J (clear), ESC[H (home), ESC[31m (red)
    fn handle_escape_sequence(&mut self) {
        if self.escape_len < 2 {
            return;
        }

        // Parse escape sequence [ESC][param1];[param2]...[command]
        if self.escape_buffer[0] != b'[' {
            return;
        }

        let cmd = self.escape_buffer[self.escape_len - 1];
        match cmd {
            // 'J' - Clear display
            b'J' => self.clear_display(),
            // 'H' - Cursor home/position
            b'H' => self.cursor_position(),
            // 'm' - SGR (Select Graphic Rendition) - colors/attributes
            b'm' => self.handle_sgr(),
            // 'A' - Cursor up
            b'A' => self.cursor_up(),
            // 'B' - Cursor down
            b'B' => self.cursor_down(),
            // 'C' - Cursor forward
            b'C' => self.cursor_forward(),
            // 'D' - Cursor back
            b'D' => self.cursor_back(),
            _ => {}
        }

        self.in_escape = false;
        self.escape_len = 0;
    }

    /// Clear display (CSI 2J)
    fn clear_display(&mut self) {
        unsafe {
            for i in 0..(SCREEN_WIDTH * SCREEN_HEIGHT * 2) {
                if i % 2 == 0 {
                    core::ptr::write_volatile(VGA_BUFFER.add(i), b' ');
                } else {
                    core::ptr::write_volatile(VGA_BUFFER.add(i), self.color);
                }
            }
        }
        self.cursor_x = 0;
        self.cursor_y = 0;
    }

    /// Parse cursor position (CSI row;colH)
    fn cursor_position(&mut self) {
        // Simple parsing: extract numbers from escape buffer
        let mut row = 1usize;
        let mut col = 1usize;

        // Skip '[' and parse first number
        let mut idx = 1;
        let mut num_str = [0u8; 3];
        let mut num_len = 0;

        while idx < self.escape_len - 1 {
            let ch = self.escape_buffer[idx];
            if ch >= b'0' && ch <= b'9' {
                if num_len < 3 {
                    num_str[num_len] = ch;
                    num_len += 1;
                }
            } else if ch == b';' {
                if num_len > 0 {
                    row = parse_number(&num_str[..num_len]);
                }
                num_len = 0;
            }
            idx += 1;
        }

        if num_len > 0 {
            col = parse_number(&num_str[..num_len]);
        }

        // Clamp to screen bounds
        if row > 0 {
            row -= 1;
        }
        if col > 0 {
            col -= 1;
        }
        if row >= SCREEN_HEIGHT {
            row = SCREEN_HEIGHT - 1;
        }
        if col >= SCREEN_WIDTH {
            col = SCREEN_WIDTH - 1;
        }

        self.cursor_x = col;
        self.cursor_y = row;
    }

    /// Handle SGR (Select Graphic Rendition) - ESC[31m etc
    fn handle_sgr(&mut self) {
        // Parse color codes like 31 (red), 32 (green), 37 (white), 0 (reset)
        let mut num_str = [0u8; 3];
        let mut num_len = 0;

        for i in 1..self.escape_len - 1 {
            let ch = self.escape_buffer[i];
            if ch >= b'0' && ch <= b'9' {
                if num_len < 3 {
                    num_str[num_len] = ch;
                    num_len += 1;
                }
            }
        }

        if num_len > 0 {
            let code = parse_number(&num_str[..num_len]);
            self.color = match code {
                0 => 0x07,  // Reset - white on black
                30 => 0x00, // Black
                31 => 0x04, // Red
                32 => 0x02, // Green
                33 => 0x06, // Yellow
                34 => 0x01, // Blue
                35 => 0x05, // Magenta
                36 => 0x03, // Cyan
                37 => 0x07, // White
                _ => self.color,
            };
        }
    }

    fn cursor_up(&mut self) {
        if self.cursor_y > 0 {
            self.cursor_y -= 1;
        }
    }

    fn cursor_down(&mut self) {
        if self.cursor_y < SCREEN_HEIGHT - 1 {
            self.cursor_y += 1;
        }
    }

    fn cursor_forward(&mut self) {
        if self.cursor_x < SCREEN_WIDTH - 1 {
            self.cursor_x += 1;
        }
    }

    fn cursor_back(&mut self) {
        if self.cursor_x > 0 {
            self.cursor_x -= 1;
        }
    }

    /// Write a character to terminal
    fn putc(&mut self, c: u8) {
        match c {
            // ESC - start escape sequence
            27 => {
                self.in_escape = true;
                self.escape_len = 0;
            }
            // Newline - move to next line
            b'\n' => {
                self.cursor_x = 0;
                if self.cursor_y < SCREEN_HEIGHT - 1 {
                    self.cursor_y += 1;
                } else {
                    // Scroll up
                    self.scroll_up();
                }
            }
            // Carriage return
            b'\r' => {
                self.cursor_x = 0;
            }
            // Backspace
            8 => {
                if self.cursor_x > 0 {
                    self.cursor_x -= 1;
                    self.write_char_at(b' ');
                }
            }
            _ if self.in_escape => {
                // Accumulate escape sequence
                if self.escape_len < self.escape_buffer.len() {
                    self.escape_buffer[self.escape_len] = c;
                    self.escape_len += 1;

                    // Check if sequence is complete (ends with letter)
                    if (c >= b'A' && c <= b'Z') || (c >= b'a' && c <= b'z') {
                        self.handle_escape_sequence();
                    }
                }
            }
            _ if c >= 32 && c < 127 => {
                // Printable character
                self.write_char_at(c);
                self.cursor_x += 1;
                if self.cursor_x >= SCREEN_WIDTH {
                    self.cursor_x = 0;
                    if self.cursor_y < SCREEN_HEIGHT - 1 {
                        self.cursor_y += 1;
                    } else {
                        self.scroll_up();
                    }
                }
            }
            _ => {}
        }
    }

    /// Write character at current cursor position
    fn write_char_at(&mut self, c: u8) {
        unsafe {
            let offset = (self.cursor_y * SCREEN_WIDTH + self.cursor_x) * 2;
            core::ptr::write_volatile(VGA_BUFFER.add(offset), c);
            core::ptr::write_volatile(VGA_BUFFER.add(offset + 1), self.color);
        }
    }

    /// Scroll display up by one line
    fn scroll_up(&mut self) {
        unsafe {
            // Move lines up
            for row in 0..SCREEN_HEIGHT - 1 {
                for col in 0..SCREEN_WIDTH {
                    let src_offset = ((row + 1) * SCREEN_WIDTH + col) * 2;
                    let dst_offset = (row * SCREEN_WIDTH + col) * 2;

                    let ch = core::ptr::read_volatile(VGA_BUFFER.add(src_offset));
                    let attr = core::ptr::read_volatile(VGA_BUFFER.add(src_offset + 1));

                    core::ptr::write_volatile(VGA_BUFFER.add(dst_offset), ch);
                    core::ptr::write_volatile(VGA_BUFFER.add(dst_offset + 1), attr);
                }
            }

            // Clear last line
            for col in 0..SCREEN_WIDTH {
                let offset = ((SCREEN_HEIGHT - 1) * SCREEN_WIDTH + col) * 2;
                core::ptr::write_volatile(VGA_BUFFER.add(offset), b' ');
                core::ptr::write_volatile(VGA_BUFFER.add(offset + 1), self.color);
            }
        }
    }
}

static mut TERMINAL: Option<TerminalState> = None;

/// Initialize terminal
#[no_mangle]
pub extern "C" fn terminal_init() {
    unsafe {
        TERMINAL = Some(TerminalState::new());
    }
}

/// Set terminal mode
#[no_mangle]
pub extern "C" fn terminal_set_mode(mode: u32) {
    unsafe {
        if let Some(ref mut term) = TERMINAL {
            let m = if mode == 0 {
                TerminalMode::TextEditor
            } else {
                TerminalMode::Terminal
            };
            term.set_mode(m);
        }
    }
}

/// Write a character to terminal
#[no_mangle]
pub extern "C" fn terminal_putc(c: u8) {
    unsafe {
        if let Some(ref mut term) = TERMINAL {
            term.putc(c);
        }
    }
}

/// Write a string to terminal
#[no_mangle]
pub extern "C" fn terminal_puts(s: *const u8) {
    if s.is_null() {
        return;
    }

    unsafe {
        let mut i = 0;
        loop {
            let c = core::ptr::read(s.add(i));
            if c == 0 {
                break;
            }
            terminal_putc(c);
            i += 1;
        }
    }
}

/// Clear terminal
#[no_mangle]
pub extern "C" fn terminal_clear() {
    unsafe {
        if let Some(ref mut term) = TERMINAL {
            term.clear_display();
        }
    }
}

/// Parse a decimal number from a byte slice
fn parse_number(bytes: &[u8]) -> usize {
    let mut num = 0usize;
    for &b in bytes {
        if b >= b'0' && b <= b'9' {
            num = num * 10 + (b - b'0') as usize;
        }
    }
    num
}
