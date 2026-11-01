/*
 * STM32 ARGB Controller v2 - Multi-Ring LED Controller
 * For Video Launcher project
 *
 * Hardware: STM32F103C8T6 (Blue Pill)
 *
 * LED Sections:
 * - Ring External LEFT  (45 LED) - PA0
 * - Ring External RIGHT (45 LED) - PA1
 * - Ring Internal LEFT  (35 LED) - PA2
 * - Ring Internal RIGHT (35 LED) - PA3
 * - Fans (existing)             - PB8
 *
 * Buttons:
 * - BTN1 (Audio switch)  - PB12
 * - BTN2 (LED control)   - PB13
 *
 * Audio Output Pins (2 HIGH / 2 LOW toggle):
 * - PB14, PB15 = pair A
 * - PA4, PA5   = pair B
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <IWatchdog.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// LED Rings
#define PIN_EXT_LEFT    PA0
#define PIN_EXT_RIGHT   PA1
#define PIN_INT_LEFT    PA2
#define PIN_INT_RIGHT   PA3
#define PIN_FANS        PB0   // LED fans data
#define PIN_FAN_PWM     PB9   // Fan speed PWM (25kHz)

// Buttons (active LOW with internal pullup)
#define PIN_BTN1        PB12    // Audio switch
#define PIN_BTN2        PB13    // LED control

// Audio output pins
#define PIN_AUDIO_A1    PB14
#define PIN_AUDIO_A2    PB15
#define PIN_AUDIO_B1    PA4
#define PIN_AUDIO_B2    PA5

// Onboard LED
#define PIN_ONBOARD_LED PC13

// ============================================================================
// LED CONFIGURATION
// ============================================================================

#define LEDS_EXT        45      // External ring LED count
#define LEDS_INT        35      // Internal ring LED count
#define LEDS_FANS       22   // Fan LED count

// Fan speed modes
#define FAN_OFF     0
#define FAN_SILENT  1
#define FAN_NORMAL  2
#define FAN_MAX     3
#define FAN_AUTO    4
#define FAN_MANUAL  5
#define MAX_LEDS        45      // Max for arrays

// ============================================================================
// GAME CONSTANTS
// ============================================================================
#define RACE_LAPS           10      // Iloœæ okr¹¿eñ do wygranej
#define FINISH_LINE_POS     0       // Pozycja mety (LED 0-1)
#define CAR_SIZE            3       // Rozmiar samochodu w LEDach
#define CLICK_SPEED_BOOST   45      // Przyspieszenie per klik
#define CAR_FRICTION        1       // Tarcie (wolne spowalnianie)
#define CAR_MAX_SPEED       150     // Maksymalna prêdkoœæ

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

#define DEBOUNCE_MS         50
#define LONG_PRESS_MS       600
#define MULTI_CLICK_MS      700
#define INDICATOR_SHOW_MS   800

// ============================================================================
// ENUMS
// ============================================================================

// LED Programs
enum Program {
    PROG_OFF = 0,
    PROG_RAINBOW,
    PROG_PULSE,
    PROG_SOLID,
    PROG_WAVE,
    PROG_BREATHING,
    PROG_SPARKLE,
    PROG_GRADIENT,
    PROG_TRICOLOR,    // Trzy kolory goni¹ce siê
    PROG_METEOR,      // Kometa z ogonem
    PROG_THEATER,     // Theater chase
    PROG_SCANNER,     // Knight Rider
    PROG_TWINKLE,     // Migaj¹ce gwiazdki
    PROG_PLASMA,      // Plasma colors
    PROG_STROBE,      // Stroboskop
    PROG_CHASE,       // 5 LED grupki tam-powrót
    PROG_HEARTBEAT,   // Bicie serca
    PROG_BOUNCE,      // Odbijaj¹ce siê pi³ki
    PROG_FILL,        // Wypelnianie LED po kolei
    PROG_CUSTOM,
    PROG_COUNT
};

// Color variants
enum ColorVariant {
    VAR_DEFAULT = 0,
    VAR_WHITE,
    VAR_RED,
    VAR_GREEN,
    VAR_BLUE,
    VAR_PINK,
    VAR_PURPLE,
    VAR_CYAN,
    VAR_ORANGE,
    VAR_WARM,       // Red-orange
    VAR_COLD,       // Blue-purple
    VAR_NATURE,     // Green-cyan
    VAR_SYNTHWAVE,  // Pink-purple
    VAR_COUNT
};

// LED Sections for selection
enum Section {
    SEC_ALL = 0,
    SEC_FANS,
    SEC_EXT_RING,   // Both left and right
    SEC_INT_RING,   // Both left and right
    SEC_COUNT
};

// Main state machine
enum State {
    STATE_NORMAL,
    STATE_SPEED,
    STATE_BRIGHTNESS,
    STATE_COLOR,
    STATE_SELECT_TARGET,    // Wybór sekcji do edycji (5-klik)
    STATE_SELECT_SECTION,
    STATE_EDIT_SECTION,
    STATE_GAME
};

// Game sub-states
enum GameState {
    GAME_COUNTDOWN,     // Odliczanie: ¿ó³ty-stop-¿ó³ty-stop-¿ó³ty-zielony
    GAME_RACING,        // Wyœcig trwa
    GAME_WINNER,        // Pokazanie zwyciêzcy
    GAME_EXIT_CHECK     // Sprawdzanie czy wyjœæ (oba przyciski 5s)
};

// Button events
enum ButtonEvent {
    BTN_NONE = 0,
    BTN_CLICK,      // Single confirmed click
    BTN_DOUBLE,     // 2 clicks
    BTN_TRIPLE,     // 3 clicks
    BTN_QUAD,       // 4 clicks
    BTN_QUINT,      // 5 clicks
    BTN_SEXT,       // 6 clicks - auto mode
    BTN_LONG,       // Long press started
    BTN_RELEASE     // Released after long press
};

// Edit target - która sekcja jest edytowana
enum EditTarget {
    TARGET_ALL = 0,     // Wszystko (ringi + fany)
    TARGET_BOTH,        // Oba ringi (zewnêtrzna + wewnêtrzna)
    TARGET_EXT,         // Tylko zewnêtrzna
    TARGET_INT,         // Tylko wewnêtrzna
    TARGET_FANS,        // Tylko fany
    TARGET_COUNT
};

// ============================================================================
// STRUCTURES
// ============================================================================

struct ButtonState {
    bool pressed;
    bool lastState;
    uint32_t pressTime;
    uint32_t releaseTime;
    uint8_t clickCount;
    bool longFired;
    bool holding;
};

struct SectionConfig {
    Program program;
    ColorVariant variant;
    uint8_t speed;          // 0-4 (5 levels)
    uint8_t brightness;     // 0-255
};

// ============================================================================
// LED STRIP OBJECTS
// ============================================================================

Adafruit_NeoPixel extLeft(LEDS_EXT, PIN_EXT_LEFT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel extRight(LEDS_EXT, PIN_EXT_RIGHT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel intLeft(LEDS_INT, PIN_INT_LEFT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel intRight(LEDS_INT, PIN_INT_RIGHT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel fans(LEDS_FANS, PIN_FANS, NEO_GRB + NEO_KHZ800);

// Hardware timer for fan PWM
HardwareTimer *fanTimer;

// Fan control
uint8_t fanMode = FAN_NORMAL;
uint8_t fanSpeed = 0;
uint8_t fanManualSpeed = 128;
uint8_t fanTargetSpeed = 0;

// Fan LED config
SectionConfig configFans = {PROG_RAINBOW, VAR_DEFAULT, 5, 200};

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Buttons
ButtonState btn1 = {0};
ButtonState btn2 = {0};

// State machine
State currentState = STATE_NORMAL;
Section selectedSection = SEC_ALL;
EditTarget editTarget = TARGET_ALL;  // Który target edytujemy (5-klik)
uint32_t stateEnterTime = 0;

// Section configs (CUSTOM mode stores individual settings)
// SectionConfig configFans = {PROG_RAINBOW, VAR_DEFAULT, 5, 200};  // Zakomentowane
SectionConfig configExtRing = {PROG_RAINBOW, VAR_DEFAULT, 5, 200};
SectionConfig configIntRing = {PROG_RAINBOW, VAR_DEFAULT, 5, 200};

// ============================================================================
// GAME STATE
// ============================================================================
GameState gameState = GAME_COUNTDOWN;
uint32_t gameStateTime = 0;         // Czas wejœcia do aktualnego stanu gry
uint8_t countdownStep = 0;          // Krok odliczania (0-6)

// Pozycje samochodów (fixed point: pozycja * 256)
int32_t car1Pos = 0;                // Gracz 1 (czerwony) - BTN1
int32_t car2Pos = 0;                // Gracz 2 (zielony) - BTN2
int16_t car1Speed = 0;              // Prêdkoœæ gracza 1
int16_t car2Speed = 0;              // Prêdkoœæ gracza 2
uint8_t car1Laps = 0;               // Okr¹¿enia gracza 1
uint8_t car2Laps = 0;               // Okr¹¿enia gracza 2
bool car1CrossedStart = false;      // Czy przekroczy³ start (¿eby liczyæ okr¹¿enia)
bool car2CrossedStart = false;
uint8_t winner = 0;                 // 0=brak, 1=gracz1, 2=gracz2
uint32_t gameExitHoldStart = 0;     // Czas przytrzymania obu przycisków do wyjœcia

// Prêdkoœci per program per ring (0-9)
uint8_t extProgSpeeds[PROG_COUNT];  // Prêdkoœæ dla ka¿dego efektu na EXT
uint8_t intProgSpeeds[PROG_COUNT];  // Prêdkoœæ dla ka¿dego efektu na INT
uint8_t fanProgSpeeds[PROG_COUNT];  // Prêdkoœæ dla ka¿dego efektu na FAN

// Global program (for non-CUSTOM mode)
Program globalProgram = PROG_RAINBOW;
ColorVariant globalVariant = VAR_DEFAULT;
uint8_t globalSpeed = 5;        // 0-9 (domyœlnie œrodek)
uint8_t globalBrightness = 200;

// Audio state
bool audioStateA = true;    // true = A high, B low
bool lastBtn1State = false; // For instant audio toggle
bool audioNeedsSave = false; // Delayed EEPROM save
uint32_t audioSaveTime = 0;  // When to save

// USB Serial buffer
#define USB_BUFFER_SIZE 64
char usbBuffer[USB_BUFFER_SIZE];
int usbBufferIndex = 0;
uint32_t lastSerialCheck = 0;

// Auto mode - automatyczna zmiana trybu co 7 sek
bool autoModeEnabled = false;
uint32_t lastAutoChange = 0;
#define AUTO_CHANGE_MS 7000

// Animation state - osobne dla ka¿dego ringu
uint32_t lastAnimUpdateExt = 0;
uint32_t lastAnimUpdateInt = 0;
uint16_t animOffsetExt = 0;
uint16_t animOffsetInt = 0;
uint8_t pulseValueExt = 0;
uint8_t pulseValueInt = 0;
uint8_t pulseValueFan = 0;
int8_t pulseDirExt = 1;
int8_t pulseDirInt = 1;
int8_t pulseDirFan = 1;
uint32_t lastAnimUpdateFan = 0;
uint16_t animOffsetFan = 0;
uint8_t sparkleTimer = 0;

// Current animation context (ustawiane przed applyEffect)
uint16_t animOffset = 0;
uint8_t pulseValue = 0;

// Temporary display state
bool showingIndicator = false;
uint32_t indicatorStartTime = 0;
uint8_t indicatorValue = 0;

// Selection blink state
uint32_t lastBlinkTime = 0;
bool blinkOn = false;


// ============================================================================
// COLOR DEFINITIONS
// ============================================================================

// Basic colors (R, G, B)
const uint8_t COLORS[][3] = {
    {255, 255, 255},    // VAR_WHITE
    {255, 0, 0},        // VAR_RED
    {0, 255, 0},        // VAR_GREEN
    {0, 0, 255},        // VAR_BLUE
    {255, 50, 150},     // VAR_PINK
    {150, 0, 255},      // VAR_PURPLE
    {0, 255, 255},      // VAR_CYAN
    {255, 100, 0},      // VAR_ORANGE
};

// Rainbow variants - start/end hues for gradient
const uint8_t RAINBOW_VARIANTS[][2] = {
    {0, 255},       // VAR_DEFAULT - full spectrum
    {0, 40},        // VAR_WARM - red to orange
    {140, 200},     // VAR_COLD - blue to purple
    {75, 130},      // VAR_NATURE - green to cyan
    {200, 240},     // VAR_SYNTHWAVE - pink to purple
};

// Speed multipliers (lower = faster) - 10 levels
const uint16_t SPEED_DELAYS[] = {150, 100, 70, 45, 30, 20, 12, 8, 5, 0};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void setupPins();
void loadSettings();
void saveSettings();

ButtonEvent processButton(ButtonState* btn, bool currentPressed);
void handleButton1(ButtonEvent event);
void handleButton2(ButtonEvent event);
void checkBothButtons();
SectionConfig* getConfigForSection(Section sec);

void updateLEDs();
void updateSection(Adafruit_NeoPixel* strip, int numLeds, SectionConfig* config);
void applyEffect(Adafruit_NeoPixel* strip, int numLeds, Program prog, ColorVariant var, uint8_t speed);

void effectOff(Adafruit_NeoPixel* strip, int numLeds);
void effectRainbow(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectPulse(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectSolid(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectWave(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectBreathing(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectSparkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectGradient(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectTricolor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectMeteor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectTheater(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectScanner(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectTwinkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectPlasma(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectStrobe(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectChase(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectHeartbeat(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectBounce(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);
void effectFill(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);

void showSpeedIndicator(uint8_t level);
void showBrightnessLevel();
void showSectionSelection(Section sec);
void showTargetSelection(EditTarget target);
void showVariantPreview(ColorVariant var);
void blinkSection(Section sec, uint8_t r, uint8_t g, uint8_t b);

uint32_t colorWheel(Adafruit_NeoPixel* strip, byte pos);
uint32_t colorFromVariant(Adafruit_NeoPixel* strip, ColorVariant var);
void getVariantColor(ColorVariant var, uint8_t* r, uint8_t* g, uint8_t* b);
void dimColorProportional(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t brightness);

void applyAudioPins();
void toggleAudio();

// USB command functions
void processUsbCommand(const char* cmd);
int getParam(const char* cmd, int paramIndex);
bool cmdIs(const char* cmd, const char* prefix);
void sendResponse(const char* msg);

// Fan control functions
void updateFan();
void setFanPWM(uint8_t speed);

// Game functions
void initGame();
void updateGame();
void renderGame();
void handleGameInput(bool btn1Click, bool btn2Click);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Blue Pill USB Reset - pull D+ (PA12) low to force USB disconnect/reconnect
    // Dluzsza pauza dla pewniejszego resetu USB
    pinMode(PA12, OUTPUT);
    digitalWrite(PA12, LOW);
    delay(100);  // 100ms low
    pinMode(PA12, INPUT);
    delay(100);  // 100ms wait

    // Watchdog - 4 second timeout
    IWatchdog.begin(4000000);

    // USB Serial
    SerialUSB.begin(115200);
    delay(200);  // Wiecej czasu na enumeracje

    setupPins();

    // Initialize LED strips
    extLeft.begin();
    extRight.begin();
    intLeft.begin();
    intRight.begin();
    fans.begin();

    // Fan PWM at 25kHz (for 4-pin fans)
    TIM_TypeDef *Instance = TIM4;
    fanTimer = new HardwareTimer(Instance);
    fanTimer->setMode(4, TIMER_OUTPUT_COMPARE_PWM1, PIN_FAN_PWM);
    fanTimer->setOverflow(25000, HERTZ_FORMAT);
    fanTimer->setCaptureCompare(4, 0, PERCENT_COMPARE_FORMAT);
    fanTimer->resume();

    // Set initial brightness
    extLeft.setBrightness(globalBrightness);
    extRight.setBrightness(globalBrightness);
    intLeft.setBrightness(globalBrightness);
    intRight.setBrightness(globalBrightness);
    fans.setBrightness(configFans.brightness);

    // Clear all
    extLeft.clear(); extLeft.show();
    extRight.clear(); extRight.show();
    intLeft.clear(); intLeft.show();
    intRight.clear(); intRight.show();
    fans.clear(); fans.show();

    // Set initial fan speed
    updateFan();

    // Load saved settings
    loadSettings();

    // Startup animation - cyan flash + RED fans test
    for (int i = 0; i < LEDS_EXT; i++) {
        extLeft.setPixelColor(i, extLeft.Color(0, 255, 255));
        extRight.setPixelColor(i, extRight.Color(0, 255, 255));
    }
    for (int i = 0; i < LEDS_INT; i++) {
        intLeft.setPixelColor(i, intLeft.Color(0, 255, 255));
        intRight.setPixelColor(i, intRight.Color(0, 255, 255));
    }
    for (int i = 0; i < LEDS_FANS; i++) {
        fans.setPixelColor(i, fans.Color(255, 0, 0));  // RED = test PB0
    }
    extLeft.show(); extRight.show();
    intLeft.show(); intRight.show();
    fans.show();
    delay(500);

    // Clear and start normal operation
    extLeft.clear(); extLeft.show();
    extRight.clear(); extRight.show();
    intLeft.clear(); intLeft.show();
    intRight.clear(); intRight.show();
    fans.clear(); fans.show();
}

void setupPins() {
    // Buttons with internal pullup
    pinMode(PIN_BTN1, INPUT_PULLUP);
    pinMode(PIN_BTN2, INPUT_PULLUP);

    // Audio output pin (only PA4 for single relay)
    pinMode(PIN_AUDIO_B1, OUTPUT);
    digitalWrite(PIN_AUDIO_B1, LOW);  // Initial state A

    // Onboard LED
    pinMode(PIN_ONBOARD_LED, OUTPUT);
    digitalWrite(PIN_ONBOARD_LED, HIGH); // OFF
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t now = millis();

    // Feed watchdog
    IWatchdog.reload();

    // USB Serial - check every 20ms
    if (now - lastSerialCheck >= 20) {
        lastSerialCheck = now;
        int avail = SerialUSB.available();
        if (avail > 0) {
            for (int i = 0; i < 8 && avail > 0; i++, avail--) {
                char c = SerialUSB.read();
                if (c == '\n' || c == '\r') {
                    if (usbBufferIndex > 0) {
                        usbBuffer[usbBufferIndex] = '\0';
                        processUsbCommand(usbBuffer);
                        usbBufferIndex = 0;
                    }
                } else if (usbBufferIndex < USB_BUFFER_SIZE - 1) {
                    usbBuffer[usbBufferIndex++] = c;
                } else {
                    usbBufferIndex = 0;
                }
            }
        }
    }

    // Read buttons
    bool btn1Pressed = !digitalRead(PIN_BTN1);  // Active LOW
    bool btn2Pressed = !digitalRead(PIN_BTN2);

    // === GAME MODE ===
    if (currentState == STATE_GAME) {
        // SprawdŸ wyjœcie z gry (oba przyciski 5 sek)
        if (btn1Pressed && btn2Pressed) {
            if (gameExitHoldStart == 0) {
                gameExitHoldStart = now;
            } else if (now - gameExitHoldStart >= 5000) {
                // WyjdŸ z gry - wyczyœæ wszystko i przywróæ jasnoœæ
                extLeft.clear();
                extRight.clear();
                intLeft.clear();
                intRight.clear();
    fans.clear();
                extLeft.setBrightness(configExtRing.brightness);
                extRight.setBrightness(configExtRing.brightness);
                intLeft.setBrightness(configIntRing.brightness);
                intRight.setBrightness(configIntRing.brightness);
                extLeft.show();
                extRight.show();
                intLeft.show();
                intRight.show();
                currentState = STATE_NORMAL;
                gameExitHoldStart = 0;
                return;
            }
        } else {
            gameExitHoldStart = 0;
        }

        // Wykryj klikniêcia (proste - zmiana z pressed na released)
        static bool lastBtn1 = false;
        static bool lastBtn2 = false;
        bool btn1Click = (!btn1Pressed && lastBtn1);  // Puszczenie = klik
        bool btn2Click = (!btn2Pressed && lastBtn2);
        lastBtn1 = btn1Pressed;
        lastBtn2 = btn2Pressed;

        handleGameInput(btn1Click, btn2Click);
        updateGame();
        renderGame();

        extLeft.show();
        extRight.show();
        intLeft.show();
        intRight.show();
        return;
    }

    // === NORMAL MODE ===

    // Instant audio toggle on BTN1 release (no multi-click delay)
    if (currentState != STATE_GAME) {
        if (lastBtn1State && !btn1Pressed) {
            // Button just released - toggle audio immediately
            toggleAudio();
            // Schedule delayed save (don't block here)
            audioNeedsSave = true;
            audioSaveTime = now + 2000; // Save after 2 sec idle
        }
        lastBtn1State = btn1Pressed;
    }

    // Delayed EEPROM save for audio state
    if (audioNeedsSave && now >= audioSaveTime) {
        saveSettings();
        audioNeedsSave = false;
    }

    // Process button events (BTN2 only needs this now)
    ButtonEvent event1 = processButton(&btn1, btn1Pressed);
    ButtonEvent event2 = processButton(&btn2, btn2Pressed);

    // Check for both buttons held (game mode)
    checkBothButtons();

    // Handle button events
    if (event1 != BTN_NONE) handleButton1(event1);
    if (event2 != BTN_NONE) handleButton2(event2);

    // Clear indicator after timeout
    if (showingIndicator && (now - indicatorStartTime >= INDICATOR_SHOW_MS)) {
        showingIndicator = false;
    }

    // Auto mode - zmiana trybu co 7 sekund
    if (autoModeEnabled) {
        if (now - lastAutoChange >= AUTO_CHANGE_MS) {
            lastAutoChange = now;
            Program nextProg = (Program)((configExtRing.program + 1) % PROG_COUNT);
            if (nextProg == PROG_OFF) nextProg = PROG_RAINBOW;
            if (nextProg == PROG_CUSTOM) nextProg = PROG_RAINBOW;
            configExtRing.program = nextProg;
            configIntRing.program = nextProg;
            globalProgram = nextProg;
        }
    }

    // Update LEDs - osobne animacje dla EXT i INT
    static const uint8_t SPEED_STEPS[] = {1, 1, 1, 1, 2, 2, 3, 4, 6, 10};

    uint8_t extSpeed = extProgSpeeds[configExtRing.program];
    uint16_t delayExt = SPEED_DELAYS[extSpeed];
    if (now - lastAnimUpdateExt >= delayExt) {
        lastAnimUpdateExt = now;
        animOffsetExt += SPEED_STEPS[extSpeed];
        uint8_t pulseStep = 3 + extSpeed;
        pulseValueExt += pulseDirExt * pulseStep;
        if (pulseValueExt >= 250) { pulseValueExt = 255; pulseDirExt = -1; }
        else if (pulseValueExt <= 5) { pulseValueExt = 0; pulseDirExt = 1; }
    }

    uint8_t intSpeed = intProgSpeeds[configIntRing.program];
    uint16_t delayInt = SPEED_DELAYS[intSpeed];
    if (now - lastAnimUpdateInt >= delayInt) {
        lastAnimUpdateInt = now;
        animOffsetInt += SPEED_STEPS[intSpeed];
        uint8_t pulseStep = 3 + intSpeed;
        pulseValueInt += pulseDirInt * pulseStep;
        if (pulseValueInt >= 250) { pulseValueInt = 255; pulseDirInt = -1; }
        else if (pulseValueInt <= 5) { pulseValueInt = 0; pulseDirInt = 1; }
    }

    // Fan LED animation - osobna prêdkoœæ (u¿ywamy fanAnimSpd ¿eby nie przes³aniaæ globalnej fanSpeed)
    uint8_t fanAnimSpd = fanProgSpeeds[configFans.program];
    uint16_t delayFan = SPEED_DELAYS[fanAnimSpd];
    if (now - lastAnimUpdateFan >= delayFan) {
        lastAnimUpdateFan = now;
        animOffsetFan += SPEED_STEPS[fanAnimSpd];
        uint8_t pulseStep = 3 + fanAnimSpd;
        pulseValueFan += pulseDirFan * pulseStep;
        if (pulseValueFan >= 250) { pulseValueFan = 255; pulseDirFan = -1; }
        else if (pulseValueFan <= 5) { pulseValueFan = 0; pulseDirFan = 1; }
    }

    if (!showingIndicator) {
        updateLEDs();
    }

    extLeft.show();
    extRight.show();
    intLeft.show();
    intRight.show();
    fans.show();

    // Update fan PWM (smooth transitions)
    static uint32_t lastFanUpdate = 0;
    if (now - lastFanUpdate >= 100) {
        lastFanUpdate = now;
        updateFan();
    }
}

// ============================================================================
// BUTTON PROCESSING
// ============================================================================

ButtonEvent processButton(ButtonState* btn, bool currentPressed) {
    uint32_t now = millis();
    ButtonEvent event = BTN_NONE;

    // Debounce - state change
    if (currentPressed != btn->lastState) {
        btn->lastState = currentPressed;

        if (currentPressed) {
            // Button pressed
            btn->pressTime = now;
            btn->longFired = false;
        } else {
            // Button released
            btn->releaseTime = now;

            if (!btn->longFired) {
                // W trybach edycji wartoœci - natychmiastowy klik bez czekania
                if (currentState == STATE_SPEED || currentState == STATE_BRIGHTNESS ||
                    currentState == STATE_COLOR) {
                    event = BTN_CLICK;
                    btn->clickCount = 0;
                } else {
                    // Normalnie - zlicz klik i czekaj na wiêcej
                    btn->clickCount++;
                }
            } else {
                // Long press released
                event = BTN_RELEASE;
                btn->clickCount = 0;
            }
        }
        return event;
    }

    // Check for long press (while held)
    if (currentPressed && !btn->longFired) {
        if (now - btn->pressTime >= LONG_PRESS_MS) {
            btn->longFired = true;
            btn->clickCount = 0;
            event = BTN_LONG;
            return event;
        }
    }

    // Check for multi-click timeout (button not pressed, has clicks)
    if (!currentPressed && btn->clickCount > 0) {
        if (now - btn->releaseTime >= MULTI_CLICK_MS) {
            // Timeout - report click count
            if (btn->clickCount >= 6) {
                event = BTN_SEXT;
            } else if (btn->clickCount == 5) {
                event = BTN_QUINT;
            } else if (btn->clickCount == 4) {
                event = BTN_QUAD;
            } else if (btn->clickCount == 3) {
                event = BTN_TRIPLE;
            } else if (btn->clickCount == 2) {
                event = BTN_DOUBLE;
            } else {
                event = BTN_CLICK;
            }
            btn->clickCount = 0;
        }
    }

    return event;
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void handleButton1(ButtonEvent event) {
    // Audio toggle is handled directly in loop() for instant response
    // This function is kept for potential future use (long press etc.)
}

void handleButton2(ButtonEvent event) {
    switch (currentState) {
        // ========== NORMAL STATE ==========
        case STATE_NORMAL:
            if (event == BTN_CLICK) {
                // 1x klik › zmiana trybu (wed³ug editTarget)
                // Wy³¹cz auto mode jeœli w³¹czony
                if (autoModeEnabled) {
                    autoModeEnabled = false;
                }
                if (editTarget == TARGET_ALL) {
                    globalProgram = (Program)((globalProgram + 1) % PROG_COUNT);
                    if (globalProgram == PROG_CUSTOM) globalProgram = PROG_OFF;
                    configExtRing.program = globalProgram;
                    configIntRing.program = globalProgram;
                    configFans.program = globalProgram;
                    // Sync speed z nowego programu
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                    configFans.speed = fanProgSpeeds[configFans.program];
                } else if (editTarget == TARGET_BOTH) {
                    globalProgram = (Program)((globalProgram + 1) % PROG_COUNT);
                    if (globalProgram == PROG_CUSTOM) globalProgram = PROG_OFF;
                    configExtRing.program = globalProgram;
                    configIntRing.program = globalProgram;
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                } else if (editTarget == TARGET_EXT) {
                    configExtRing.program = (Program)((configExtRing.program + 1) % PROG_COUNT);
                    if (configExtRing.program == PROG_CUSTOM) configExtRing.program = PROG_OFF;
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                } else if (editTarget == TARGET_INT) {
                    configIntRing.program = (Program)((configIntRing.program + 1) % PROG_COUNT);
                    if (configIntRing.program == PROG_CUSTOM) configIntRing.program = PROG_OFF;
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                } else if (editTarget == TARGET_FANS) {
                    configFans.program = (Program)((configFans.program + 1) % PROG_COUNT);
                    if (configFans.program == PROG_CUSTOM) configFans.program = PROG_OFF;
                    configFans.speed = fanProgSpeeds[configFans.program];
                }
            }
            else if (event == BTN_DOUBLE) {
                // 2x klik › tryb prêdkoœci
                currentState = STATE_SPEED;
            }
            else if (event == BTN_TRIPLE) {
                // 3x klik › tryb jasnoœci
                currentState = STATE_BRIGHTNESS;
            }
            else if (event == BTN_QUAD) {
                // 4x klik › tryb koloru
                currentState = STATE_COLOR;
            }
            else if (event == BTN_QUINT) {
                // 5x klik › wybór targetu (ext/int/both)
                currentState = STATE_SELECT_TARGET;
            }
            else if (event == BTN_SEXT) {
                // 6x klik › w³¹cz/wy³¹cz auto mode
                autoModeEnabled = !autoModeEnabled;
                lastAutoChange = millis();
                // Flash fioletowy (ON) lub szary (OFF)
                uint32_t flashColor = autoModeEnabled ?
                    extLeft.Color(200, 0, 255) : extLeft.Color(100, 100, 100);
                for (int i = 0; i < LEDS_EXT; i++) {
                    extLeft.setPixelColor(i, flashColor);
                    extRight.setPixelColor(i, flashColor);
                }
                for (int i = 0; i < LEDS_INT; i++) {
                    intLeft.setPixelColor(i, flashColor);
                    intRight.setPixelColor(i, flashColor);
                }
                extLeft.show(); extRight.show();
                intLeft.show(); intRight.show();
                delay(300);
            }
            else if (event == BTN_LONG) {
                // Hold › selekcja sekcji
                currentState = STATE_SELECT_SECTION;
                selectedSection = SEC_ALL;
            }
            break;

        // ========== SPEED STATE (5 poziomów) ==========
        case STATE_SPEED:
            if (event == BTN_CLICK) {
                // 1x klik › zmiana prêdkoœci dla aktualnego programu
                if (editTarget == TARGET_ALL) {
                    extProgSpeeds[configExtRing.program] = (extProgSpeeds[configExtRing.program] + 1) % 10;
                    intProgSpeeds[configIntRing.program] = (intProgSpeeds[configIntRing.program] + 1) % 10;
                    fanProgSpeeds[configFans.program] = (fanProgSpeeds[configFans.program] + 1) % 10;
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                    configFans.speed = fanProgSpeeds[configFans.program];
                } else if (editTarget == TARGET_BOTH) {
                    extProgSpeeds[configExtRing.program] = (extProgSpeeds[configExtRing.program] + 1) % 10;
                    intProgSpeeds[configIntRing.program] = (intProgSpeeds[configIntRing.program] + 1) % 10;
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                } else if (editTarget == TARGET_EXT) {
                    extProgSpeeds[configExtRing.program] = (extProgSpeeds[configExtRing.program] + 1) % 10;
                    configExtRing.speed = extProgSpeeds[configExtRing.program];
                } else if (editTarget == TARGET_INT) {
                    intProgSpeeds[configIntRing.program] = (intProgSpeeds[configIntRing.program] + 1) % 10;
                    configIntRing.speed = intProgSpeeds[configIntRing.program];
                } else if (editTarget == TARGET_FANS) {
                    fanProgSpeeds[configFans.program] = (fanProgSpeeds[configFans.program] + 1) % 10;
                    configFans.speed = fanProgSpeeds[configFans.program];
                }
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                // Hold › zatwierdŸ i wyjdŸ
                currentState = STATE_NORMAL;
                saveSettings();
            }
            break;

        // ========== BRIGHTNESS STATE (5 poziomów) ==========
        case STATE_BRIGHTNESS:
            if (event == BTN_CLICK) {
                // 1x klik › nastêpny poziom jasnoœci (wed³ug editTarget)
                static const uint8_t brightLevels[] = {50, 100, 150, 200, 255};
                static uint8_t brightIdx = 3;
                brightIdx = (brightIdx + 1) % 5;
                uint8_t newBright = brightLevels[brightIdx];

                if (editTarget == TARGET_ALL) {
                    globalBrightness = newBright;
                    configExtRing.brightness = newBright;
                    configIntRing.brightness = newBright;
                    configFans.brightness = newBright;
                    extLeft.setBrightness(newBright);
                    extRight.setBrightness(newBright);
                    intLeft.setBrightness(newBright);
                    intRight.setBrightness(newBright);
                    fans.setBrightness(newBright);
                } else if (editTarget == TARGET_BOTH) {
                    globalBrightness = newBright;
                    configExtRing.brightness = newBright;
                    configIntRing.brightness = newBright;
                    extLeft.setBrightness(newBright);
                    extRight.setBrightness(newBright);
                    intLeft.setBrightness(newBright);
                    intRight.setBrightness(newBright);
                } else if (editTarget == TARGET_EXT) {
                    configExtRing.brightness = newBright;
                    extLeft.setBrightness(newBright);
                    extRight.setBrightness(newBright);
                } else if (editTarget == TARGET_INT) {
                    configIntRing.brightness = newBright;
                    intLeft.setBrightness(newBright);
                    intRight.setBrightness(newBright);
                } else if (editTarget == TARGET_FANS) {
                    configFans.brightness = newBright;
                    fans.setBrightness(newBright);
                }
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                // Hold › zatwierdŸ i wyjdŸ
                currentState = STATE_NORMAL;
                saveSettings();
            }
            break;

        // ========== COLOR STATE ==========
        case STATE_COLOR:
            if (event == BTN_CLICK) {
                // 1x klik › nastêpny kolor (wed³ug editTarget)
                if (editTarget == TARGET_ALL) {
                    globalVariant = (ColorVariant)((globalVariant + 1) % VAR_COUNT);
                    configExtRing.variant = globalVariant;
                    configIntRing.variant = globalVariant;
                    configFans.variant = globalVariant;
                    showVariantPreview(globalVariant);
                } else if (editTarget == TARGET_BOTH) {
                    globalVariant = (ColorVariant)((globalVariant + 1) % VAR_COUNT);
                    configExtRing.variant = globalVariant;
                    configIntRing.variant = globalVariant;
                    showVariantPreview(globalVariant);
                } else if (editTarget == TARGET_EXT) {
                    configExtRing.variant = (ColorVariant)((configExtRing.variant + 1) % VAR_COUNT);
                    showVariantPreview(configExtRing.variant);
                } else if (editTarget == TARGET_INT) {
                    configIntRing.variant = (ColorVariant)((configIntRing.variant + 1) % VAR_COUNT);
                    showVariantPreview(configIntRing.variant);
                } else if (editTarget == TARGET_FANS) {
                    configFans.variant = (ColorVariant)((configFans.variant + 1) % VAR_COUNT);
                    showVariantPreview(configFans.variant);
                }
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                // Hold › zatwierdŸ i wyjdŸ
                currentState = STATE_NORMAL;
                saveSettings();
            }
            break;

        // ========== SELECT TARGET STATE (5-klik) ==========
        case STATE_SELECT_TARGET:
            if (event == BTN_CLICK) {
                // 1x klik › nastêpny target
                editTarget = (EditTarget)((editTarget + 1) % TARGET_COUNT);
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                // Hold › zatwierdŸ i wyjdŸ
                currentState = STATE_NORMAL;
                saveSettings();
            }
            break;

        // ========== SELECT SECTION STATE ==========
        case STATE_SELECT_SECTION:
            if (event == BTN_CLICK) {
                // 1x klik › nastêpna sekcja
                selectedSection = (Section)((selectedSection + 1) % SEC_COUNT);
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                if (selectedSection == SEC_ALL) {
                    // WyjdŸ z selekcji
                    currentState = STATE_NORMAL;
                } else {
                    // Edytuj wybran¹ sekcjê
                    currentState = STATE_EDIT_SECTION;
                    globalProgram = PROG_CUSTOM;
                }
            }
            break;

        // ========== EDIT SECTION STATE ==========
        case STATE_EDIT_SECTION:
            if (event == BTN_CLICK) {
                // 1x klik › zmieñ program dla sekcji
                SectionConfig* config = getConfigForSection(selectedSection);
                if (config) {
                    config->program = (Program)((config->program + 1) % PROG_CUSTOM);
                }
            }
            else if (event == BTN_LONG || event == BTN_RELEASE) {
                // Hold › zatwierdŸ i wróæ do selekcji
                currentState = STATE_SELECT_SECTION;
                saveSettings();
            }
            break;

        case STATE_GAME:
            // Game mode handled separately
            break;
    }
}

SectionConfig* getConfigForSection(Section sec) {
    switch (sec) {
        case SEC_FANS: return &configFans;
        case SEC_EXT_RING: return &configExtRing;
        case SEC_INT_RING: return &configIntRing;
        default: return NULL;
    }
}

void checkBothButtons() {
    static uint32_t bothPressStart = 0;
    bool btn1Pressed = !digitalRead(PIN_BTN1);
    bool btn2Pressed = !digitalRead(PIN_BTN2);

    if (btn1Pressed && btn2Pressed) {
        if (bothPressStart == 0) {
            bothPressStart = millis();
        }
        if (millis() - bothPressStart >= 2000) {
            // Enter game mode
            initGame();
            currentState = STATE_GAME;
            bothPressStart = 0;
        }
    } else {
        bothPressStart = 0;
    }
}

// ============================================================================
// AUDIO TOGGLE
// ============================================================================

// Apply audio pins according to current audioStateA (without changing the variable)
void applyAudioPins() {
    // Only PA4 (PIN_AUDIO_B1) for single relay control
    digitalWrite(PIN_AUDIO_B1, audioStateA ? LOW : HIGH);
}

void toggleAudio() {
    audioStateA = !audioStateA;
    applyAudioPins();
}

// ============================================================================
// LED UPDATE
// ============================================================================

void updateLEDs() {
    switch (currentState) {
        case STATE_NORMAL:
            // External rings - w³asna animacja
            animOffset = animOffsetExt;
            pulseValue = pulseValueExt;
            applyEffect(&extLeft, LEDS_EXT, configExtRing.program, configExtRing.variant, configExtRing.speed);
            applyEffect(&extRight, LEDS_EXT, configExtRing.program, configExtRing.variant, configExtRing.speed);

            // Internal rings - w³asna animacja
            animOffset = animOffsetInt;
            pulseValue = pulseValueInt;
            applyEffect(&intLeft, LEDS_INT, configIntRing.program, configIntRing.variant, configIntRing.speed);
            applyEffect(&intRight, LEDS_INT, configIntRing.program, configIntRing.variant, configIntRing.speed);

            // Fan LEDs - w³asna animacja
            animOffset = animOffsetFan;
            pulseValue = pulseValueFan;
            applyEffect(&fans, LEDS_FANS, configFans.program, configFans.variant, configFans.speed);
            break;

        case STATE_SELECT_TARGET:
            // Blink target selection (cyan for selected, dim for not selected)
            if (millis() - lastBlinkTime >= 300) {
                lastBlinkTime = millis();
                blinkOn = !blinkOn;
            }
            showTargetSelection(editTarget);
            break;

        case STATE_SELECT_SECTION:
            // Blink selected section green, others off
            if (millis() - lastBlinkTime >= 300) {
                lastBlinkTime = millis();
                blinkOn = !blinkOn;
            }
            showSectionSelection(selectedSection);
            break;

        case STATE_EDIT_SECTION:
            // Show effect on selected section, dim others
            {
                SectionConfig* config = getConfigForSection(selectedSection);
                if (config) {
                    // Apply effect to selected section
                    switch (selectedSection) {
                        case SEC_FANS:
                            // Apply effect to fans, clear rings
                            applyEffect(&fans, LEDS_FANS, config->program, config->variant, globalSpeed);
                            extLeft.clear(); extRight.clear();
                            intLeft.clear(); intRight.clear();
                            break;
                        case SEC_EXT_RING:
                            applyEffect(&extLeft, LEDS_EXT, config->program, config->variant, globalSpeed);
                            applyEffect(&extRight, LEDS_EXT, config->program, config->variant, globalSpeed);
                            intLeft.clear(); intRight.clear();
                            fans.clear();
                            break;
                        case SEC_INT_RING:
                            applyEffect(&intLeft, LEDS_INT, config->program, config->variant, globalSpeed);
                            applyEffect(&intRight, LEDS_INT, config->program, config->variant, globalSpeed);
                            extLeft.clear(); extRight.clear();
                            fans.clear();
                            break;
                        default:
                            break;
                    }
                }
            }
            break;

        case STATE_COLOR:
            // Show color on selected target, dim others
            {
                uint8_t r, g, b;
                ColorVariant showVar;

                // Wybierz wariant do pokazania
                if (editTarget == TARGET_FANS) showVar = configFans.variant;
                else if (editTarget == TARGET_EXT) showVar = configExtRing.variant;
                else if (editTarget == TARGET_INT) showVar = configIntRing.variant;
                else showVar = globalVariant;

                getVariantColor(showVar, &r, &g, &b);
                uint32_t dimColor = extLeft.Color(20, 20, 20);

                // External rings
                if (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_EXT) {
                    for (int i = 0; i < LEDS_EXT; i++) {
                        extLeft.setPixelColor(i, extLeft.Color(r, g, b));
                        extRight.setPixelColor(i, extRight.Color(r, g, b));
                    }
                } else {
                    for (int i = 0; i < LEDS_EXT; i++) {
                        extLeft.setPixelColor(i, dimColor);
                        extRight.setPixelColor(i, dimColor);
                    }
                }

                // Internal rings
                if (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_INT) {
                    for (int i = 0; i < LEDS_INT; i++) {
                        intLeft.setPixelColor(i, intLeft.Color(r, g, b));
                        intRight.setPixelColor(i, intRight.Color(r, g, b));
                    }
                } else {
                    for (int i = 0; i < LEDS_INT; i++) {
                        intLeft.setPixelColor(i, dimColor);
                        intRight.setPixelColor(i, dimColor);
                    }
                }

                // Fans
                if (editTarget == TARGET_ALL || editTarget == TARGET_FANS) {
                    for (int i = 0; i < LEDS_FANS; i++) {
                        fans.setPixelColor(i, fans.Color(r, g, b));
                    }
                } else {
                    fans.clear();
                }
            }
            break;

        case STATE_BRIGHTNESS:
            // Show 5 white segments, brightness matches level
            {
                uint8_t level = 0;
                uint8_t currentBright;

                // Wybierz jasnoœæ do pokazania
                if (editTarget == TARGET_FANS) currentBright = configFans.brightness;
                else if (editTarget == TARGET_EXT) currentBright = configExtRing.brightness;
                else if (editTarget == TARGET_INT) currentBright = configIntRing.brightness;
                else currentBright = globalBrightness;

                // Oblicz poziom (0-4)
                if (currentBright >= 250) level = 4;
                else if (currentBright >= 175) level = 3;
                else if (currentBright >= 125) level = 2;
                else if (currentBright >= 75) level = 1;
                else level = 0;

                // Jasnoœci dla ka¿dego poziomu
                static const uint8_t segBright[] = {50, 100, 150, 200, 255};
                uint32_t dimColor = extLeft.Color(20, 20, 20);

                // External rings - 5 segmentów
                int segSizeExt = LEDS_EXT / 5;
                for (int seg = 0; seg < 5; seg++) {
                    uint8_t b = (seg <= level) ? segBright[seg] : 0;
                    uint32_t color = (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_EXT)
                        ? extLeft.Color(b, b, b) : dimColor;
                    for (int i = seg * segSizeExt; i < (seg + 1) * segSizeExt && i < LEDS_EXT; i++) {
                        extLeft.setPixelColor(i, color);
                        extRight.setPixelColor(i, color);
                    }
                }

                // Internal rings - 5 segmentów
                int segSizeInt = LEDS_INT / 5;
                for (int seg = 0; seg < 5; seg++) {
                    uint8_t b = (seg <= level) ? segBright[seg] : 0;
                    uint32_t color = (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_INT)
                        ? intLeft.Color(b, b, b) : dimColor;
                    for (int i = seg * segSizeInt; i < (seg + 1) * segSizeInt && i < LEDS_INT; i++) {
                        intLeft.setPixelColor(i, color);
                        intRight.setPixelColor(i, color);
                    }
                }

                // Fans - poka¿ gdy edytujemy FANS lub ALL
                if (editTarget == TARGET_ALL || editTarget == TARGET_FANS) {
                    uint8_t b = segBright[level];
                    for (int i = 0; i < LEDS_FANS; i++) {
                        fans.setPixelColor(i, fans.Color(b, b, b));
                    }
                } else {
                    fans.clear();
                }
            }
            break;

        case STATE_SPEED:
            // Show 10-level speed indicator - na wybranym ringu
            {
                uint8_t levelExt = extProgSpeeds[configExtRing.program];
                uint8_t levelInt = intProgSpeeds[configIntRing.program];

                // 10 kolorów: blue -> cyan -> green -> yellow -> orange -> red
                uint32_t colors[] = {
                    extLeft.Color(0, 0, 255),     // 0 - Blue (slowest)
                    extLeft.Color(0, 128, 255),   // 1
                    extLeft.Color(0, 255, 255),   // 2 - Cyan
                    extLeft.Color(0, 255, 128),   // 3
                    extLeft.Color(0, 255, 0),     // 4 - Green
                    extLeft.Color(128, 255, 0),   // 5
                    extLeft.Color(255, 255, 0),   // 6 - Yellow
                    extLeft.Color(255, 180, 0),   // 7 - Orange
                    extLeft.Color(255, 80, 0),    // 8
                    extLeft.Color(255, 0, 0)      // 9 - Red (fastest)
                };
                uint32_t dimColor = extLeft.Color(15, 15, 15);

                // External rings
                int segSizeExt = LEDS_EXT / 10;
                bool extActive = (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_EXT);
                for (int seg = 0; seg < 10; seg++) {
                    uint32_t color;
                    if (extActive) {
                        color = (seg <= levelExt) ? colors[seg] : extLeft.Color(0, 0, 0);
                    } else {
                        color = dimColor;  // Przyciemnione gdy edytujemy INT
                    }
                    for (int i = seg * segSizeExt; i < (seg + 1) * segSizeExt && i < LEDS_EXT; i++) {
                        extLeft.setPixelColor(i, color);
                        extRight.setPixelColor(i, color);
                    }
                }

                // Internal rings - poka¿ gdy edytujemy INT lub BOTH
                int segSizeInt = LEDS_INT / 10;
                bool intActive = (editTarget == TARGET_ALL || editTarget == TARGET_BOTH || editTarget == TARGET_INT);
                for (int seg = 0; seg < 10; seg++) {
                    uint32_t color;
                    if (intActive) {
                        color = (seg <= levelInt) ? colors[seg] : intLeft.Color(0, 0, 0);
                    } else {
                        color = dimColor;  // Przyciemnione gdy edytujemy EXT
                    }
                    for (int i = seg * segSizeInt; i < (seg + 1) * segSizeInt && i < LEDS_INT; i++) {
                        intLeft.setPixelColor(i, color);
                        intRight.setPixelColor(i, color);
                    }
                }

                // Fans - poka¿ gdy edytujemy FANS lub ALL
                bool fansActive = (editTarget == TARGET_ALL || editTarget == TARGET_FANS);
                if (fansActive) {
                    uint8_t levelFan = fanProgSpeeds[configFans.program];
                    int segSizeFan = LEDS_FANS / 10;
                    if (segSizeFan < 1) segSizeFan = 1;
                    for (int seg = 0; seg < 10 && seg * segSizeFan < LEDS_FANS; seg++) {
                        uint32_t color = (seg <= levelFan) ? colors[seg] : fans.Color(0, 0, 0);
                        for (int i = seg * segSizeFan; i < (seg + 1) * segSizeFan && i < LEDS_FANS; i++) {
                            fans.setPixelColor(i, color);
                        }
                    }
                } else {
                    fans.clear();
                }
            }
            break;

        case STATE_GAME:
            // TODO: Game rendering
            break;
    }
}

// ============================================================================
// EFFECT FUNCTIONS
// ============================================================================

void applyEffect(Adafruit_NeoPixel* strip, int numLeds, Program prog, ColorVariant var, uint8_t speed) {
    switch (prog) {
        case PROG_OFF:
            effectOff(strip, numLeds);
            break;
        case PROG_RAINBOW:
            effectRainbow(strip, numLeds, var);
            break;
        case PROG_PULSE:
            effectPulse(strip, numLeds, var);
            break;
        case PROG_SOLID:
            effectSolid(strip, numLeds, var);
            break;
        case PROG_WAVE:
            effectWave(strip, numLeds, var);
            break;
        case PROG_BREATHING:
            effectBreathing(strip, numLeds, var);
            break;
        case PROG_SPARKLE:
            effectSparkle(strip, numLeds, var);
            break;
        case PROG_GRADIENT:
            effectGradient(strip, numLeds, var);
            break;
        case PROG_TRICOLOR:
            effectTricolor(strip, numLeds, var);
            break;
        case PROG_METEOR:
            effectMeteor(strip, numLeds, var);
            break;
        case PROG_THEATER:
            effectTheater(strip, numLeds, var);
            break;
        case PROG_SCANNER:
            effectScanner(strip, numLeds, var);
            break;
        case PROG_TWINKLE:
            effectTwinkle(strip, numLeds, var);
            break;
        case PROG_PLASMA:
            effectPlasma(strip, numLeds, var);
            break;
        case PROG_STROBE:
            effectStrobe(strip, numLeds, var);
            break;
        case PROG_CHASE:
            effectChase(strip, numLeds, var);
            break;
        case PROG_HEARTBEAT:
            effectHeartbeat(strip, numLeds, var);
            break;
        case PROG_BOUNCE:
            effectBounce(strip, numLeds, var);
            break;
        case PROG_FILL:
            effectFill(strip, numLeds, var);
            break;
        default:
            break;
    }
}

void effectOff(Adafruit_NeoPixel* strip, int numLeds) {
    strip->clear();
}

void effectRainbow(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t hueStart = 0, hueEnd = 255;

    // Get hue range based on variant
    if (var < 5) {
        hueStart = RAINBOW_VARIANTS[var][0];
        hueEnd = RAINBOW_VARIANTS[var][1];
    }

    uint8_t hueRange = hueEnd - hueStart;

    for (int i = 0; i < numLeds; i++) {
        uint8_t hue = hueStart + ((i * hueRange / numLeds + animOffset) % hueRange);
        strip->setPixelColor(i, colorWheel(strip, hue));
    }
}

void effectPulse(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Apply pulse brightness - proporcjonalne zachowanie ratio kolorow
    dimColorProportional(&r, &g, &b, pulseValue);

    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(r, g, b));
    }
}

void effectSolid(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(r, g, b));
    }
}

void effectWave(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Sub-pixel position for smooth movement (8-bit fractional)
    uint16_t posFrac = (animOffset * 8) % (numLeds * 256);
    uint8_t subPos = posFrac & 255;  // Fractional part (0-255)
    int pos = posFrac / 256;         // Integer position

    for (int i = 0; i < numLeds; i++) {
        // Calculate distance with wrapping
        int dist1 = abs(i - pos);
        int dist2 = abs(i - (pos + 1) % numLeds);
        if (dist1 > numLeds / 2) dist1 = numLeds - dist1;
        if (dist2 > numLeds / 2) dist2 = numLeds - dist2;

        // Interpolated brightness with sub-pixel anti-aliasing
        uint8_t bright1 = 0, bright2 = 0;

        // Tail length = 6 LEDs
        if (dist1 < 6) {
            bright1 = 255 - (dist1 * 42);
        }
        if (dist2 < 6) {
            bright2 = 255 - (dist2 * 42);
        }

        // Blend between positions based on fractional part
        uint16_t brightness = ((uint16_t)bright1 * (255 - subPos) + (uint16_t)bright2 * subPos) / 255;
        if (brightness > 255) brightness = 255;

        uint8_t pr = r, pg = g, pb = b;
        dimColorProportional(&pr, &pg, &pb, (uint8_t)brightness);

        strip->setPixelColor(i, strip->Color(pr, pg, pb));
    }
}

void effectBreathing(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Smooth breathing: 0->255->0 (triangle with easing)
    uint8_t breathVal;
    uint8_t phase = (animOffset / 2) & 255;

    // 0-127: rising (0->255), 128-255: falling (255->0)
    uint8_t linear;
    if (phase < 128) {
        linear = phase * 2;
    } else {
        linear = (255 - phase) * 2;
    }

    // Apply quadratic easing for smoother effect
    breathVal = ((uint16_t)linear * linear) / 255;

    // Proporcjonalne przyciemnianie - zachowuje ratio kolorow
    dimColorProportional(&r, &g, &b, breathVal);

    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(r, g, b));
    }
}

void effectSparkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Dim base color
    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(r / 4, g / 4, b / 4));
    }

    // Random sparkles
    for (int i = 0; i < 3; i++) {
        int pos = random(numLeds);
        strip->setPixelColor(pos, strip->Color(255, 255, 255));
    }
}

void effectGradient(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    // Two-color gradient that shifts
    uint8_t r1, g1, b1, r2, g2, b2;

    // Pick two colors based on variant
    switch (var) {
        case VAR_WARM:
            r1 = 255; g1 = 0; b1 = 0;
            r2 = 255; g2 = 100; b2 = 0;
            break;
        case VAR_COLD:
            r1 = 0; g1 = 0; b1 = 255;
            r2 = 150; g2 = 0; b2 = 255;
            break;
        case VAR_NATURE:
            r1 = 0; g1 = 255; b1 = 0;
            r2 = 0; g2 = 255; b2 = 255;
            break;
        case VAR_SYNTHWAVE:
            r1 = 255; g1 = 0; b1 = 150;
            r2 = 150; g2 = 0; b2 = 255;
            break;
        default:
            r1 = 255; g1 = 0; b1 = 0;
            r2 = 0; g2 = 0; b2 = 255;
            break;
    }

    for (int i = 0; i < numLeds; i++) {
        int pos = (i + animOffset) % numLeds;
        uint8_t blend = (pos * 255) / numLeds;

        uint8_t r = r1 + ((r2 - r1) * blend / 255);
        uint8_t g = g1 + ((g2 - g1) * blend / 255);
        uint8_t b = b1 + ((b2 - b1) * blend / 255);

        strip->setPixelColor(i, strip->Color(r, g, b));
    }
}

void effectTricolor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    // Trzy kolory goni¹ce siê dooko³a - ka¿dy zajmuje 1/3 ringu
    uint8_t r1, g1, b1, r2, g2, b2, r3, g3, b3;

    // Wybierz 3 kolory w zale¿noœci od wariantu
    switch (var) {
        case VAR_WARM:
            r1 = 255; g1 = 0; b1 = 0;      // Red
            r2 = 255; g2 = 100; b2 = 0;    // Orange
            r3 = 255; g3 = 200; b3 = 0;    // Yellow
            break;
        case VAR_COLD:
            r1 = 0; g1 = 100; b1 = 255;    // Blue
            r2 = 0; g2 = 200; b2 = 255;    // Cyan
            r3 = 150; g3 = 0; b3 = 255;    // Purple
            break;
        case VAR_NATURE:
            r1 = 0; g1 = 255; b1 = 0;      // Green
            r2 = 0; g2 = 255; b2 = 150;    // Teal
            r3 = 100; g3 = 255; b3 = 0;    // Lime
            break;
        default:
            r1 = 255; g1 = 0; b1 = 0;      // Red
            r2 = 0; g2 = 255; b2 = 0;      // Green
            r3 = 0; g3 = 0; b3 = 255;      // Blue
            break;
    }

    int segmentSize = numLeds / 3;
    int pos = animOffset % numLeds;

    for (int i = 0; i < numLeds; i++) {
        int adjusted = (i + pos) % numLeds;
        int segment = adjusted / segmentSize;

        if (segment == 0) {
            strip->setPixelColor(i, strip->Color(r1, g1, b1));
        } else if (segment == 1) {
            strip->setPixelColor(i, strip->Color(r2, g2, b2));
        } else {
            strip->setPixelColor(i, strip->Color(r3, g3, b3));
        }
    }
}

void effectMeteor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    int meteorSize = 4;
    int pos = animOffset % (numLeds + meteorSize * 2);

    // Fade all LEDs - z progiem odciêcia ¿eby nie zostawa³y resztki kolorów
    for (int i = 0; i < numLeds; i++) {
        uint32_t color = strip->getPixelColor(i);
        uint8_t pr = (color >> 16) & 0xFF;
        uint8_t pg = (color >> 8) & 0xFF;
        uint8_t pb = color & 0xFF;
        // Fade i odetnij ma³e wartoœci
        pr = (pr > 8) ? (pr * 180 / 256) : 0;
        pg = (pg > 8) ? (pg * 180 / 256) : 0;
        pb = (pb > 8) ? (pb * 180 / 256) : 0;
        strip->setPixelColor(i, strip->Color(pr, pg, pb));
    }

    // Draw meteor
    for (int i = 0; i < meteorSize; i++) {
        int ledPos = pos - i;
        if (ledPos >= 0 && ledPos < numLeds) {
            uint8_t bright = 255 - (i * 50);
            uint8_t pr = r, pg = g, pb = b;
            dimColorProportional(&pr, &pg, &pb, bright);
            strip->setPixelColor(ledPos, strip->Color(pr, pg, pb));
        }
    }
}

void effectTheater(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    int phase = (animOffset / 2) % 3;

    for (int i = 0; i < numLeds; i++) {
        if ((i + phase) % 3 == 0) {
            strip->setPixelColor(i, strip->Color(r, g, b));
        } else {
            strip->setPixelColor(i, 0);
        }
    }
}

void effectScanner(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Knight Rider - back and forth
    int cycle = (numLeds - 1) * 2;
    int pos = animOffset % cycle;
    if (pos >= numLeds) pos = cycle - pos;

    for (int i = 0; i < numLeds; i++) {
        int dist = abs(i - pos);
        uint8_t bright = 0;
        if (dist == 0) bright = 255;
        else if (dist == 1) bright = 150;
        else if (dist == 2) bright = 60;
        else if (dist == 3) bright = 20;

        uint8_t pr = r, pg = g, pb = b;
        dimColorProportional(&pr, &pg, &pb, bright);
        strip->setPixelColor(i, strip->Color(pr, pg, pb));
    }
}

void effectTwinkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Keep track of twinkle states
    static uint8_t twinkle[50];

    // Fade existing twinkles
    for (int i = 0; i < numLeds; i++) {
        if (twinkle[i] > 0) {
            twinkle[i] = (twinkle[i] > 15) ? twinkle[i] - 15 : 0;
        }

        // Random new twinkle
        if (twinkle[i] == 0 && random(100) < 3) {
            twinkle[i] = 255;
        }

        uint8_t bright = twinkle[i];
        uint8_t pr = r, pg = g, pb = b;
        dimColorProportional(&pr, &pg, &pb, bright);
        strip->setPixelColor(i, strip->Color(pr, pg, pb));
    }
}

void effectPlasma(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    // Uproszczona plasma bez sin()
    for (int i = 0; i < numLeds; i++) {
        uint8_t hue = (i * 6 + animOffset * 2) & 0xFF;
        if (var == VAR_WARM) hue = hue / 4;
        else if (var == VAR_COLD) hue = 128 + (hue / 4);
        else if (var == VAR_NATURE) hue = 64 + (hue / 4);
        strip->setPixelColor(i, colorWheel(strip, hue));
    }
}

void effectStrobe(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Fast on/off based on animation frame
    bool on = (animOffset % 4) < 2;

    uint32_t color = on ? strip->Color(r, g, b) : 0;
    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, color);
    }
}

void effectChase(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // 5 LEDs on, 5 off pattern, moving back and forth
    int groupSize = 5;
    int pattern = groupSize * 2;  // 5 on + 5 off = 10
    int cycle = numLeds * 2;
    int pos = animOffset % cycle;
    if (pos >= numLeds) pos = cycle - pos - 1;  // Reverse direction

    for (int i = 0; i < numLeds; i++) {
        int adjusted = (i + pos) % pattern;
        if (adjusted < groupSize) {
            strip->setPixelColor(i, strip->Color(r, g, b));
        } else {
            strip->setPixelColor(i, 0);
        }
    }
}

void effectHeartbeat(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Heartbeat pattern: two quick pulses then pause
    // Pattern over 60 frames: pulse(10), gap(5), pulse(10), long gap(35)
    int frame = animOffset % 60;
    uint8_t bright = 0;

    if (frame < 10) {
        // First pulse up/down
        bright = (frame < 5) ? frame * 51 : (10 - frame) * 51;
    } else if (frame >= 15 && frame < 25) {
        // Second pulse up/down
        int f = frame - 15;
        bright = (f < 5) ? f * 51 : (10 - f) * 51;
    }
    // else: pause (bright stays 0)

    uint8_t pr = r, pg = g, pb = b;
    dimColorProportional(&pr, &pg, &pb, bright);
    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(pr, pg, pb));
    }
}

void effectBounce(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    // Uproszczony bounce - jedna pi³ka
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    static int16_t pos = 0;
    static int8_t dir = 1;

    strip->clear();
    pos += dir * 3;
    if (pos >= (numLeds - 1) * 10) { pos = (numLeds - 1) * 10; dir = -1; }
    if (pos <= 0) { pos = 0; dir = 1; }

    int led = pos / 10;
    strip->setPixelColor(led, strip->Color(r, g, b));
    if (led > 0) {
        uint8_t pr = r, pg = g, pb = b;
        dimColorProportional(&pr, &pg, &pb, 85); // ~1/3 brightness
        strip->setPixelColor(led-1, strip->Color(pr, pg, pb));
    }
    if (led < numLeds-1) {
        uint8_t pr = r, pg = g, pb = b;
        dimColorProportional(&pr, &pg, &pb, 85);
        strip->setPixelColor(led+1, strip->Color(pr, pg, pb));
    }
}

void effectFill(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    // Efekt wypelniania: LED zapalaja sie po kolei, potem gasna po kolei
    // Pelny cykl = 2 * numLeds krokow
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    int cycle = numLeds * 2;
    int phase = animOffset % cycle;

    if (phase < numLeds) {
        // Faza wypelniania: LED 0..phase sa zapalone
        int fillLevel = phase;
        for (int i = 0; i < numLeds; i++) {
            if (i <= fillLevel) {
                // LED zapalony - z lagodnym rozjasnianiem na krawedzi
                uint8_t bright = 255;
                if (i == fillLevel) {
                    // Ostatni LED - plynne rozjasnianie bazujace na ulamkowej pozycji
                    uint8_t frac = (animOffset * 4) & 0xFF;
                    bright = frac;
                }
                uint8_t pr = r, pg = g, pb = b;
                dimColorProportional(&pr, &pg, &pb, bright);
                strip->setPixelColor(i, strip->Color(pr, pg, pb));
            } else {
                strip->setPixelColor(i, 0);
            }
        }
    } else {
        // Faza wygaszania: LED gasna od 0 do numLeds-1
        int offLevel = phase - numLeds;
        for (int i = 0; i < numLeds; i++) {
            if (i <= offLevel) {
                // LED zgaszony - z lagodnym wygaszaniem na krawedzi
                uint8_t bright = 0;
                if (i == offLevel) {
                    uint8_t frac = 255 - ((animOffset * 4) & 0xFF);
                    bright = frac;
                }
                uint8_t pr = r, pg = g, pb = b;
                dimColorProportional(&pr, &pg, &pb, bright);
                strip->setPixelColor(i, strip->Color(pr, pg, pb));
            } else {
                // LED jeszcze zapalony
                strip->setPixelColor(i, strip->Color(r, g, b));
            }
        }
    }
}

// ============================================================================
// INDICATOR FUNCTIONS
// ============================================================================

void showSpeedIndicator(uint8_t level) {
    showingIndicator = true;
    indicatorStartTime = millis();
    indicatorValue = level;

    // Divide external ring into 5 segments
    int segmentSize = LEDS_EXT / 5;

    // Colors for each speed level (slow=blue to fast=red)
    uint32_t colors[] = {
        extLeft.Color(0, 0, 255),     // Level 0 - Blue (slowest)
        extLeft.Color(0, 255, 255),   // Level 1 - Cyan
        extLeft.Color(0, 255, 0),     // Level 2 - Green
        extLeft.Color(255, 255, 0),   // Level 3 - Yellow
        extLeft.Color(255, 0, 0)      // Level 4 - Red (fastest)
    };

    extLeft.clear();
    extRight.clear();
    intLeft.clear();
    intRight.clear();
    fans.clear();

    // Light up segments up to current level
    for (int seg = 0; seg <= level; seg++) {
        for (int i = seg * segmentSize; i < (seg + 1) * segmentSize && i < LEDS_EXT; i++) {
            extLeft.setPixelColor(i, colors[seg]);
            extRight.setPixelColor(i, colors[seg]);
        }
    }
}

void showBrightnessLevel() {
    showingIndicator = true;
    indicatorStartTime = millis();

    // 5 poziomów jasnoœci: 50, 100, 150, 200, 255
    // Pokazujemy na ma³ych ringach
    uint8_t level = 0;
    if (globalBrightness >= 250) level = 4;
    else if (globalBrightness >= 175) level = 3;
    else if (globalBrightness >= 125) level = 2;
    else if (globalBrightness >= 75) level = 1;
    else level = 0;

    int segmentSize = LEDS_INT / 5;

    intLeft.clear();
    intRight.clear();
    fans.clear();

    // Bia³e segmenty pokazuj¹ce poziom jasnoœci
    for (int seg = 0; seg <= level; seg++) {
        for (int i = seg * segmentSize; i < (seg + 1) * segmentSize && i < LEDS_INT; i++) {
            intLeft.setPixelColor(i, intLeft.Color(255, 255, 255));
            intRight.setPixelColor(i, intRight.Color(255, 255, 255));
        }
    }
}

void showSectionSelection(Section sec) {
    // Clear all
    extLeft.clear();
    extRight.clear();
    intLeft.clear();
    intRight.clear();
    fans.clear();

    uint32_t color = blinkOn ? extLeft.Color(0, 255, 0) : extLeft.Color(0, 50, 0);

    switch (sec) {
        case SEC_ALL:
            // Blink all (including fans)
            for (int i = 0; i < LEDS_EXT; i++) {
                extLeft.setPixelColor(i, color);
                extRight.setPixelColor(i, color);
            }
            for (int i = 0; i < LEDS_INT; i++) {
                intLeft.setPixelColor(i, color);
                intRight.setPixelColor(i, color);
            }
            for (int i = 0; i < LEDS_FANS; i++) { fans.setPixelColor(i, color); }
            fans.show();
            break;

        case SEC_FANS:
            // Blink fans only
            for (int i = 0; i < LEDS_FANS; i++) { fans.setPixelColor(i, color); }
            fans.show();
            break;

        case SEC_EXT_RING:
            for (int i = 0; i < LEDS_EXT; i++) {
                extLeft.setPixelColor(i, color);
                extRight.setPixelColor(i, color);
            }
            break;

        case SEC_INT_RING:
            for (int i = 0; i < LEDS_INT; i++) {
                intLeft.setPixelColor(i, color);
                intRight.setPixelColor(i, color);
            }
            for (int i = 0; i < LEDS_FANS; i++) { fans.setPixelColor(i, color); }
            fans.show();
            break;
    }
}

void showTargetSelection(EditTarget target) {
    // Clear all
    extLeft.clear();
    extRight.clear();
    intLeft.clear();
    intRight.clear();
    fans.clear();

    uint32_t colorOn = extLeft.Color(0, 200, 255);   // Cyan for selected
    uint32_t colorOff = extLeft.Color(0, 30, 50);    // Dim cyan for blink off
    uint32_t dimColor = extLeft.Color(20, 20, 20);   // Dim gray for not selected

    uint32_t extColor, intColor, fanColor;

    switch (target) {
        case TARGET_ALL:
            extColor = blinkOn ? colorOn : colorOff;
            intColor = blinkOn ? colorOn : colorOff;
            fanColor = blinkOn ? colorOn : colorOff;
            break;
        case TARGET_BOTH:
            extColor = blinkOn ? colorOn : colorOff;
            intColor = blinkOn ? colorOn : colorOff;
            fanColor = dimColor;
            break;
        case TARGET_EXT:
            extColor = blinkOn ? colorOn : colorOff;
            intColor = dimColor;
            fanColor = dimColor;
            break;
        case TARGET_INT:
            extColor = dimColor;
            intColor = blinkOn ? colorOn : colorOff;
            fanColor = dimColor;
            break;
        case TARGET_FANS:
            extColor = dimColor;
            intColor = dimColor;
            fanColor = blinkOn ? colorOn : colorOff;
            break;
        default:
            extColor = colorOff;
            intColor = colorOff;
            fanColor = colorOff;
            break;
    }

    // Set external rings
    for (int i = 0; i < LEDS_EXT; i++) {
        extLeft.setPixelColor(i, extColor);
        extRight.setPixelColor(i, extColor);
    }

    // Set internal rings
    for (int i = 0; i < LEDS_INT; i++) {
        intLeft.setPixelColor(i, intColor);
        intRight.setPixelColor(i, intColor);
    }

    // Set fans
    for (int i = 0; i < LEDS_FANS; i++) {
        fans.setPixelColor(i, fanColor);
    }
    fans.show();
}

void showVariantPreview(ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Quick flash of the color
    for (int i = 0; i < LEDS_INT; i++) {
        intLeft.setPixelColor(i, intLeft.Color(r, g, b));
        intRight.setPixelColor(i, intRight.Color(r, g, b));
    }
    intLeft.show();
    intRight.show();
    delay(100);
}

// ============================================================================
// COLOR HELPERS
// ============================================================================

uint32_t colorWheel(Adafruit_NeoPixel* strip, byte pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return strip->Color(255 - pos * 3, 0, pos * 3);
    }
    if (pos < 170) {
        pos -= 85;
        return strip->Color(0, pos * 3, 255 - pos * 3);
    }
    pos -= 170;
    return strip->Color(pos * 3, 255 - pos * 3, 0);
}

void getVariantColor(ColorVariant var, uint8_t* r, uint8_t* g, uint8_t* b) {
    switch (var) {
        case VAR_WHITE:   *r = 255; *g = 255; *b = 255; break;
        case VAR_RED:     *r = 255; *g = 0;   *b = 0;   break;
        case VAR_GREEN:   *r = 0;   *g = 255; *b = 0;   break;
        case VAR_BLUE:    *r = 0;   *g = 0;   *b = 255; break;
        case VAR_PINK:    *r = 255; *g = 50;  *b = 150; break;
        case VAR_PURPLE:  *r = 150; *g = 0;   *b = 255; break;
        case VAR_CYAN:    *r = 0;   *g = 255; *b = 255; break;
        case VAR_ORANGE:  *r = 255; *g = 100; *b = 0;   break;
        case VAR_WARM:    *r = 255; *g = 80;  *b = 0;   break;
        case VAR_COLD:    *r = 100; *g = 100; *b = 255; break;
        case VAR_NATURE:  *r = 50;  *g = 255; *b = 100; break;
        case VAR_SYNTHWAVE: *r = 255; *g = 0; *b = 200; break;
        default:          *r = 255; *g = 255; *b = 255; break;
    }
}

// Proporcjonalne przyciemnianie kolorow - zachowuje ratio RGB
// Uzywa 16-bit math z zaokraglaniem, zeby mniejsze komponenty nie zanikaly szybciej
void dimColorProportional(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t brightness) {
    if (brightness == 0) {
        *r = 0; *g = 0; *b = 0;
        return;
    }
    if (brightness == 255) {
        return; // Bez zmian
    }

    // Uzywamy 16-bit z zaokraglaniem (+128) dla dokladniejszego skalowania
    uint16_t newR = ((uint16_t)(*r) * brightness + 128) / 255;
    uint16_t newG = ((uint16_t)(*g) * brightness + 128) / 255;
    uint16_t newB = ((uint16_t)(*b) * brightness + 128) / 255;

    // Jesli oryginalny komponent > 0 i brightness > prog, zachowaj minimum 1
    // To zapobiega zanikaniu jednego koloru przed drugim
    if (*r > 0 && brightness > 8 && newR == 0) newR = 1;
    if (*g > 0 && brightness > 8 && newG == 0) newG = 1;
    if (*b > 0 && brightness > 8 && newB == 0) newB = 1;

    *r = (uint8_t)newR;
    *g = (uint8_t)newG;
    *b = (uint8_t)newB;
}

// ============================================================================
// EEPROM
// ============================================================================

#define EEPROM_MAGIC        0xAF  // Zmienione - proporcjonalne dimming kolorow
#define EEPROM_ADDR_MAGIC   0
#define EEPROM_ADDR_PROG    1
#define EEPROM_ADDR_VAR     2
#define EEPROM_ADDR_SPEED   3
#define EEPROM_ADDR_BRIGHT  4
#define EEPROM_ADDR_AUDIO   5
#define EEPROM_ADDR_TARGET  6
// Section configs start at 10 (4 bytes each: program, variant, speed, brightness)
#define EEPROM_ADDR_FANS    10
#define EEPROM_ADDR_EXT     14
#define EEPROM_ADDR_INT     18
// Per-program speeds (PROG_COUNT bytes each)
#define EEPROM_ADDR_EXT_SPEEDS  30
#define EEPROM_ADDR_INT_SPEEDS  50
#define EEPROM_ADDR_FAN_SPEEDS  70

void loadSettings() {
    // Najpierw inicjalizuj prêdkoœci per program domyœlnymi wartoœciami
    for (int i = 0; i < PROG_COUNT; i++) {
        extProgSpeeds[i] = 5;  // Domyœlna prêdkoœæ (œrodek skali)
        intProgSpeeds[i] = 5;
        fanProgSpeeds[i] = 5;
    }

    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
        // First run - save defaults
        saveSettings();
        return;
    }

    globalProgram = (Program)EEPROM.read(EEPROM_ADDR_PROG);
    globalVariant = (ColorVariant)EEPROM.read(EEPROM_ADDR_VAR);
    globalSpeed = EEPROM.read(EEPROM_ADDR_SPEED);
    globalBrightness = EEPROM.read(EEPROM_ADDR_BRIGHT);
    audioStateA = EEPROM.read(EEPROM_ADDR_AUDIO);
    editTarget = (EditTarget)EEPROM.read(EEPROM_ADDR_TARGET);
    if (editTarget >= TARGET_COUNT) editTarget = TARGET_ALL;

    // Load section configs (program, variant, speed, brightness)
    configFans.program = (Program)EEPROM.read(EEPROM_ADDR_FANS);
    configFans.variant = (ColorVariant)EEPROM.read(EEPROM_ADDR_FANS + 1);
    configFans.speed = EEPROM.read(EEPROM_ADDR_FANS + 2);
    configFans.brightness = EEPROM.read(EEPROM_ADDR_FANS + 3);

    configExtRing.program = (Program)EEPROM.read(EEPROM_ADDR_EXT);
    configExtRing.variant = (ColorVariant)EEPROM.read(EEPROM_ADDR_EXT + 1);
    configExtRing.speed = EEPROM.read(EEPROM_ADDR_EXT + 2);
    configExtRing.brightness = EEPROM.read(EEPROM_ADDR_EXT + 3);

    configIntRing.program = (Program)EEPROM.read(EEPROM_ADDR_INT);
    configIntRing.variant = (ColorVariant)EEPROM.read(EEPROM_ADDR_INT + 1);
    configIntRing.speed = EEPROM.read(EEPROM_ADDR_INT + 2);
    configIntRing.brightness = EEPROM.read(EEPROM_ADDR_INT + 3);

    // Load per-program speeds
    for (int i = 0; i < PROG_COUNT; i++) {
        uint8_t extSpd = EEPROM.read(EEPROM_ADDR_EXT_SPEEDS + i);
        uint8_t intSpd = EEPROM.read(EEPROM_ADDR_INT_SPEEDS + i);
        uint8_t fanSpd = EEPROM.read(EEPROM_ADDR_FAN_SPEEDS + i);
        extProgSpeeds[i] = (extSpd <= 9) ? extSpd : 5;
        intProgSpeeds[i] = (intSpd <= 9) ? intSpd : 5;
        fanProgSpeeds[i] = (fanSpd <= 9) ? fanSpd : 5;
    }

    // Sync configXXX.speed z aktualnego programu - kazda sekcja uzywa swojej predkosci
    configExtRing.speed = extProgSpeeds[configExtRing.program];
    configIntRing.speed = intProgSpeeds[configIntRing.program];
    configFans.speed = fanProgSpeeds[configFans.program];

    // Validate brightness
    if (configExtRing.brightness == 0) configExtRing.brightness = 200;
    if (configIntRing.brightness == 0) configIntRing.brightness = 200;
    if (configFans.brightness == 0) configFans.brightness = 200;

    // Validate fans (force rainbow if OFF or invalid)
    if (configFans.program == PROG_OFF || configFans.program >= PROG_COUNT) {
        configFans.program = PROG_RAINBOW;
    }

    // Validate
    if (globalProgram >= PROG_COUNT) globalProgram = PROG_RAINBOW;
    if (globalSpeed > 9) globalSpeed = 5;
    if (globalBrightness == 0) globalBrightness = 200;

    // Apply per-section brightness
    extLeft.setBrightness(configExtRing.brightness);
    extRight.setBrightness(configExtRing.brightness);
    intLeft.setBrightness(configIntRing.brightness);
    intRight.setBrightness(configIntRing.brightness);
    fans.setBrightness(configFans.brightness);

    // Apply audio state from EEPROM (set pins according to saved state)
    applyAudioPins();
}

void saveSettings() {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_PROG, (uint8_t)globalProgram);
    EEPROM.write(EEPROM_ADDR_VAR, (uint8_t)globalVariant);
    EEPROM.write(EEPROM_ADDR_SPEED, globalSpeed);
    EEPROM.write(EEPROM_ADDR_BRIGHT, globalBrightness);
    EEPROM.write(EEPROM_ADDR_AUDIO, audioStateA);
    EEPROM.write(EEPROM_ADDR_TARGET, (uint8_t)editTarget);

    // Save section configs (program, variant, speed, brightness)
    EEPROM.write(EEPROM_ADDR_FANS, (uint8_t)configFans.program);
    EEPROM.write(EEPROM_ADDR_FANS + 1, (uint8_t)configFans.variant);
    EEPROM.write(EEPROM_ADDR_FANS + 2, configFans.speed);
    EEPROM.write(EEPROM_ADDR_FANS + 3, configFans.brightness);

    EEPROM.write(EEPROM_ADDR_EXT, (uint8_t)configExtRing.program);
    EEPROM.write(EEPROM_ADDR_EXT + 1, (uint8_t)configExtRing.variant);
    EEPROM.write(EEPROM_ADDR_EXT + 2, configExtRing.speed);
    EEPROM.write(EEPROM_ADDR_EXT + 3, configExtRing.brightness);

    EEPROM.write(EEPROM_ADDR_INT, (uint8_t)configIntRing.program);
    EEPROM.write(EEPROM_ADDR_INT + 1, (uint8_t)configIntRing.variant);
    EEPROM.write(EEPROM_ADDR_INT + 2, configIntRing.speed);
    EEPROM.write(EEPROM_ADDR_INT + 3, configIntRing.brightness);

    // Save per-program speeds
    for (int i = 0; i < PROG_COUNT; i++) {
        EEPROM.write(EEPROM_ADDR_EXT_SPEEDS + i, extProgSpeeds[i]);
        EEPROM.write(EEPROM_ADDR_INT_SPEEDS + i, intProgSpeeds[i]);
        EEPROM.write(EEPROM_ADDR_FAN_SPEEDS + i, fanProgSpeeds[i]);
    }
}

// ============================================================================
// GAME FUNCTIONS - Wyœcigi LED
// ============================================================================

void initGame() {
    gameState = GAME_COUNTDOWN;
    gameStateTime = millis();
    countdownStep = 0;

    car1Pos = 2 * 256;      // Start na pozycji 2 (za met¹)
    car2Pos = 2 * 256;
    car1Speed = 0;
    car2Speed = 0;
    car1Laps = 0;
    car2Laps = 0;
    car1CrossedStart = false;
    car2CrossedStart = false;
    winner = 0;
    gameExitHoldStart = 0;

    // Ustaw jasnoœæ na max dla gry
    extLeft.setBrightness(255);
    extRight.setBrightness(255);
    intLeft.setBrightness(255);
    intRight.setBrightness(255);
}

void handleGameInput(bool btn1Click, bool btn2Click) {
    if (gameState != GAME_RACING) return;

    // Klikniêcie = ma³e przyspieszenie, samochód jedzie i powoli zwalnia
    if (btn1Click) {
        car1Speed += CLICK_SPEED_BOOST;
        if (car1Speed > CAR_MAX_SPEED) car1Speed = CAR_MAX_SPEED;
    }
    if (btn2Click) {
        car2Speed += CLICK_SPEED_BOOST;
        if (car2Speed > CAR_MAX_SPEED) car2Speed = CAR_MAX_SPEED;
    }
}

void updateGame() {
    uint32_t now = millis();
    uint32_t elapsed = now - gameStateTime;

    switch (gameState) {
        case GAME_COUNTDOWN:
            // Countdown: ¿ó³ty-stop-¿ó³ty-stop-¿ó³ty-stop-ZIELONY-start
            // Ka¿dy krok 500ms
            {
                uint8_t newStep = elapsed / 500;
                if (newStep > 7) newStep = 7;

                if (newStep != countdownStep) {
                    countdownStep = newStep;
                }

                // Po 7 krokach start wyœcigu (zielony trwa 500ms)
                if (countdownStep >= 7) {
                    gameState = GAME_RACING;
                    gameStateTime = now;
                }
            }
            break;

        case GAME_RACING:
            {
                // Aktualizuj pozycje samochodów
                car1Pos += car1Speed;
                car2Pos += car2Speed;

                // Tarcie (spowalnianie)
                if (car1Speed > 0) car1Speed -= CAR_FRICTION;
                if (car1Speed < 0) car1Speed = 0;
                if (car2Speed > 0) car2Speed -= CAR_FRICTION;
                if (car2Speed < 0) car2Speed = 0;

                // Zawijanie pozycji (okr¹¿enia)
                int32_t trackLength = LEDS_EXT * 256;

                // SprawdŸ przekroczenie mety dla gracza 1
                if (car1Pos >= trackLength) {
                    car1Pos -= trackLength;
                    if (car1CrossedStart) {
                        car1Laps++;
                        if (car1Laps >= RACE_LAPS) {
                            winner = 1;
                            gameState = GAME_WINNER;
                            gameStateTime = now;
                        }
                    }
                    car1CrossedStart = true;
                }

                // SprawdŸ przekroczenie mety dla gracza 2
                if (car2Pos >= trackLength) {
                    car2Pos -= trackLength;
                    if (car2CrossedStart) {
                        car2Laps++;
                        if (car2Laps >= RACE_LAPS) {
                            winner = 2;
                            gameState = GAME_WINNER;
                            gameStateTime = now;
                        }
                    }
                    car2CrossedStart = true;
                }
            }
            break;

        case GAME_WINNER:
            // Poka¿ zwyciêzcê przez 3 sekundy, potem restart
            if (elapsed >= 3000) {
                initGame();  // Restart gry
            }
            break;

        default:
            break;
    }
}

void renderGame() {
    // Wyczyœæ wszystkie ringi
    extLeft.clear();
    extRight.clear();
    intLeft.clear();
    intRight.clear();
    fans.clear();

    uint32_t colorYellow = extLeft.Color(255, 200, 0);
    uint32_t colorGreen = extLeft.Color(0, 255, 0);
    uint32_t colorRed = extLeft.Color(255, 0, 0);
    uint32_t colorBlue = extLeft.Color(0, 0, 255);
    uint32_t colorOff = extLeft.Color(0, 0, 0);

    // === MA£Y RING - Countdown / Status ===
    switch (gameState) {
        case GAME_COUNTDOWN:
            // Countdown: ¿ó³ty-stop-¿ó³ty-stop-¿ó³ty-stop-ZIELONY
            // Krok 0: ¿ó³ty, 1: stop, 2: ¿ó³ty, 3: stop, 4: ¿ó³ty, 5: stop, 6: ZIELONY
            {
                uint32_t countdownColor = colorOff;
                if (countdownStep == 0 || countdownStep == 2 || countdownStep == 4) {
                    countdownColor = colorYellow;
                } else if (countdownStep == 6) {
                    countdownColor = colorGreen;
                }

                for (int i = 0; i < LEDS_INT; i++) {
                    intLeft.setPixelColor(i, countdownColor);
                    intRight.setPixelColor(i, countdownColor);
                }
            }
            break;

        case GAME_RACING:
            // Poka¿ okr¹¿enia: lewy ring = gracz 1, prawy = gracz 2
            {
                // Gracz 1 (czerwony) - laps na lewym ringu
                int segSize = LEDS_INT / RACE_LAPS;
                for (int lap = 0; lap < car1Laps && lap < RACE_LAPS; lap++) {
                    for (int i = lap * segSize; i < (lap + 1) * segSize; i++) {
                        intLeft.setPixelColor(i, colorRed);
                    }
                }

                // Gracz 2 (zielony) - laps na prawym ringu
                for (int lap = 0; lap < car2Laps && lap < RACE_LAPS; lap++) {
                    for (int i = lap * segSize; i < (lap + 1) * segSize; i++) {
                        intRight.setPixelColor(i, colorGreen);
                    }
                }
            }
            break;

        case GAME_WINNER:
            // Flash kolorem zwyciêzcy
            {
                uint32_t winColor = (winner == 1) ? colorRed : colorGreen;
                bool flash = ((millis() / 200) % 2) == 0;
                if (flash) {
                    for (int i = 0; i < LEDS_INT; i++) {
                        intLeft.setPixelColor(i, winColor);
                        intRight.setPixelColor(i, winColor);
                    }
                }
            }
            break;

        default:
            break;
    }

    // === DU¯Y RING - Tor wyœcigu (oba samochody na obu ringach) ===

    // Meta - 2 niebieskie LEDy na pozycji 0-1
    extLeft.setPixelColor(0, colorBlue);
    extLeft.setPixelColor(1, colorBlue);
    extRight.setPixelColor(0, colorBlue);
    extRight.setPixelColor(1, colorBlue);

    if (gameState == GAME_RACING || gameState == GAME_COUNTDOWN) {
        int car1Led = car1Pos / 256;
        int car2Led = car2Pos / 256;

        // Rysuj OBA samochody na OBU ringach
        // Samochód 1 (czerwony)
        for (int i = 0; i < CAR_SIZE; i++) {
            int pos = (car1Led - i + LEDS_EXT) % LEDS_EXT;
            if (pos > 1) {
                uint8_t bright = 255 - (i * 70);
                extLeft.setPixelColor(pos, extLeft.Color(bright, 0, 0));
                extRight.setPixelColor(pos, extRight.Color(bright, 0, 0));
            }
        }

        // Samochód 2 (zielony)
        for (int i = 0; i < CAR_SIZE; i++) {
            int pos = (car2Led - i + LEDS_EXT) % LEDS_EXT;
            if (pos > 1) {
                uint8_t bright = 255 - (i * 70);
                // Dodaj do istniej¹cego koloru (¿eby oba by³y widoczne gdy siê mijaj¹)
                uint32_t existing = extLeft.getPixelColor(pos);
                uint8_t er = (existing >> 16) & 0xFF;
                extLeft.setPixelColor(pos, extLeft.Color(er, bright, 0));

                existing = extRight.getPixelColor(pos);
                er = (existing >> 16) & 0xFF;
                extRight.setPixelColor(pos, extRight.Color(er, bright, 0));
            }
        }
    }

    if (gameState == GAME_WINNER) {
        uint32_t winColor = (winner == 1) ? colorRed : colorGreen;
        bool flash = ((millis() / 200) % 2) == 0;
        if (flash) {
            for (int i = 2; i < LEDS_EXT; i++) {
                extLeft.setPixelColor(i, winColor);
                extRight.setPixelColor(i, winColor);
            }
        }
    }
}

// ============================================================================
// USB SERIAL COMMANDS
// ============================================================================

int getParam(const char* cmd, int paramIndex) {
    int colonCount = 0;
    int startPos = 0;
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] == ':') {
            colonCount++;
            if (colonCount == paramIndex) {
                startPos = i + 1;
            } else if (colonCount == paramIndex + 1) {
                return atoi(cmd + startPos);
            }
        }
    }
    if (colonCount >= paramIndex && startPos > 0) {
        return atoi(cmd + startPos);
    }
    return 0;
}

bool cmdIs(const char* cmd, const char* prefix) {
    int len = strlen(prefix);
    return strncmp(cmd, prefix, len) == 0 && (cmd[len] == ':' || cmd[len] == '\0');
}

void sendResponse(const char* msg) {
    SerialUSB.println(msg);
}

void processUsbCommand(const char* cmd) {
    if (cmdIs(cmd, "PING")) {
        sendResponse("PONG");
    }
    else if (cmdIs(cmd, "AUDIO")) {
        // AUDIO:0 or AUDIO:1 - set audio state
        int state = getParam(cmd, 1);
        audioStateA = (state == 1);
        applyAudioPins();
        audioNeedsSave = true;
        audioSaveTime = millis() + 2000;
        sendResponse(audioStateA ? "AUDIO:A" : "AUDIO:B");
    }
    else if (cmdIs(cmd, "TOGGLE")) {
        // Toggle audio
        toggleAudio();
        audioNeedsSave = true;
        audioSaveTime = millis() + 2000;
        sendResponse(audioStateA ? "AUDIO:A" : "AUDIO:B");
    }
    else if (cmdIs(cmd, "EFFECT")) {
        // EFFECT:id - set effect for both rings (0-17)
        int effect = constrain(getParam(cmd, 1), 0, PROG_COUNT - 1);
        globalProgram = (Program)effect;
        configExtRing.program = globalProgram;
        configIntRing.program = globalProgram;
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "BRIGHT")) {
        // BRIGHT:level (0-255)
        int bright = constrain(getParam(cmd, 1), 0, 255);
        globalBrightness = bright;
        configExtRing.brightness = bright;
        configIntRing.brightness = bright;
        extLeft.setBrightness(bright);
        extRight.setBrightness(bright);
        intLeft.setBrightness(bright);
        intRight.setBrightness(bright);
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "COLOR")) {
        // COLOR:variant (0-12)
        int var = constrain(getParam(cmd, 1), 0, VAR_COUNT - 1);
        globalVariant = (ColorVariant)var;
        configExtRing.variant = globalVariant;
        configIntRing.variant = globalVariant;
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "SPEED")) {
        // SPEED:level (0-9)
        int spd = constrain(getParam(cmd, 1), 0, 9);
        globalSpeed = spd;
        configExtRing.speed = spd;
        configIntRing.speed = spd;
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "GET")) {
        // Return all section settings + audio state
        // Format: SETTINGS:ext_p:ext_v:ext_s:ext_b:int_p:int_v:int_s:int_b:fan_p:fan_v:fan_s:fan_b:fanMode:fanSpeed:audio
        char buf[128];
        snprintf(buf, sizeof(buf), "SETTINGS:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
            (int)configExtRing.program, (int)configExtRing.variant,
            (int)extProgSpeeds[configExtRing.program], (int)configExtRing.brightness,
            (int)configIntRing.program, (int)configIntRing.variant,
            (int)intProgSpeeds[configIntRing.program], (int)configIntRing.brightness,
            (int)configFans.program, (int)configFans.variant,
            (int)configFans.speed, (int)configFans.brightness,
            (int)fanMode, (int)fanSpeed, audioStateA ? 1 : 0);
        sendResponse(buf);
    }
    else if (cmdIs(cmd, "FAN")) {
        // FAN:mode or FAN:mode:speed
        int mode = constrain(getParam(cmd, 1), 0, 5);
        fanMode = mode;
        if (fanMode == FAN_MANUAL) {
            int speed = getParam(cmd, 2);
            fanManualSpeed = constrain(speed, 0, 255);
        }
        updateFan();
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "FANLED")) {
        // FANLED:effect:variant:speed:brightness
        int effect = constrain(getParam(cmd, 1), 0, PROG_COUNT - 1);
        int variant = constrain(getParam(cmd, 2), 0, VAR_COUNT - 1);
        int speed = constrain(getParam(cmd, 3), 0, 9);
        int bright = constrain(getParam(cmd, 4), 0, 255);
        configFans.program = (Program)effect;
        configFans.variant = (ColorVariant)variant;
        configFans.speed = speed;
        fanProgSpeeds[effect] = speed;  // Zapisz prêdkoœæ dla tego efektu
        configFans.brightness = bright;
        fans.setBrightness(bright);
        // Apply immediately - u¿yj w³asnego offsetu fana
        animOffset = animOffsetFan;
        pulseValue = pulseValueFan;
        applyEffect(&fans, LEDS_FANS, configFans.program, configFans.variant, configFans.speed);
        fans.show();
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "EXTLED")) {
        // EXTLED:effect:variant:speed:brightness
        int effect = constrain(getParam(cmd, 1), 0, PROG_COUNT - 1);
        int variant = constrain(getParam(cmd, 2), 0, VAR_COUNT - 1);
        int speed = constrain(getParam(cmd, 3), 0, 9);
        int bright = constrain(getParam(cmd, 4), 0, 255);
        configExtRing.program = (Program)effect;
        configExtRing.variant = (ColorVariant)variant;
        configExtRing.speed = speed;
        extProgSpeeds[effect] = speed;
        configExtRing.brightness = bright;
        extLeft.setBrightness(bright);
        extRight.setBrightness(bright);
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "INTLED")) {
        // INTLED:effect:variant:speed:brightness
        int effect = constrain(getParam(cmd, 1), 0, PROG_COUNT - 1);
        int variant = constrain(getParam(cmd, 2), 0, VAR_COUNT - 1);
        int speed = constrain(getParam(cmd, 3), 0, 9);
        int bright = constrain(getParam(cmd, 4), 0, 255);
        configIntRing.program = (Program)effect;
        configIntRing.variant = (ColorVariant)variant;
        configIntRing.speed = speed;
        intProgSpeeds[effect] = speed;
        configIntRing.brightness = bright;
        intLeft.setBrightness(bright);
        intRight.setBrightness(bright);
        saveSettings();
        sendResponse("OK");
    }
    else if (cmdIs(cmd, "RESET")) {
        NVIC_SystemReset();
    }
}

// ============================================================================
// FAN CONTROL
// ============================================================================

void setFanPWM(uint8_t speed) {
    uint8_t percent = map(speed, 0, 255, 0, 100);
    fanTimer->setCaptureCompare(4, percent, PERCENT_COMPARE_FORMAT);
}

void updateFan() {
    switch (fanMode) {
        case FAN_OFF:
            fanTargetSpeed = 0;
            break;
        case FAN_SILENT:
            fanTargetSpeed = 77;  // ~30%
            break;
        case FAN_NORMAL:
            fanTargetSpeed = 153; // ~60%
            break;
        case FAN_MAX:
            fanTargetSpeed = 255; // 100%
            break;
        case FAN_MANUAL:
            fanTargetSpeed = fanManualSpeed;
            break;
        default:
            fanTargetSpeed = 153;
            break;
    }

    // Smooth transition
    if (fanSpeed < fanTargetSpeed) {
        fanSpeed = min(fanSpeed + 5, fanTargetSpeed);
    } else if (fanSpeed > fanTargetSpeed) {
        fanSpeed = max(fanSpeed - 5, fanTargetSpeed);
    }

    setFanPWM(fanSpeed);
}
