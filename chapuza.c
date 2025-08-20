#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

// Pines para el display Nokia 5110 (PCD8544)
#define PIN_RST  4   // Reset
#define PIN_CS   15  // Chip Select (Chip Enable)
#define PIN_DC   2   // Data/Command
#define PIN_MOSI 23  // SPI MOSI (DIN en el display)
#define PIN_CLK  18  // SPI Clock (CLK)
#define PIN_BL   5   // Backlight (Luz de fondo)

// Dimensiones del display
#define LCD_WIDTH  84
#define LCD_HEIGHT 48
#define LCD_PAGES  (LCD_HEIGHT / 8)

spi_device_handle_t spi;

// --- Bitmap de la carita sonriente CORREGIDO (8x8) ---
// Este bitmap ha sido rotado para que la carita aparezca en vertical,
// igual que el texto. Cada byte representa una COLUMNA vertical de 8 píxeles.
const uint8_t smiley_corregido_bitmap_8x8[] = {
    0x3E, 0x42, 0xA9, 0xBD, 0xBD, 0xA9, 0x42, 0x3E
};

// --- Fuente 5x7 ASCII (solo caracteres del 32 al 127) ---
// NOTA: Esta fuente está incompleta. Solo se han definido algunos caracteres.
// Para un proyecto completo, deberías añadir todos los caracteres que necesites.
const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
    {0x00,0x41,0x7F,0x41,0x00}, // '!' (33)
    {0x07,0x00,0x07,0x00,0x07}, // '"' (34)
    {0x14,0x7F,0x14,0x7F,0x14}, // '#' (35)
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$' (36)
    {0x23,0x13,0x08,0x64,0x62}, // '%' (37)
    {0x36,0x49,0x55,0x22,0x50}, // '&' (38)
    {0x00,0x05,0x03,0x00,0x00}, // ''' (39)
    {0x00,0x1C,0x22,0x41,0x00}, // '(' (40)
    {0x00,0x41,0x22,0x1C,0x00}, // ')' (41)
    {0x14,0x08,0x3E,0x08,0x14}, // '*' (42)
    {0x08,0x08,0x3E,0x08,0x08}, // '+' (43)
    {0x00,0x50,0x30,0x00,0x00}, // ',' (44)
    {0x08,0x08,0x08,0x08,0x08}, // '-' (45)
    {0x00,0x60,0x60,0x00,0x00}, // '.' (46)
    {0x20,0x10,0x08,0x04,0x02}, // '/' (47)
    {0x3E,0x51,0x49,0x45,0x3E}, // '0' (48)
    {0x00,0x42,0x7F,0x40,0x00}, // '1' (49)
    {0x42,0x61,0x51,0x49,0x46}, // '2' (50)
    {0x21,0x41,0x45,0x4B,0x31}, // '3' (51)
    {0x18,0x14,0x12,0x7F,0x10}, // '4' (52)
    {0x27,0x45,0x45,0x45,0x39}, // '5' (53)
    {0x3C,0x4A,0x49,0x49,0x30}, // '6' (54)
    {0x01,0x71,0x09,0x05,0x03}, // '7' (55)
    {0x36,0x49,0x49,0x49,0x36}, // '8' (56)
    {0x06,0x49,0x49,0x29,0x1E}, // '9' (57)
    {0x00,0x36,0x36,0x00,0x00}, // ':' (58)
    {0x00,0x56,0x36,0x00,0x00}, // ';' (59)
    {0x08,0x14,0x22,0x41,0x00}, // '<' (60)
    {0x14,0x14,0x14,0x14,0x14}, // '=' (61)
    {0x00,0x41,0x22,0x14,0x08}, // '>' (62)
    {0x02,0x01,0x51,0x09,0x06}, // '?' (63)
    {0x32,0x49,0x79,0x41,0x3E}, // '@' (64)
    {0x7C,0x12,0x11,0x12,0x7C}, // 'A' (65)
    {0x7F,0x49,0x49,0x49,0x36}, // 'B' (66)
    {0x3E,0x41,0x41,0x41,0x22}, // 'C' (67)
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D' (68)
    {0x7F,0x49,0x49,0x49,0x41}, // 'E' (69)
    {0x7F,0x09,0x09,0x09,0x01}, // 'F' (70)
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G' (71)
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H' (72)
    {0x00,0x41,0x7F,0x41,0x00}, // 'I' (73)
    {0x20,0x40,0x41,0x3F,0x01}, // 'J' (74)
    {0x7F,0x08,0x14,0x22,0x41}, // 'K' (75)
    {0x7F,0x40,0x40,0x40,0x40}, // 'L' (76)
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M' (77)
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N' (78)
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O' (79)
    {0x7F,0x09,0x09,0x09,0x06}, // 'P' (80)
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q' (81)
    {0x7F,0x09,0x19,0x29,0x46}, // 'R' (82)
    // ... y así sucesivamente
};

