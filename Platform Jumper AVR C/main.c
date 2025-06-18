#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>

#define RS PD0
#define EN PD1
#define LCD_PORT PORTD
#define LCD_DDR DDRD

#define BUTTON_PIN PINC
#define BUTTON PC0
#define BUZZER PB0

char terrainLower[17] = "                ";
const char character_run1 = 0;
const char character_run2 = 1;
const char character_jump = 2;
const char block_sprite = 3;

uint8_t isJumping = 0;
uint8_t jumpCount = 0;
uint16_t jumpTimer = 0;
uint8_t gameOver = 0;
uint16_t score = 0;
uint16_t highScore = 0;
uint16_t speed = 80;
uint8_t jumpMessageTimer = 0;
uint8_t jumpRequest = 0;
uint8_t currentButtonState = 1;
uint8_t previousButtonState = 1;
uint8_t runFrame = 0;

void delay_variable_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

void lcd_command(uint8_t cmd) {
    LCD_PORT = (LCD_PORT & 0x0F) | (cmd & 0xF0);
    LCD_PORT &= ~(1 << RS);
    LCD_PORT |= (1 << EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << EN);
    _delay_us(100);
    LCD_PORT = (LCD_PORT & 0x0F) | (cmd << 4);
    LCD_PORT |= (1 << EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << EN);
    _delay_ms(2);
}

void lcd_data(uint8_t data) {
    LCD_PORT = (LCD_PORT & 0x0F) | (data & 0xF0);
    LCD_PORT |= (1 << RS);
    LCD_PORT |= (1 << EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << EN);
    _delay_us(100);
    LCD_PORT = (LCD_PORT & 0x0F) | (data << 4);
    LCD_PORT |= (1 << EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << EN);
    _delay_ms(2);
}

void lcd_init() {
    LCD_DDR |= 0xFF;
    _delay_ms(20);
    lcd_command(0x02);
    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x06);
    lcd_command(0x01);
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(0x80 | (col + row_offsets[row]));
}

void lcd_string(const char* str) {
    while (*str) lcd_data(*str++);
}

void lcd_custom_char() {
    uint8_t run1[8] = {0b00000, 0b01110, 0b01101, 0b00110, 0b11110, 0b01110, 0b10010, 0b00000};
    lcd_command(0x40); for (int i = 0; i < 8; i++) lcd_data(run1[i]);

    uint8_t run2[8] = {0b00000, 0b01110, 0b01101, 0b00110, 0b11110, 0b01110, 0b01100, 0b00000};
    lcd_command(0x48); for (int i = 0; i < 8; i++) lcd_data(run2[i]);

    uint8_t jump[8] = {0b00000, 0b01110, 0b01101, 0b11110, 0b00010, 0b01110, 0b00000, 0b00000};
    lcd_command(0x50); for (int i = 0; i < 8; i++) lcd_data(jump[i]);

    uint8_t block[8] = {0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};
    lcd_command(0x58); for (int i = 0; i < 8; i++) lcd_data(block[i]);
}

void terrain_shift_left() {
    static uint8_t spawnCooldown = 0;
    static uint8_t currentObstacleLen = 0;
    for (int i = 0; i < 15; i++) terrainLower[i] = terrainLower[i + 1];
    if (spawnCooldown > 0) { terrainLower[15] = ' '; spawnCooldown--; }
    else if (currentObstacleLen > 0) { terrainLower[15] = '#'; currentObstacleLen--; }
    else {
        if (rand() % 6 == 0) {
            currentObstacleLen = (rand() % 2);
            terrainLower[15] = '#';
            spawnCooldown = 4;
        } else terrainLower[15] = ' ';
    }
}

