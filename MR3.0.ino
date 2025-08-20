#include <Control_Surface.h>
#include <Adafruit_NeoPixel.h>

#define PIN 16
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Tira de LEDs
#define PIN_STRIP 8
#define NUM_LEDS 4
Adafruit_NeoPixel strip(NUM_LEDS, PIN_STRIP, NEO_GRB + NEO_KHZ800);

// =============================================================================
// CONFIGURACIÓN DE BANCOS 
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

// Color fijo para el LED del increment button (LED 3)
uint32_t incrementButtonColor = strip.Color(255, 255, 255); // Blanco

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
// Pines: {increment, decrement} = {2, 14}
// LEDs: {9, 10, 11, 12, 13}
IncrementDecrementSelectorLEDs<5> selector {
  bank,
  {2, 14},
  {9, 10, 11, 12, 13}
};

// Pulsadores únicos - todos CCButtonLatched
// Pines: {3, 4, 5, 6}
// CC: {0x51, 0x52, 0x53, 0x54}
Bankable::CCButtonLatched <5> pulsador1 {
  {bank, BankType::ChangeAddress},
  3,
  {0x51, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador2 {
  {bank, BankType::ChangeAddress},
  4,
  {0x52, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador3 {
  {bank, BankType::ChangeAddress},
  5,
  {0x53, Channel_11},
};

Bankable::CCButtonLatched <5> pulsador4 {
  {bank, BankType::ChangeAddress},
  6,
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
  // Verificar si el selector de bancos está presionado
  static bool lastSelectorState = false;
  static bool lastPulsador3State = false;
  static bool brightnessModeActive = false;
  static unsigned long pressStartTime = 0;
  static bool modeActivated = false;
  
  bool currentSelectorState = digitalRead(2) == LOW; // LOW cuando está presionado
  bool currentPulsador3State = digitalRead(5) == LOW; // LOW cuando está presionado (pulsador3)
  
  // Detectar cuando se presiona el selector
  if (currentSelectorState && !lastSelectorState) {
    pressStartTime = millis();
    modeActivated = false;
  }
  
  // Detectar cuando se suelta el selector
  if (!currentSelectorState && lastSelectorState) {
    brightnessModeActive = false;
    brightnessMode = false;
    modeActivated = false;
    
    // Solo mostrar indicador verde si estaba en modo de brillo
    if (brightnessModeActive) {
      // Indicador visual: parpadear el LED individual en verde
      pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Verde
      pixels.show();
      delay(200);
      pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // Volver a blanco
      pixels.show();
    }
  }
  
  // Verificar si se mantiene presionado por 5 segundos
  if (currentSelectorState && !modeActivated && (millis() - pressStartTime > 5000)) {
    brightnessModeActive = true;
    modeActivated = true;
    
    // Indicador visual: cambiar el LED individual a violeta
    pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Violeta
    pixels.show();
  }
  
  // Si el selector está presionado y el modo está activado, activar modo de brillo
  if (currentSelectorState && brightnessModeActive) {
    brightnessMode = true;
    
    // Mantener el LED en violeta durante el modo de brillo
    pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Violeta
    pixels.show();
    
    // Detectar cuando se presiona el pulsador3
    if (currentPulsador3State && !lastPulsador3State) {
      // Cambiar al siguiente nivel de brillo
      currentBrightnessLevel = (currentBrightnessLevel + 1) % 5;
      ledBrightness = BRIGHTNESS_LEVELS[currentBrightnessLevel];
      
      // Actualizar LEDs con nuevo brillo
      updateBankLeds();
      
      // Indicador visual: mostrar el nivel actual brevemente
      uint8_t indicatorBrightness = map(ledBrightness, 0, 255, 0, 255);
      pixels.setPixelColor(0, pixels.Color(indicatorBrightness, indicatorBrightness, indicatorBrightness));
      pixels.show();
      delay(100);
      pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Volver a violeta
      pixels.show();
    }
  }
  
  lastSelectorState = currentSelectorState;
  lastPulsador3State = currentPulsador3State;
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
    default: r = 255; g = 0; b = q; break;
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

  // LED 0: pulsador3
  if (currentConfig.pulsador3_type == 1) {
    // Es latched
    if (latchedStates[2]) {
      strip.setPixelColor(0, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(0, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(0, applyBrightness(color, ledBrightness));
  }

  // LED 1: pulsador2
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

  // LED 2: pulsador1
  if (currentConfig.pulsador1_type == 1) {
    // Es latched
    if (latchedStates[0]) {
      strip.setPixelColor(2, applyBrightness(color, ledBrightness));
    } else {
      strip.setPixelColor(2, 0);
    }
  } else {
    // Es momentáneo - siempre encendido con color del banco
    strip.setPixelColor(2, applyBrightness(color, ledBrightness));
  }

  // LED 3: increment button - color fijo
  strip.setPixelColor(3, applyBrightness(incrementButtonColor, ledBrightness));

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

  // Controlar la intensidad de los LEDs
  controlLedBrightness();

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

// =============================================================================
// GUÍA DE MODIFICACIÓN - LEE ESTO ANTES DE CAMBIAR ALGO
// =============================================================================

/*
CÓMO MODIFICAR LA CONFIGURACIÓN DE BANCOS:

1. CONFIGURACIÓN DE PULSADORES POR BANCO:
   En la sección "CONFIGURACIÓN DE CADA BANCO", modifica el array bankConfigs:
   
   Formato: {pulsador1, pulsador2, pulsador3, pulsador4}
   - 0 = Momentáneo (CCButton)
   - 1 = Latched (CCButtonLatched)
   
   Ejemplo:
   {1, 0, 1, 0} = pulsador1 y pulsador3 latched, pulsador2 y pulsador4 momentáneos

2. CONFIGURACIÓN DE COLORES DE BANCOS:
   En la sección "CONFIGURACIÓN DE COLORES DE BANCOS", modifica bankColors:
   
   strip.Color(R, G, B) donde R, G, B van de 0 a 255
   
   Ejemplo:
   strip.Color(255, 0, 0)     // Rojo
   strip.Color(0, 255, 0)     // Verde
   strip.Color(0, 0, 255)     // Azul
   strip.Color(255, 255, 0)   // Amarillo
   strip.Color(255, 0, 255)   // Magenta
   strip.Color(0, 255, 255)   // Cian

3. CONFIGURACIÓN DE NIVELES DE BRILLO:
   En la sección "CONFIGURACIÓN DE NIVELES DE BRILLO", modifica BRIGHTNESS_LEVELS:
   
   Los valores van de 0 (apagado) a 255 (máximo brillo)
   
   Ejemplo para 5 niveles: 20%, 40%, 60%, 80%, 100%
   {51, 102, 153, 204, 255}

4. CONFIGURACIÓN DE PINES:
   ⚠️ CUIDADO: Solo modifica si sabes lo que haces
   
   - Selector de banco: pines 2 y 14
   - LEDs del selector: pines 9, 10, 11, 12, 13
   - Pulsadores: pines 3, 4, 5, 6
   - LED individual: pin 16
   - Tira de LEDs: pin 8
   - Potenciómetros: A0, A1

5. CONFIGURACIÓN MIDI:
   - Canal: Channel_11 (cambia si necesitas otro canal)
   - CC: 0x51, 0x52, 0x53, 0x54 (cambia si necesitas otros CC)

CÓMO USAR EL SISTEMA:

1. CAMBIO DE BANCO:
   - Usa los botones del selector de banco (pines 2 y 14)
   - Los LEDs del selector muestran el banco activo

2. CONTROL DE BRILLO:
   - Mantén presionado el selector de banco (pin 2) durante 5 segundos
   - El LED individual se pondrá violeta
   - Presiona el pulsador3 (pin 5) para cambiar niveles de brillo
   - Suelta el selector para salir del modo

3. PULSADORES:
   - Los pulsadores funcionan según la configuración del banco activo
   - Los LEDs reflejan el estado de los pulsadores latched
   - Los pulsadores momentáneos mantienen el LED encendido

4. POTENCIÓMETROS:
   - Funcionan normalmente en todos los bancos
   - Envían CC General Purpose Controller 1 y 2

ESTRUCTURA DE ARCHIVO:
- Líneas 1-50:   Configuración de bancos y colores
- Líneas 51-100: Configuración de brillo y pines
- Líneas 101-150: Definición de objetos Control_Surface
- Líneas 151-200: Funciones de utilidad
- Líneas 201-250: Funciones de control de LEDs
- Líneas 251-300: Funciones de control de brillo
- Líneas 301-350: Funciones de actualización de LEDs
- Líneas 351-400: Setup y loop principales
- Líneas 401+:   Guía de modificación
*/