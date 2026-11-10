// T-Deck dual-boot selector. Lives in the factory partition; both firmwares hand the next boot back
// here at their start (DUAL_BOOT_SELECTOR), so every power-on comes here first.
//
// - Power-on, crash, brownout: a short menu, Meshtastic or WADAMESH, the last choice preselected,
//   continuing with it after a countdown.
// - Software restart or deep-sleep wake (a firmware rebooting itself, e.g. after a settings change):
//   straight on to the last choice, no menu.
// Keys: 1 / m = Meshtastic, 2 / w = WADAMESH, Enter or trackball press = the highlighted one,
// trackball up/down = move. The trackball press is NOT usable at power-on itself: it is GPIO0, the
// strapping pin, and held at reset it puts the chip into download mode.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <Adafruit_ST7789.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

// T-Deck pins (from the Meshtastic and MeshCore/WADAMESH T-Deck variants)
static const int PIN_POWER_ON = 10;   // peripherals, keyboard included
static const int PIN_I2C_SDA = 18, PIN_I2C_SCL = 8;
static const uint8_t KB_ADDR = 0x55;
static const int PIN_TFT_CS = 12, PIN_TFT_DC = 11, PIN_TFT_BL = 42;
static const int PIN_SPI_SCK = 40, PIN_SPI_MOSI = 41, PIN_SPI_MISO = 38;
static const int PIN_LORA_CS = 9, PIN_SD_CS = 39;   // shared SPI bus: keep the other devices deselected
static const int TB_UP = 3, TB_DOWN = 15, TB_PRESS = 0;

static const uint32_t COUNTDOWN_MS = 5000;

struct Choice {
  const char *name;
  esp_partition_subtype_t slot;
};
static const Choice CHOICES[] = {
  {"Meshtastic", ESP_PARTITION_SUBTYPE_APP_OTA_0},
  {"WADAMESH", ESP_PARTITION_SUBTYPE_APP_OTA_1},
};
static const int CHOICE_COUNT = 2;

static SPIClass displaySpi(HSPI);
static Adafruit_ST7789 tft(&displaySpi, PIN_TFT_CS, PIN_TFT_DC, -1);
static Preferences prefs;

static const esp_partition_t *slotPartition(int index) {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, CHOICES[index].slot, NULL);
}

// A slot is bootable only if it holds an app image; an empty slot is shown but cannot be chosen.
static bool slotHasApp(int index, esp_app_desc_t *desc) {
  const esp_partition_t *part = slotPartition(index);
  return part != NULL && esp_ota_get_partition_description(part, desc) == ESP_OK;
}

static int loadLast() {
  prefs.begin("dualboot", true);
  int last = prefs.getInt("last", 0);
  prefs.end();
  return (last >= 0 && last < CHOICE_COUNT) ? last : 0;
}

static void saveLast(int index) {
  prefs.begin("dualboot", false);
  if (prefs.getInt("last", -1) != index) prefs.putInt("last", index);   // no flash write when unchanged
  prefs.end();
}

static void bootInto(int index) {
  esp_app_desc_t desc;
  if (!slotHasApp(index, &desc)) return;
  saveLast(index);
  if (esp_ota_set_boot_partition(slotPartition(index)) == ESP_OK) {
    delay(50);
    esp_restart();
  }
}

static int readKey() {
  Wire.requestFrom(KB_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

static void drawMenu(int selected, uint32_t remainingMs, const bool *available, esp_app_desc_t *descs) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Inditas / Boot");
  for (int i = 0; i < CHOICE_COUNT; i++) {
    int y = 60 + i * 60;
    if (i == selected) tft.fillRect(5, y - 8, 310, 48, 0x2A69);
    tft.setTextColor(available[i] ? ST77XX_WHITE : 0x7BEF);
    tft.setTextSize(3);
    tft.setCursor(15, y);
    tft.printf("%d %s", i + 1, CHOICES[i].name);
    tft.setTextSize(1);
    tft.setCursor(17, y + 28);
    tft.print(available[i] ? descs[i].version : "(nincs telepitve / not installed)");
  }
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 200);
  tft.print("1/m, 2/w, Enter, trackball");
  tft.setCursor(10, 215);
  tft.printf("Automatikusan: %s  %lus", CHOICES[selected].name, (unsigned long)((remainingMs + 999) / 1000));
}

void setup() {
  esp_reset_reason_t reason = esp_reset_reason();
  int last = loadLast();
  esp_app_desc_t descs[CHOICE_COUNT];
  bool available[CHOICE_COUNT];
  for (int i = 0; i < CHOICE_COUNT; i++) available[i] = slotHasApp(i, &descs[i]);

  // A firmware restarting itself comes straight back to where it was.
  if ((reason == ESP_RST_SW || reason == ESP_RST_DEEPSLEEP) && available[last]) bootInto(last);

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_LORA_CS, OUTPUT);
  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(TB_UP, INPUT_PULLUP);
  pinMode(TB_DOWN, INPUT_PULLUP);
  pinMode(TB_PRESS, INPUT_PULLUP);
  delay(200);   // the keyboard controller needs a moment after power-on

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  displaySpi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_TFT_CS);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  tft.init(240, 320);
  tft.setRotation(3);   // 1 drew the menu upside down on a T-Deck Plus (first device test, 2026-11-10)

  int selected = available[last] ? last : (available[0] ? 0 : 1);
  uint32_t start = millis(), lastDraw = 0;
  int upPrev = HIGH, downPrev = HIGH;
  bool interacted = false;
  drawMenu(selected, COUNTDOWN_MS, available, descs);

  while (true) {
    uint32_t elapsed = millis() - start;
    if (!interacted && elapsed >= COUNTDOWN_MS && available[selected]) bootInto(selected);

    int key = readKey();
    int pick = -1;
    if (key == '1' || key == 'm' || key == 'M') pick = 0;
    else if (key == '2' || key == 'w' || key == 'W') pick = 1;
    else if (key == '\r' || key == '\n') pick = selected;
    if (digitalRead(TB_PRESS) == LOW) pick = selected;

    int up = digitalRead(TB_UP), down = digitalRead(TB_DOWN);
    bool moved = false;
    if (up == LOW && upPrev == HIGH) { selected = (selected + CHOICE_COUNT - 1) % CHOICE_COUNT; moved = true; }
    if (down == LOW && downPrev == HIGH) { selected = (selected + 1) % CHOICE_COUNT; moved = true; }
    upPrev = up;
    downPrev = down;
    if (key) interacted = true;
    if (moved) interacted = true;

    if (pick >= 0 && available[pick]) bootInto(pick);
    if (moved || (!interacted && millis() - lastDraw > 1000)) {
      drawMenu(selected, interacted ? 0 : COUNTDOWN_MS - min(elapsed, COUNTDOWN_MS), available, descs);
      lastDraw = millis();
    }
    delay(20);
  }
}

void loop() {}
