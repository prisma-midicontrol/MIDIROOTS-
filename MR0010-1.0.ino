#include <Control_Surface.h>
#include <Adafruit_NeoPixel.h>

#define PIN 16
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Tira de LEDs
#define PIN_STRIP 6
#define NUM_LEDS 4
Adafruit_NeoPixel strip(NUM_LEDS, PIN_STRIP, NEO_GRB + NEO_KHZ800);

// =============================================================================
// CONFIGURACIÓN DE BANCOS - FÁCIL DE MODIFICAR
// =============================================================================

// Configuración de comportamiento por banco
// 0 = CCButton (momentáneo), 1 = CCButtonLatched
struct ButtonBankConfig {
  uint8_t pulsador1_type; // 0 = CCButton, 1 = CCButtonLatched
  uint8_t pulsador2_type;
  uint8_t pulsador3_type;
  uint8_t pulsador4_type;
};

// CONFIGURACIÓN DE CADA BANCO - MODIFICA AQUÍ
// Formato: {pulsador1, pulsador2, pulsador3, pulsador4}
// 0 = Momentáneo (CCButton), 1 = Latched (CCButtonLatched)
ButtonBankConfig bankConfigs[5] = {
  {1, 1, 1, 1}, // Banco 0: todos latched
  {0, 1, 0, 1}, // Banco 1: pulsador1 y pulsador3 momentáneos
  {1, 0, 1, 0}, // Banco 2: pulsador2 y pulsador4 momentáneos
  {0, 0, 1, 1}, // Banco 3: pulsador1 y pulsador2 momentáneos
  {1, 1, 0, 0}  // Banco 4: pulsador3 y pulsador4 momentáneos
};

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
// CONFIGURACIÓN DE NIVELES DE BRILLO - FÁCIL DE MODIFICAR
// =============================================================================

// Niveles de brillo preestablecidos (10%, 20%, 30%, 60%, 100%)
// Modifica estos valores para cambiar los niveles de brillo
const uint8_t BRIGHTNESS_LEVELS[5] = {26, 51, 77, 153, 255};
uint8_t currentBrightnessLevel = 4; // Comenzar en 100% (índice 4)

// =============================================================================
// CONFIGURACIÓN DE PINES - VERIFICAR ANTES DE MODIFICAR
// =============================================================================

// Variables para control de intensidad de LEDs
uint8_t ledBrightness = 255; // Brillo actual (0-255)
bool brightnessMode = false; // Modo de control de brillo activo

// Estados de los pulsadores latched (para tracking)
bool latchedStates[4] = {false, false, false, false};

USBMIDI_Interface midi;
Bank<5> bank(5);

// Selector de banco con LEDs
// Pines: {increment, decrement} = {14, 15}
// LEDs: {9, 10, 11, 12, 13}
IncrementDecrementSelectorLEDs<5> selector {
  bank,
  {14, 15},
  {9, 10, 11, 12, 13}
};