void lcd_draw_scene() {
    char buf[6];
    itoa(score, buf, 10);
    lcd_set_cursor(0, 0);
    for (int i = 0; i < 16; i++) {
        if (isJumping && i == 1) lcd_data(character_jump);
        else lcd_data(' ');
    }

    lcd_set_cursor(16 - strlen(buf), 0);
    lcd_string(buf);

    lcd_set_cursor(8, 0);
    if (jumpMessageTimer > 0) { lcd_string("Jump!"); jumpMessageTimer--; }
    else lcd_string("     ");

    lcd_set_cursor(0, 1);
    for (int i = 0; i < 16; i++) {
        if (!isJumping && i == 1) lcd_data(runFrame ? character_run1 : character_run2);
        else if (terrainLower[i] == '#') lcd_data(block_sprite);
        else lcd_data(' ');
    }
}

void short_beep() {
    for (int i = 0; i < 100; i++) {
        PORTB ^= (1 << BUZZER);
        _delay_us(500);
    }
}

void game_start_beep() {
    for (int i = 0; i < 2; i++) {
        short_beep(); _delay_ms(100);
    }
}

void game_over_beep() {
    for (int i = 0; i < 3; i++) {
        short_beep(); _delay_ms(150);
    }
}

void game_over_screen() {
    lcd_command(0x01);
    for (int i = 0; i < 4; i++) {
        lcd_set_cursor(2, 0);
        if (i % 2 == 0) lcd_string("GAME OVER !");
        else lcd_string("            ");
        _delay_ms(300);
    }

    if (score > highScore) highScore = score;

    char scoreLine[17];
    char bestLine[17];
    sprintf(scoreLine, "Score: %u", score);
    sprintf(bestLine, "Best : %u", highScore);

    lcd_command(0x01);
    lcd_set_cursor(0, 0);
    lcd_string(scoreLine);
    lcd_set_cursor(0, 1);
    lcd_string(bestLine);

    game_over_beep();
}

void reset_game() {
    score = 0;
    speed = 80;
    isJumping = 0;
    jumpCount = 0;
    jumpTimer = 0;
    jumpMessageTimer = 0;
    gameOver = 0;
    jumpRequest = 0;
    runFrame = 0;
    for (int i = 0; i < 16; i++) terrainLower[i] = ' ';
    lcd_command(0x01);
    game_start_beep();
}

int main(void) {
    lcd_init();
    lcd_custom_char();

    DDRC &= ~(1 << BUTTON);
    PORTC |= (1 << BUTTON);
    DDRB |= (1 << BUZZER);

    lcd_set_cursor(0, 0);
    lcd_string("PLATFORM JUMPER");

    for (int i = 0; i < 6; i++) {
        lcd_set_cursor(0, 1);
        if (i % 2 == 0) lcd_string("Press to Jump !");
        else lcd_string("                ");
        _delay_ms(300);
    }

    while ((BUTTON_PIN & (1 << BUTTON))) {}
    _delay_ms(300);
    lcd_command(0x01);
    game_start_beep();

    while (1) {
        currentButtonState = (BUTTON_PIN & (1 << BUTTON)) == 0;
        if (currentButtonState && !previousButtonState && jumpCount < 2 && !gameOver)
            jumpRequest = 1;
        previousButtonState = currentButtonState;

        if (jumpRequest && jumpCount < 2 && !gameOver) {
            isJumping = 1;
            jumpTimer = 3;
            jumpMessageTimer = 3;
            short_beep();
            jumpCount++;
            jumpRequest = 0;
        }

        if (!gameOver) {
            terrain_shift_left();
            runFrame = !runFrame;

            if (isJumping && jumpTimer > 0) jumpTimer--;
            else if (isJumping && jumpTimer == 0) {
                isJumping = 0;
                if (terrainLower[1] != '#') jumpCount = 0;
            }

            if (!isJumping && terrainLower[1] == '#') {
                gameOver = 1;
                game_over_screen();
                continue;
            }

            lcd_draw_scene();
            score++;
            if (score % 50 == 0 && speed > 50) speed -= 5;
            delay_variable_ms(speed);
        } else {
            if ((BUTTON_PIN & (1 << BUTTON)) == 0) {
                _delay_ms(300);
                reset_game();
            }
        }
    }
}
