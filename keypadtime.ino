#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <time.h>

// WiFi credentials
const char* ssid = "SSID";
const char* password = "PASS";

// Weather API URL
const char* weather_url = "https://api.open-meteo.com/v1/forecast?latitude=40.29&longitude=-74.72&current=temperature_2m,weathercode&timezone=auto&temperature_unit=fahrenheit";

// Display and GPIO
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL 27
#define RELAY_PIN 25

// Touch toggle button position
#define TOGGLE_X 420
#define TOGGLE_Y 10
#define TOGGLE_W 50
#define TOGGLE_H 30

// Keypad constants
#define BUTTON_W 70
#define BUTTON_H 70
#define BUTTON_GAP 8
#define BUTTON_X_START 10
#define BUTTON_Y_START 10

int buttonX[11], buttonY[11];

const String presetCode = "1234";
String enteredCode = "";

#define BUTTON_COLOR TFT_BLUE
#define BUTTON_PRESS_COLOR TFT_GREEN
#define TEXT_COLOR TFT_WHITE

#define TEXT_X_START 270
#define TEXT_Y_START 60
#define TEXT_WIDTH 320
#define TEXT_HEIGHT 40
#define ENTERED_PIN_Y_START 100

#define STATUS_X_START 300
#define STATUS_Y_START 240
#define STATUS_WIDTH 180
#define STATUS_HEIGHT 40



bool isUnlocked = false;
unsigned long unlockTime = 0;
bool showWeather = false;

// Variables to control refresh rate
unsigned long lastUpdateTime = 0;  // Variable to store the last update time
const unsigned long updateInterval = 60000;  // 10 seconds interval (in milliseconds)

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Time synced");
}

void drawToggleButton(bool active) {
  tft.fillRect(TOGGLE_X, TOGGLE_Y, TOGGLE_W, TOGGLE_H, TFT_DARKGREY);
  tft.drawRect(TOGGLE_X, TOGGLE_Y, TOGGLE_W, TOGGLE_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);  // Fit the text inside the small button
  tft.setCursor(TOGGLE_X + 2, TOGGLE_Y + 10);
  tft.print(active ? "Lock" : "Home");
}

void drawKeypad() {
  for (int i = 0; i < 9; i++) {
    int col = i % 3;
    int row = i / 3;
    buttonX[i] = BUTTON_X_START + (BUTTON_W + BUTTON_GAP) * col;
    buttonY[i] = BUTTON_Y_START + (BUTTON_H + BUTTON_GAP) * row;
    tft.fillRect(buttonX[i], buttonY[i], BUTTON_W, BUTTON_H, BUTTON_COLOR);
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(3);
    tft.setCursor(buttonX[i] + BUTTON_W / 4, buttonY[i] + BUTTON_H / 4);
    tft.print(i + 1);
  }
  buttonX[9] = BUTTON_X_START + (BUTTON_W + BUTTON_GAP) * 1;
  buttonY[9] = BUTTON_Y_START + (BUTTON_H + BUTTON_GAP) * 3;
  drawSpecialButton("0", buttonX[9], buttonY[9], 3);

  buttonX[10] = BUTTON_X_START + (BUTTON_W + BUTTON_GAP) * 2;
  buttonY[10] = BUTTON_Y_START + (BUTTON_H + BUTTON_GAP) * 3;
  drawSpecialButton("Clear", buttonX[10], buttonY[10], 2);
}