// Pulsadores únicos - todos CCButtonLatched
// Pines: {2, 3, 4, 5} - CORREGIDO: sin conflicto con selector
// CC: {0x51, 0x52, 0x53, 0x54}
Bankable::CCButtonLatched <5> pulsador1 {
  {bank, BankType::ChangeAddress},
  2,
  {0x51, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador2 {
  {bank, BankType::ChangeAddress},
  3,
  {0x52, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador3 {
  {bank, BankType::ChangeAddress},
  4,
  {0x53, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador4 {
  {bank, BankType::ChangeAddress},
  5,
  {0x54, Channel_11},
};

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

// Función para controlar la intensidad de LEDs con niveles preestablecidos
void controlLedBrightness() {
  // Esta función debe ser implementada si se va a usar
  // Por ahora la dejamos vacía para evitar errores de compilación
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

// Actualiza los LEDs de la tira según el banco activo y el estado latched
void updateBankLeds() {
  uint8_t activeBank = bank.getSelection();
  uint32_t color = bankColors[activeBank];
  ButtonBankConfig currentConfig = bankConfigs[activeBank];

  // LED 0: asociado al pulsador del pin 2 (pulsador1)
  if (currentConfig.pulsador1_type == 1) {
    // Es latched
    if (latchedStates[0]) {
      strip.setPixelColor(0, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(0, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(0, applyBrightness(color, ledBrightness));
  }

  // LED 1: asociado al pulsador del pin 3 (pulsador2)
  if (currentConfig.pulsador2_type == 1) {
    // Es latched
    if (latchedStates[1]) {
      strip.setPixelColor(1, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(1, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(1, applyBrightness(color, ledBrightness));
  }

  // LED 2: asociado al pulsador del pin 4 (pulsador3)
  if (currentConfig.pulsador3_type == 1) {
    // Es latched
    if (latchedStates[2]) {
      strip.setPixelColor(2, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(2, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(2, applyBrightness(color, ledBrightness));
  }

  // LED 3: asociado al pulsador del pin 5 (pulsador4)
  if (currentConfig.pulsador4_type == 1) {
    // Es latched
    if (latchedStates[3]) {
      strip.setPixelColor(3, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(3, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(3, applyBrightness(color, ledBrightness));
  }

  strip.show();
}

void setup() {
  pixels.begin();
  pixels.show();

  strip.begin();
  strip.show();

  animateRainbow(); // Animación de inicio

  Control_Surface.begin();
  potentiometer.map(mappingFunction);
  potentiometer2.map(mappingFunction);
}

void loop() {
  // Mantener el LED individual en blanco después de la animación (solo si no está en modo de brillo)
  if (!brightnessMode) {
    pixels.setPixelColor(0, pixels.Color(255, 255, 255));
    pixels.show();
  }

  // Solo actualizar LEDs cuando haya cambios
  static uint8_t lastBank = 255;
  
  uint8_t currentBank = bank.getSelection();

  // Solo actualizar si cambió el banco
  if (currentBank != lastBank) {
    updateBankLeds();
    lastBank = currentBank;
  }

  // Dejar que Control_Surface maneje todos los pulsadores automáticamente
  Control_Surface.loop();

  // Actualizar LEDs según el estado de los pulsadores latched
  updateLedsFromButtonStates();

  // Indicadores de estado del potenciómetro (opcional)
  if (potentiometer.getState() == CCSmartPot::Lower) {
    // Aquí puedes encender un LED si el valor es muy bajo
  } else if (potentiometer.getState() == CCSmartPot::Higher) {
    // Aquí puedes encender un LED si el valor es muy alto
  }
}

// Función simple para actualizar LEDs según el estado de los pulsadores
void updateLedsFromButtonStates() {
  uint8_t activeBank = bank.getSelection();
  ButtonBankConfig currentConfig = bankConfigs[activeBank];
  bool statesChanged = false;

  // Actualizar estados de LEDs según la configuración del banco
  if (currentConfig.pulsador1_type == 1) {
    bool newState = pulsador1.getState();
    if (newState != latchedStates[0]) {
      latchedStates[0] = newState;
      statesChanged = true;
    }
  } else {
    if (latchedStates[0]) {
      latchedStates[0] = false;
      statesChanged = true;
    }
  }
  
  if (currentConfig.pulsador2_type == 1) {
    bool newState = pulsador2.getState();
    if (newState != latchedStates[1]) {
      latchedStates[1] = newState;
      statesChanged = true;
    }
  } else {
    if (latchedStates[1]) {
      latchedStates[1] = false;
      statesChanged = true;
    }
  }
  
  if (currentConfig.pulsador3_type == 1) {
    bool newState = pulsador3.getState();
    if (newState != latchedStates[2]) {
      latchedStates[2] = newState;
      statesChanged = true;
    }
  } else {
    if (latchedStates[2]) {
      latchedStates[2] = false;
      statesChanged = true;
    }
  }
  
  if (currentConfig.pulsador4_type == 1) {
    bool newState = pulsador4.getState();
    if (newState != latchedStates[3]) {
      latchedStates[3] = newState;
      statesChanged = true;
    }
  } else {
    if (latchedStates[3]) {
      latchedStates[3] = false;
      statesChanged = true;
    }
  }

  // Actualizar LEDs si hubo cambios
  if (statesChanged) {
    updateBankLeds();
  }
}