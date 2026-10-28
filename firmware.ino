/*
 * Test sketch for the discrete half-bridge gate driver
 * Target : ESP32 (Arduino-ESP32 core)
 *
 * Libraries: none external. Only the Arduino core and Espressif built-in
 * headers (driver/gpio.h) are used.
 *
 * Wiring: HIN_PIN -> Rb (4.7k) -> Q1 base of the high-side channel,
 *         LIN_PIN -> Rb (4.7k) -> Q1 base of the low-side channel.
 * Both channels are non-inverting: pin HIGH = power MOSFET ON.
 * Switching is timed in software, which is fine for bring-up at 20 kHz or below.
 * For production use, move to the hardware MCPWM peripheral with dead-time.
 *
 * All parameters can be overridden at run time over Serial (115200 baud by
 * default, line ending: Newline or CR). Type "help" for the command list.
 *
 * Bring-up order: mode=blink (LED load) -> mode=high (load to GND, no power
 * load first) -> mode=half (small load).
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"

// ---------------- Test configuration (defaults) ----------------
enum TestMode { MODE_LOW_SIDE_BLINK = 0, MODE_HIGH_SIDE_PWM = 1, MODE_HALF_BRIDGE_PWM = 2 };

struct Config {
  int      hinPin;        // High-side input (avoid strapping pins)
  int      linPin;        // Low-side input
  uint8_t  mode;          // TestMode
  uint32_t freqHz;        // Switching frequency for PWM modes
  uint8_t  duty;          // High-side duty (0-100)
  uint32_t deadUs;        // Gap between high and low conduction
  uint32_t maxHighOnUs;   // Bootstrap limit (about 200 us at 1 uF, 2 ms at 10 uF)
  uint32_t bootMs;        // Low-side on time at start-up / mode change
  uint32_t blinkMs;       // Low-side blink period
  uint32_t baud;
};

const Config DEFAULTS = {
  25,                     // hinPin
  26,                     // linPin
  MODE_LOW_SIDE_BLINK,    // mode
  20000,                  // freqHz
  25,                     // duty
  5,                      // deadUs (BJT storage time; reduce while measuring)
  200,                    // maxHighOnUs
  10,                     // bootMs
  1000,                   // blinkMs
  115200                  // baud
};

Config cfg = DEFAULTS;
bool   running = true;

// Blink state (non-blocking so Serial stays responsive)
bool     blinkOn   = false;
uint32_t blinkLast = 0;

// ---------------- Output control ----------------
void driveOutputs(bool highOn, bool lowOn) {
  if (highOn && lowOn) {          // Never allow both on
    highOn = false;
    lowOn  = false;
  }
  digitalWrite(cfg.hinPin, highOn ? HIGH : LOW);
  digitalWrite(cfg.linPin, lowOn  ? HIGH : LOW);
}

void allOff() {
  driveOutputs(false, false);
}

void initPins() {
  pinMode(cfg.hinPin, OUTPUT);
  pinMode(cfg.linPin, OUTPUT);
  allOff();
}

// Release the pins to high-Z (the 10 k base pull-downs keep both Q1 off, so both gates stay off).
void releasePins() {
  allOff();
  pinMode(cfg.hinPin, INPUT);
  pinMode(cfg.linPin, INPUT);
}

void chargeBootstrap() {
  driveOutputs(false, true);
  delay(cfg.bootMs);
  allOff();
}

void resetBlink() {
  blinkOn   = false;
  blinkLast = millis();
}

// ---------------- Test modes ----------------
void runLowSideBlink() {
  uint32_t now = millis();
  if (now - blinkLast >= cfg.blinkMs / 2) {
    blinkLast = now;
    blinkOn   = !blinkOn;
    driveOutputs(false, blinkOn);
  }
  delay(1);
}

// One PWM period. If complementary is true, the low side conducts in the off time.
void runPwmCycle(bool complementary) {
  uint32_t periodUs = 1000000UL / cfg.freqHz;
  uint32_t onUs     = periodUs * cfg.duty / 100;
  if (onUs > cfg.maxHighOnUs) onUs = cfg.maxHighOnUs;

  uint32_t usedUs = onUs + 2 * cfg.deadUs;
  uint32_t offUs  = (periodUs > usedUs) ? (periodUs - usedUs) : 0;

  allOff();
  delayMicroseconds(cfg.deadUs);
  if (onUs > 0) {
    driveOutputs(true, false);
    delayMicroseconds(onUs);
    allOff();
  }
  delayMicroseconds(cfg.deadUs);
  driveOutputs(false, complementary);
  delayMicroseconds(offUs);
}

void runTest() {
  switch (cfg.mode) {
    case MODE_LOW_SIDE_BLINK:   runLowSideBlink();     break;
    case MODE_HIGH_SIDE_PWM:    runPwmCycle(false);    break;
    case MODE_HALF_BRIDGE_PWM:  runPwmCycle(true);     break;
  }
}

// ---------------- Serial reporting ----------------
const char* modeName(uint8_t m) {
  switch (m) {
    case MODE_LOW_SIDE_BLINK:  return "blink";
    case MODE_HIGH_SIDE_PWM:   return "high";
    case MODE_HALF_BRIDGE_PWM: return "half";
  }
  return "?";
}

void printConfig() {
  uint32_t periodUs = 1000000UL / cfg.freqHz;
  uint32_t onUs     = periodUs * cfg.duty / 100;
  Serial.println("--- Half-bridge gate driver test ---");
  Serial.printf("state : %s\n", running ? "running" : "stopped");
  Serial.printf("mode  : %s (%u)\n", modeName(cfg.mode), (unsigned)cfg.mode);
  Serial.printf("hin   : GPIO%d   lin: GPIO%d\n", cfg.hinPin, cfg.linPin);
  Serial.printf("freq  : %lu Hz (period %lu us)\n",
                (unsigned long)cfg.freqHz, (unsigned long)periodUs);
  Serial.printf("duty  : %u %%\n", (unsigned)cfg.duty);
  Serial.printf("dead  : %lu us\n", (unsigned long)cfg.deadUs);
  Serial.printf("maxon : %lu us%s\n", (unsigned long)cfg.maxHighOnUs,
                onUs > cfg.maxHighOnUs ? "  (clamping the high-side pulse)" : "");
  Serial.printf("boot  : %lu ms\n", (unsigned long)cfg.bootMs);
  Serial.printf("blink : %lu ms\n", (unsigned long)cfg.blinkMs);
  Serial.printf("baud  : %lu\n", (unsigned long)cfg.baud);
}

void printHelp() {
  Serial.println("Commands (case-insensitive, 'key=value' or 'key value'):");
  Serial.println("  mode=blink|high|half   (or 0|1|2)  test mode");
  Serial.println("  freq=<Hz>      100..50000   PWM frequency");
  Serial.println("  duty=<%>       0..100       high-side duty");
  Serial.println("  dead=<us>      0..1000      dead time");
  Serial.println("  maxon=<us>     1..100000    max continuous high-side on time");
  Serial.println("  boot=<ms>      0..1000      bootstrap charge time");
  Serial.println("  blink=<ms>     2..60000     blink period");
  Serial.println("  hin=<gpio>     high-side input pin");
  Serial.println("  lin=<gpio>     low-side input pin");
  Serial.println("  baud=<rate>    1200..2000000");
  Serial.println("  start | stop   enable / disable all outputs");
  Serial.println("  charge         charge the bootstrap capacitor now");
  Serial.println("  show           print the current settings");
  Serial.println("  defaults       restore the built-in defaults");
  Serial.println("  help | ?       this list");
}

// ---------------- Setting parsers ----------------
bool parseNumber(const char* s, long& out) {
  if (!s || !*s) return false;
  char* end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0') return false;
  out = v;
  return true;
}

bool parseMode(const char* s, uint8_t& out) {
  long n;
  if (parseNumber(s, n)) {
    if (n < 0 || n > 2) return false;
    out = (uint8_t)n;
    return true;
  }
  if (!strcmp(s, "blink") || !strcmp(s, "low"))                          { out = MODE_LOW_SIDE_BLINK;  return true; }
  if (!strcmp(s, "high")  || !strcmp(s, "hs"))                           { out = MODE_HIGH_SIDE_PWM;   return true; }
  if (!strcmp(s, "half")  || !strcmp(s, "bridge") || !strcmp(s, "hb"))   { out = MODE_HALF_BRIDGE_PWM; return true; }
  return false;
}

bool pinOk(long p) {
  if (p < 0 || p > 48) return false;
  if (!GPIO_IS_VALID_OUTPUT_GPIO((int)p)) return false;
#ifdef CONFIG_IDF_TARGET_ESP32
  if (p >= 6 && p <= 11) return false;       // SPI flash pins
#endif
  return true;
}

bool isStrappingPin(long p) {
#ifdef CONFIG_IDF_TARGET_ESP32
  return p == 0 || p == 2 || p == 5 || p == 12 || p == 15;
#else
  return false;
#endif
}

bool timingOk(uint32_t freqHz, uint32_t deadUs) {
  return 2UL * deadUs < 1000000UL / freqHz;
}

// Returns true when the setting was recognised and applied.
bool applySetting(const char* key, const char* val) {
  long n;

  if (!strcmp(key, "mode")) {
    uint8_t m;
    if (!parseMode(val, m)) { Serial.println("ERR mode: use blink|high|half or 0..2"); return true; }
    allOff();
    cfg.mode = m;
    resetBlink();
    if (running) chargeBootstrap();
    Serial.printf("OK mode=%s\n", modeName(cfg.mode));
    return true;
  }

  if (!strcmp(key, "freq")) {
    if (!parseNumber(val, n) || n < 100 || n > 50000) { Serial.println("ERR freq: 100..50000"); return true; }
    if (!timingOk((uint32_t)n, cfg.deadUs))           { Serial.println("ERR freq: period too short for current dead time"); return true; }
    cfg.freqHz = (uint32_t)n;
    Serial.printf("OK freq=%lu\n", (unsigned long)cfg.freqHz);
    return true;
  }

  if (!strcmp(key, "duty")) {
    if (!parseNumber(val, n) || n < 0 || n > 100) { Serial.println("ERR duty: 0..100"); return true; }
    cfg.duty = (uint8_t)n;
    Serial.printf("OK duty=%u\n", (unsigned)cfg.duty);
    return true;
  }

  if (!strcmp(key, "dead")) {
    if (!parseNumber(val, n) || n < 0 || n > 1000) { Serial.println("ERR dead: 0..1000"); return true; }
    if (!timingOk(cfg.freqHz, (uint32_t)n))        { Serial.println("ERR dead: too long for current frequency"); return true; }
    cfg.deadUs = (uint32_t)n;
    Serial.printf("OK dead=%lu\n", (unsigned long)cfg.deadUs);
    return true;
  }

  if (!strcmp(key, "maxon")) {
    if (!parseNumber(val, n) || n < 1 || n > 100000) { Serial.println("ERR maxon: 1..100000"); return true; }
    cfg.maxHighOnUs = (uint32_t)n;
    Serial.printf("OK maxon=%lu\n", (unsigned long)cfg.maxHighOnUs);
    return true;
  }

  if (!strcmp(key, "boot")) {
    if (!parseNumber(val, n) || n < 0 || n > 1000) { Serial.println("ERR boot: 0..1000"); return true; }
    cfg.bootMs = (uint32_t)n;
    Serial.printf("OK boot=%lu\n", (unsigned long)cfg.bootMs);
    return true;
  }

  if (!strcmp(key, "blink")) {
    if (!parseNumber(val, n) || n < 2 || n > 60000) { Serial.println("ERR blink: 2..60000"); return true; }
    cfg.blinkMs = (uint32_t)n;
    Serial.printf("OK blink=%lu\n", (unsigned long)cfg.blinkMs);
    return true;
  }

  if (!strcmp(key, "hin") || !strcmp(key, "lin")) {
    bool isHin = (key[0] == 'h');
    int  other = isHin ? cfg.linPin : cfg.hinPin;
    if (!parseNumber(val, n) || !pinOk(n)) { Serial.println("ERR pin: not a valid output GPIO"); return true; }
    if (n == other)                        { Serial.println("ERR pin: already used by the other input"); return true; }
    releasePins();
    if (isHin) cfg.hinPin = (int)n; else cfg.linPin = (int)n;
    initPins();
    resetBlink();
    Serial.printf("OK %s=GPIO%ld%s\n", key, n, isStrappingPin(n) ? "  (warning: strapping pin)" : "");
    return true;
  }

  if (!strcmp(key, "baud")) {
    if (!parseNumber(val, n) || n < 1200 || n > 2000000) { Serial.println("ERR baud: 1200..2000000"); return true; }
    Serial.printf("OK baud=%ld (reconnect your terminal)\n", n);
    Serial.flush();
    cfg.baud = (uint32_t)n;
    Serial.end();
    Serial.begin(cfg.baud);
    return true;
  }

  return false;
}

// ---------------- Command handling ----------------
void handleLine(char* line) {
  // Trim and lowercase
  while (*line && isspace((unsigned char)*line)) line++;
  size_t len = strlen(line);
  while (len > 0 && isspace((unsigned char)line[len - 1])) line[--len] = '\0';
  if (len == 0) return;
  for (char* c = line; *c; c++) *c = (char)tolower((unsigned char)*c);

  // Split into key and value at the first '=', ':' or whitespace
  char* key = line;
  char* val = nullptr;
  char* sep = strpbrk(line, "=: \t");
  if (sep) {
    *sep = '\0';
    val = sep + 1;
    while (*val && (isspace((unsigned char)*val) || *val == '=' || *val == ':')) val++;
    if (*val == '\0') val = nullptr;
  }

  // Outputs go low while a command is processed; the next cycle resumes cleanly.
  allOff();

  if (val) {
    if (!applySetting(key, val)) Serial.println("ERR unknown setting (type 'help')");
    return;
  }

  if (!strcmp(key, "help") || !strcmp(key, "?")) { printHelp();   return; }
  if (!strcmp(key, "show"))                      { printConfig(); return; }
  if (!strcmp(key, "stop"))  { running = false; allOff(); Serial.println("OK stopped"); return; }
  if (!strcmp(key, "start")) {
    running = true;
    resetBlink();
    chargeBootstrap();
    Serial.println("OK running");
    return;
  }
  if (!strcmp(key, "charge")) {
    chargeBootstrap();
    Serial.println("OK bootstrap charged");
    return;
  }
  if (!strcmp(key, "defaults")) {
    releasePins();
    cfg = DEFAULTS;
    initPins();
    resetBlink();
    Serial.end();
    Serial.begin(cfg.baud);
    Serial.println("OK defaults restored");
    printConfig();
    return;
  }

  Serial.println("ERR unknown command (type 'help')");
}

void pollSerial() {
  static char buf[64];
  static uint8_t n = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      buf[n] = '\0';
      n = 0;
      handleLine(buf);
    } else if (n < sizeof(buf) - 1) {
      buf[n++] = c;
    } else {
      n = 0;                       // Line too long: discard it
      Serial.println("ERR line too long");
    }
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(cfg.baud);
  initPins();
  printConfig();
  Serial.println("Type 'help' for the command list.");
  resetBlink();
  chargeBootstrap();
}

void loop() {
  // In PWM modes poll Serial only every few cycles to keep the timing tight.
  static uint8_t pollDiv = 0;
  if (!running || cfg.mode == MODE_LOW_SIDE_BLINK || ++pollDiv >= 32) {
    pollDiv = 0;
    pollSerial();
  }

  if (!running) {
    allOff();
    delay(1);
    return;
  }
  runTest();
}
