#include <Control_Surface.h>
#include <Adafruit_NeoPixel.h>

#include "pico/bootrom.h"
#include "pico/stdlib.h"

#define BUTTON_2 2
#define BUTTON_3 3

#define PIN 16
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Tira de LEDs
#define PIN_STRIP 6
#define NUM_LEDS 4
Adafruit_NeoPixel strip(NUM_LEDS, PIN_STRIP, NEO_GRB + NEO_KHZ800);

// =============================================================================
// CONFIGURACIÓN DE COLORES DE BANCOS - FÁCIL DE MODIFICAR
// =============================================================================

// Colores para cada banco (RGB)
uint32_t bankColors[5] = {
  strip.Color(255, 0, 0),    // Banco 0: Rojo
  strip.Color(0, 255, 0),    // Banco 1: Verde
  strip.Color(0, 0, 255),    // Banco 2: Azul
  strip.Color(255, 255, 0),  // Banco 3: Amarillo
  strip.Color(255, 0, 255)   // Banco 4: Magenta
};

// =============================================================================
// CONFIGURACIÓN DE PINES - VERIFICAR ANTES DE MODIFICAR
// =============================================================================

// Variables para control de intensidad de LEDs
uint8_t ledBrightness = 130; // Brillo actual (0-255)

USBMIDI_Interface midi;
Bank<5> bank(5);

// Selector de banco con LEDs - PINES 7 Y 8
// Pines: {increment, decrement} = {7, 8}
// LEDs: {9, 10, 11, 12, 13}
IncrementDecrementSelectorLEDs<5> selector {
  bank,
  {7, 8},
  {9, 10, 11, 12, 13}
};

// =============================================================================
// PULSADORES BANKABLE - VERSIÓN SIMPLIFICADA
// =============================================================================

// Pulsadores latched (CCButtonLatched) - Pulsadores 1, 2 y 3
Bankable::CCButtonLatched<5> pulsador1 {
  {bank, BankType::ChangeAddress},
  2,
  {0x51, Channel_11},
};

Bankable::CCButtonLatched<5> pulsador2 {
  {bank, BankType::ChangeAddress},
  3,
  {0x52, Channel_11},
};

Bankable::CCButtonLatched<5> pulsador3 {
  {bank, BankType::ChangeAddress},
  4,
  {0x53, Channel_11},
};

// Pulsador momentáneo (CCButton) - Pulsador 4
Bankable::CCButton pulsador4 {
  {bank, BankType::ChangeAddress},
  5,
  {0x54, Channel_11},
};

// =============================================================================
// POTENCIÓMETROS CON FILTRO SIMPLE
// =============================================================================

using CCSmartPot = Bankable::CCSmartPotentiometer<5>;

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
constexpr analog_t minimumValue = 7500;
constexpr analog_t maximumValue = 12500;


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

  // LED 0: asociado al pulsador del pin 2 (pulsador1) - Latched
  if (pulsador1.getState()) {
    strip.setPixelColor(0, applyBrightness(color, ledBrightness));
  } else {
    strip.setPixelColor(0, 0);
  }

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

  // LED 3: asociado al pulsador del pin 5 (pulsador4) - Momentáneo, siempre encendido
  strip.setPixelColor(3, applyBrightness(color, ledBrightness));

  strip.show();
}

void setup() {

  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);

  delay(50); // pequeña espera para estabilizar

  if (digitalRead(BUTTON_2) == LOW && digitalRead(BUTTON_3) == LOW) {
    // Si ambos pulsadores están presionados, entrar al modo BOOTSEL
    reset_usb_boot(0, 0);
  }
  
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
  static bool lastStates[3] = {false, false, false}; // Solo 3 pulsadores latched
  
  uint8_t currentBank = bank.getSelection();
  bool currentStates[3] = {
    pulsador1.getState(),
    pulsador2.getState(),
    pulsador3.getState()
    // pulsador4 es momentáneo, no necesitamos trackear su estado
  };

  // Verificar si cambió el banco o algún estado de pulsador latched
  bool needsUpdate = false;
  
  if (currentBank != lastBank) {
    needsUpdate = true;
    lastBank = currentBank;
  }
  
  for (int i = 0; i < 3; i++) {
    if (currentStates[i] != lastStates[i]) {
      needsUpdate = true;
      lastStates[i] = currentStates[i];
    }
  }

  // Actualizar LEDs si hubo cambios
  if (needsUpdate) {
    updateBankLeds();
  }

  // Dejar que Control_Surface maneje todos los pulsadores automáticamente
  Control_Surface.loop();
 
  if (potentiometer.getState() == CCSmartPot::Lower) {
    
  } else if (potentiometer.getState() == CCSmartPot::Higher) {
    
  }
}
