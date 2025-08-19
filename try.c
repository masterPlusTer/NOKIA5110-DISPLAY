//this code runs in an ESP32 WROVER




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
#define LCD_PAGES  (LCD_HEIGHT / 8) // El display tiene 6 "páginas" o "bancos" de 8 píxeles de alto

// Handle para el dispositivo SPI
spi_device_handle_t spi;

// --- FIGURA: Bitmap de una cara sonriente de 8x8 píxeles ---
// Cada byte representa una columna vertical de 8 píxeles.
const uint8_t smiley_bitmap_8x8[] = {
    0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C
};

void nokia_send_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0); // D/C en bajo para COMANDO
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(spi, &t);
}

void nokia_send_data(const uint8_t *data, int len) {
    if (len == 0) return;
    gpio_set_level(PIN_DC, 1); // D/C en alto para DATOS
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8; // Longitud en bits
    t.tx_buffer = data;
    spi_device_polling_transmit(spi, &t);
}

// Función para posicionar el "cursor" de escritura en la pantalla
void lcd_set_cursor(uint8_t x, uint8_t page) {
    if (x >= LCD_WIDTH || page >= LCD_PAGES) return;
    // El display se direcciona por páginas (bancos de 8 píxeles de alto)
    nokia_send_cmd(0x40 | page); // Comando para establecer la dirección Y (página)
    nokia_send_cmd(0x80 | x);    // Comando para establecer la dirección X (columna)
}

// --- NUEVA FUNCIÓN: Limpia toda la pantalla ---
void lcd_clear(void) {
    // Creamos un buffer en blanco del tamaño de una página
    uint8_t zero_buffer[LCD_WIDTH] = {0};
    for (int page = 0; page < LCD_PAGES; page++) {
        lcd_set_cursor(0, page);
        // Enviamos 84 bytes con valor 0x00 para limpiar cada página
        nokia_send_data(zero_buffer, LCD_WIDTH);
    }
}

// --- NUEVA FUNCIÓN: Dibuja un bitmap en la pantalla ---
// Nota: Esta función simple asume que la altura del bitmap es un múltiplo de 8
// y que se dibuja alineado a las páginas del display.
void lcd_draw_bitmap(uint8_t x, uint8_t y_page, const uint8_t* bitmap, uint8_t w, uint8_t h_pages) {
    for (int p = 0; p < h_pages; p++) {
        lcd_set_cursor(x, y_page + p);
        // Enviamos una fila de 'w' bytes del bitmap a la vez
        nokia_send_data(bitmap + (p * w), w);
    }
}

void lcd_init(void) {
    // Configurar pines de control
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BL, 1);

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

    // Configuración del dispositivo SPI
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7, // Aumentado para permitir transacciones más grandes
    };
    spi_bus_add_device(HSPI_HOST, &devcfg, &spi);

    // Secuencia de inicialización del controlador PCD8544
    nokia_send_cmd(0x21); // Modo de instrucción extendido
    nokia_send_cmd(0xC0); // Contraste (Vop), puedes ajustarlo si es necesario
    nokia_send_cmd(0x04); // Coeficiente de temperatura
    nokia_send_cmd(0x13); // Bias system (1:48)
    nokia_send_cmd(0x20); // Volver al modo de instrucción básico
    nokia_send_cmd(0x0C); // Modo de display normal
}

void app_main(void) {
    // 1. Inicializar el display
    lcd_init();

    // 2. Limpiar la pantalla para empezar desde un estado conocido (todo apagado)
    lcd_clear();

    // 3. Dibujar nuestra figura
    // Parámetros:
    // - x: 38 (columna donde empieza el dibujo, para centrarlo (84-8)/2)
    // - y_page: 2 (página donde empieza el dibujo, para centrarlo (6-1)/2)
    // - bitmap: smiley_bitmap_8x8 (nuestra figura)
    // - w: 8 (ancho de la figura en píxeles)
    // - h_pages: 1 (alto de la figura en páginas; como mide 8px, ocupa 1 página)
    lcd_draw_bitmap(38, 2, smiley_bitmap_8x8, 8, 1);

    // También podemos dibujar otra en una esquina
    lcd_draw_bitmap(0, 0, smiley_bitmap_8x8, 8, 1);

    // Y otra en la esquina opuesta
    lcd_draw_bitmap(LCD_WIDTH - 8, LCD_PAGES - 1, smiley_bitmap_8x8, 8, 1);
}
