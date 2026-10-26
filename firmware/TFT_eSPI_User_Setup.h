// Screen settings for the TFT_eSPI library.
// Copy this file over Arduino/libraries/TFT_eSPI/User_Setup.h
//
// Change the driver and size if your screen is different
// (see the other examples in TFT_eSPI/User_Setups/).

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Screen pins (ESP32-S3)
#define TFT_MISO -1   // not used
#define TFT_MOSI 11   // SDA on the screen
#define TFT_SCLK 12   // SCL on the screen
#define TFT_CS   -1   // screen without CS pin
#define TFT_DC    5
#define TFT_RST   4

// The RFID reader uses the default SPI bus on other pins,
// so the screen must use its own SPI port.
#define USE_HSPI_PORT

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define SPI_FREQUENCY 27000000