// --- Funciones básicas ---
void nokia_send_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0); // Comando
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(spi, &t);
}

void nokia_send_data(const uint8_t *data, int len) {
    if(len == 0) return;
    gpio_set_level(PIN_DC, 1); // Datos
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(spi, &t);
}

void lcd_set_cursor(uint8_t x, uint8_t page) {
    if(x >= LCD_WIDTH || page >= LCD_PAGES) return;
    nokia_send_cmd(0x40 | page);
    nokia_send_cmd(0x80 | x);
}

void lcd_clear(void) {
    uint8_t zero_buffer[LCD_WIDTH] = {0};
    for(int page=0; page<LCD_PAGES; page++) {
        lcd_set_cursor(0,page);
        nokia_send_data(zero_buffer,LCD_WIDTH);
    }
}

void lcd_draw_bitmap(uint8_t x, uint8_t y_page, const uint8_t* bitmap, uint8_t w, uint8_t h_pages) {
    for(int p=0; p<h_pages; p++) {
        lcd_set_cursor(x, y_page + p);
        nokia_send_data(bitmap + (p*w), w);
    }
}

// --- Funciones de texto ---
void lcd_draw_char(uint8_t x, uint8_t page, char c) {
    // Si el caracter está fuera del rango que hemos definido en la fuente, lo reemplazamos por '?'
    if(c < 32 || c > 'R') {
        c = '?';
    }
    
    const uint8_t* bitmap;
    
    if (c == '?') {
        // Bitmap para '?' (signo de interrogación).
        // AÑADIMOS 'static' PARA QUE LA MEMORIA NO SE LIBERE AL SALIR DEL BLOQUE IF.
        static const uint8_t q_mark[] = {0x02, 0x01, 0x51, 0x09, 0x06};
        bitmap = q_mark;
    } else {
        // Para el resto de caracteres, usamos la tabla global font5x7
        bitmap = font5x7[c - 32];
    }

    lcd_set_cursor(x, page);
    nokia_send_data(bitmap, 5);
    uint8_t space = 0x00;
    nokia_send_data(&space, 1); // espacio de 1 pixel entre caracteres
}

void lcd_draw_text(uint8_t x, uint8_t page, const char* str) {
    while(*str) {
        lcd_draw_char(x, page, *str++);
        x += 6; // 5 pixeles del caracter + 1 pixel de espacio
        if(x + 5 >= LCD_WIDTH) break; // Si no cabe el siguiente caracter, paramos
    }
}

// --- Inicialización ---
void lcd_init(void) {
    // Configuración de los pines
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BL, 1); // Encender la luz de fondo

    // Reset del display
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Configuración del bus SPI
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_PAGES
    };
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // Configuración del dispositivo SPI (el display)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000, // 4 MHz
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7
    };
    spi_bus_add_device(HSPI_HOST, &devcfg, &spi);

    // Comandos de inicialización para el controlador PCD8544
    nokia_send_cmd(0x21); // Set extendido de instrucciones
    nokia_send_cmd(0xC0); // Set Vop (contraste) - puedes ajustar este valor
    nokia_send_cmd(0x07); // Set temp. coefficient
    nokia_send_cmd(0x13); // Set Bias system (1:48)
    nokia_send_cmd(0x20); // Volver al set básico de instrucciones
    nokia_send_cmd(0x0C); // Display normal (no invertido)
}

// --- Función principal ---
void app_main(void) {
    lcd_init();
    lcd_clear();

    // --- DIBUJAR TEXTO ---
    // El texto se dibuja en las páginas (filas) 0 y 1
    lcd_draw_text(0, 0, "Hola ESP32!");
    lcd_draw_text(0, 1, "Nokia 5110");

    // --- DIBUJAR BITMAPS (CARITAS) ---
    // Las caritas se dibujan más abajo (en la página 4) para no superponerse.
    // Usamos el nuevo bitmap que está orientado correctamente.
    
    // Dibujar una carita a la izquierda
    lcd_draw_bitmap(20, 4, smiley_corregido_bitmap_8x8, 8, 1);

    // Dibujar otra carita a la derecha
    lcd_draw_bitmap(LCD_WIDTH - 20 - 8, 4, smiley_corregido_bitmap_8x8, 8, 1);
}