void drawSpecialButton(const char *text, int x, int y, int tSize) {
  tft.fillRect(x, y, BUTTON_W, BUTTON_H, BUTTON_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(tSize);
  tft.setCursor(x + 10, y + BUTTON_H / 4);
  tft.print(text);
}

void updateStatus(const char* label, const char* status, uint16_t color) {
  tft.fillRect(STATUS_X_START, STATUS_Y_START, STATUS_WIDTH, STATUS_HEIGHT * 2, TFT_BLACK);
  tft.setCursor(STATUS_X_START, STATUS_Y_START);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print(label);
  tft.setCursor(STATUS_X_START, STATUS_Y_START + 20);
  tft.setTextColor(color);
  tft.print(status);
}

String getWeatherSymbol(int code) {
  if (code == 0) return "Clear Sky";
  if (code <= 3) return "Partly Cloudy";
  if (code <= 48) return "Foggy or Hazy";
  if (code <= 67) return "Rainy";
  if (code <= 77) return "Snowy";
  if (code <= 99) return "Thunderstorm";
  return "Unknown";
}

void showKeypadScreen() {
  tft.fillScreen(TFT_BLACK);
  drawKeypad();
  tft.setCursor(TEXT_X_START, TEXT_Y_START);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.print("Enter PIN: ");
  updateStatus("Status:", isUnlocked ? "Unlocked" : "Locked", isUnlocked ? TFT_GREEN : TFT_RED);
  drawToggleButton(showWeather);
}

void showWeatherScreen() {
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  
  // Get current time (UTC) and convert to Eastern Time
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  timeinfo->tm_hour -= 4;  // Adjust to Eastern Time Zone (UTC - 4)
  if (timeinfo->tm_hour < 0) {
    timeinfo->tm_hour += 24;  // Wrap around if it goes negative
  }

  // Format time and date
  char timeString[32];
  strftime(timeString, sizeof(timeString), "%I:%M %p", timeinfo);

  // Set text size for time (larger text)
  tft.setTextSize(10);  // Bigger time text
  int timeWidth = tft.textWidth(timeString);
  int timeX = (tft.width() - timeWidth) / 2;
  int timeY = 70;

  tft.setCursor(timeX, timeY);
  tft.println(timeString);

  char dateString[32];
  strftime(dateString, sizeof(dateString), "%b %d, %Y", timeinfo);

  // Set text size for date (a little bigger than default)
  tft.setTextSize(3);  // Slightly bigger date text
  int dateWidth = tft.textWidth(dateString);
  int dateX = (tft.width() - dateWidth) / 2;
  int dateY = timeY + 120;

  tft.setCursor(dateX, dateY);
  tft.println(dateString);

  // HTTP request for weather data
  HTTPClient http;
  http.begin(weather_url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    float tempF = doc["current"]["temperature_2m"];
    int code = doc["current"]["weathercode"][0];
    String symbol = getWeatherSymbol(code);

    // Set text size for weather information (same size for all)
    tft.setTextSize(3);  // Weather text size (smaller than time)
    
    // Weather info on a single line
    String weatherString = String(tempF, 1) + "*F | " + symbol;
    
    int weatherWidth = tft.textWidth(weatherString);
    int weatherX = (tft.width() - weatherWidth) / 2;
    int weatherY = dateY + 40; // Position below the date

    tft.setCursor(weatherX, weatherY);
    tft.println(weatherString);

  } else {
    tft.setCursor(10, 130);
    tft.println("Error getting data");
  }

  http.end();  // Close the HTTP request

  // Redraw the toggle button
  drawToggleButton(showWeather);
}





void handleButtonPress(int digit) {
  if (digit == 10) {
    enteredCode = "";
    tft.fillRect(TEXT_X_START, TEXT_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
    tft.setCursor(TEXT_X_START, TEXT_Y_START);
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(3);
    tft.print("Enter PIN: ");
    return;
  }

  if (digit < 9) enteredCode += String(digit + 1);
  else if (digit == 9) enteredCode += "0";

  tft.fillRect(TEXT_X_START, ENTERED_PIN_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
  tft.setCursor(TEXT_X_START, ENTERED_PIN_Y_START);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.print(enteredCode);

  if (enteredCode.length() > 4) {
    tft.fillRect(TEXT_X_START, ENTERED_PIN_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
    tft.setCursor(TEXT_X_START, ENTERED_PIN_Y_START);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.print("Incorrect Code");
    delay(2000);
    enteredCode = "";
    tft.fillRect(TEXT_X_START, ENTERED_PIN_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
  }

  if (enteredCode.length() == presetCode.length()) {
    if (enteredCode == presetCode) {
      digitalWrite(RELAY_PIN, HIGH);
      unlockTime = millis();
      isUnlocked = true;
      updateStatus("Status:", "Unlocked", TFT_GREEN);
    } else {
      tft.fillRect(TEXT_X_START, ENTERED_PIN_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
      tft.setCursor(TEXT_X_START, ENTERED_PIN_Y_START);
      tft.setTextColor(TFT_RED);
      tft.setTextSize(2);
      tft.print("Wrong Code");
      delay(2000);
      updateStatus("Status:", "Locked", TFT_RED);
    }
    enteredCode = "";
    tft.fillRect(TEXT_X_START, ENTERED_PIN_Y_START, TEXT_WIDTH, TEXT_HEIGHT, TFT_BLACK);
  }
}

void checkTouch() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    // Flip X and Y to match display orientation
    x = 480 - x;
    y = y;

    if (x > TOGGLE_X && x < TOGGLE_X + TOGGLE_W && y > TOGGLE_Y && y < TOGGLE_Y + TOGGLE_H) {
      showWeather = !showWeather;
      if (showWeather) showWeatherScreen();
      else showKeypadScreen();
      delay(500);
      return;
    }
    if (!showWeather) {
      for (int i = 0; i < 11; i++) {
        if (x > buttonX[i] && x < buttonX[i] + BUTTON_W &&
            y > buttonY[i] && y < buttonY[i] + BUTTON_H) {
          handleButtonPress(i);
          delay(200);
          break;
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(RELAY_PIN, LOW);

  tft.init();
  tft.setRotation(3); // flipped 180 degrees
  tft.fillScreen(TFT_BLACK);
  connectToWiFi();
  showKeypadScreen();
}

void loop() {
  checkTouch();

  // Check if 10 seconds have passed since the last update
  if (millis() - lastUpdateTime >= updateInterval) {
    // Update weather and time
    if (showWeather) {
      showWeatherScreen();
    } else {
      showKeypadScreen();
    }

    // Update the last update time
    lastUpdateTime = millis();
  }

  if (isUnlocked && millis() - unlockTime >= 5000) {
    digitalWrite(RELAY_PIN, LOW);
    updateStatus("Status:", "Locked", TFT_RED);
    isUnlocked = false;
  }
}