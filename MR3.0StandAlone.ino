#include <Control_Surface.h> // Incluye la librería Control Surface
#include <Adafruit_NeoPixel.h>

#include "pico/bootrom.h"
#include "pico/stdlib.h"

//#define BUTTON_2 2
//#define BUTTON_3 3

#define PIN 16
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Nueva tira de LEDs
#define PIN_STRIP 8
#define NUM_LEDS 4
Adafruit_NeoPixel strip(NUM_LEDS, PIN_STRIP, NEO_GRB + NEO_KHZ800);

// Colores para cada banco (puedes modificarlos)
uint32_t bankColors[5] = {
  strip.Color(211, 0, 0),    // Banco 0: Rojo
  strip.Color(117, 0, 209),    // Banco 1: Verde
  strip.Color(41,183, 183),    // Banco 2: Azul
  strip.Color(255, 255, 0),  // Banco 3: Amarillo
  strip.Color(255, 0, 255)   // Banco 4: Magenta
};

// Variables para control de intensidad de LEDs
uint8_t ledBrightness = 130; // Brillo actual (0-255)

// Instancia de la interfaz MIDI por USB
USBMIDI_Interface midi;

Bank<5> bank(5);

// Selector de banco con LEDs
IncrementSelectorLEDs<5> selector {
  bank,
  {2},
  {9,10,11,12,13}  // Estos parecen ser los pines para los LEDs del selector
};

Bankable::CCButton pulsador1 {
  {bank, BankType::ChangeAddress},
  5,
  {0x51, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador2 {
  {bank, BankType::ChangeAddress},
  4,
  {0x52, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador3 {
  {bank, BankType::ChangeAddress},
  3,
  {0x53, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador4 {
  {bank, BankType::ChangeAddress},
  7,
  {0x54, Channel_11},
};

using CCSmartPot = Bankable::CCSmartPotentiometer<5>;
using CCSmartPot2 = Bankable::CCSmartPotentiometer<5>;

CCSmartPot potentiometer {
  {bank, BankType::ChangeAddress},
  A0,
  {MIDI_CC::General_Purpose_Controller_1 , Channel_11},
};

CCSmartPot potentiometer2 {
  {bank, BankType::ChangeAddress},
  A1,
  {MIDI_CC::General_Purpose_Controller_2, Channel_11},
};

constexpr analog_t maxRawValue = CCSmartPot::getMaxRawValue();
constexpr analog_t minimumValue = 6000;
constexpr analog_t maximumValue = 10000;

analog_t mappingFunction(analog_t raw) {
  raw = constrain(raw, minimumValue, maximumValue);
  return map(raw, minimumValue, maximumValue, 0, maxRawValue);
}

// Función para aplicar brillo a un color
uint32_t applyBrightness(uint32_t color, uint8_t brightness) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  
  return strip.Color(r, g, b);
}

// Conversión HSV a RGB
uint32_t colorWheel(byte hue) {
  byte r, g, b;
  byte region = hue / 43;
  byte remainder = (hue - (region * 43)) * 6;

  byte p = 0;
  byte q = (255 * (255 - remainder)) / 255;
  byte t = (255 * remainder) / 255;

  switch (region) {
    case 0: r = 255; g = t; b = 0; break;
    case 1: r = q; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = t; break;
    case 3: r = 0; g = q; b = 255; break;
    case 4: r = t; g = 0; b = 255; break;
  }

  return pixels.Color(r, g, b);
}

// Animación arcoíris suave
void animateRainbow() {
  const int duration = 3000; // Duración total en ms
  const int steps = 256;     // Pasos en el arcoíris
  const int delayTime = duration / steps;

  for (int i = 0; i < steps; ++i) {
    byte hue = i;
    pixels.setPixelColor(0, colorWheel(hue));
    pixels.show();
    delay(delayTime);
  }
}

// Actualiza los LEDs de la tira según el banco activo y el estado de los pulsadores
void updateBankLeds() {
  uint8_t activeBank = bank.getSelection();
  uint32_t color = bankColors[activeBank];

  // LED 0: asociado al pulsador del pin 5 (pulsador4) - Momentáneo, siempre encendido
  strip.setPixelColor(0, applyBrightness(color, ledBrightness));

  // LED 1: asociado al pulsador del pin 3 (pulsador2) - Latched
  if (pulsador2.getState()) {
    strip.setPixelColor(1, applyBrightness(color, ledBrightness));
  } else {
    strip.setPixelColor(1, 0);
  }

  // LED 2: asociado al pulsador del pin 4 (pulsador3) - Latched
  if (pulsador3.getState()) {
    strip.setPixelColor(2, applyBrightness(color, ledBrightness));
  } else {
    strip.setPixelColor(2, 0);
  }

  // LED 3: Cuarto LED - siempre encendido en blanco claro (brillo ajustado)
  strip.setPixelColor(3, strip.Color(200, 200, 200)); // Blanco claro

  strip.show();
}

void setup() {
  pixels.begin();
  pixels.show();

  strip.begin();
  strip.show();

  animateRainbow(); // Animación de inicio

  // Configuración simplificada
  Control_Surface.begin();
  
  potentiometer.map(mappingFunction);
  potentiometer2.map(mappingFunction);
}

void loop() {
  // Mantener el LED individual en blanco después de la animación
  pixels.setPixelColor(0, pixels.Color(255, 255, 255));
  pixels.show();

  // Solo actualizar LEDs cuando haya cambios
  static uint8_t lastBank = 255;
  static bool lastStates[2] = {false, false}; // Para los 2 pulsadores latched
  
  uint8_t currentBank = bank.getSelection();
  bool currentStates[2] = {
    pulsador2.getState(),
    pulsador3.getState()
  };

  // Verificar si cambió el banco o algún estado de pulsador latched
  bool needsUpdate = false;
  
  if (currentBank != lastBank) {
    needsUpdate = true;
    lastBank = currentBank;
  }
  
  for (int i = 0; i < 2; i++) {
    if (currentStates[i] != lastStates[i]) {
      needsUpdate = true;
      lastStates[i] = currentStates[i];
    }
  }

  // El cuarto LED siempre debe estar encendido, así que actualizamos siempre
  needsUpdate = true;

  // Actualizar LEDs si hubo cambios
  if (needsUpdate) {
    updateBankLeds();
  }

  // Dejar que Control_Surface maneje todos los pulsadores automáticamente
  Control_Surface.loop();
}