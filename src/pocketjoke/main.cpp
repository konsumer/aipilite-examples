#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include "../pins.h"
#include "index_htm.h"

// Screen configuration
Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_CLK, PIN_LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7735(bus, PIN_LCD_RST, 3, false, 128, 128, 0, 0);

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;

// Configuration
String wifi_ssid = "";
String wifi_pass = "";
String joke_categories = "Any";
String joke_url = "https://v2.jokeapi.dev/joke/Any?format=json";

// Network profile caching variables
uint32_t cached_ip = 0, cached_gw = 0, cached_sn = 0, cached_dns = 0;
bool has_ip_cache = false;

// Pagination variables
String current_joke = "";
int total_pages = 0;
int current_page = 0;
const int MAX_PAGES = 10;
int page_indices[MAX_PAGES]; // Array to store string index markers for each page start

// Configuration metrics for text layout
const int CHAR_WIDTH = 6;   // Default 5x7 font width + 1 pixel padding
const int CHAR_HEIGHT = 8;  // Default 5x7 font height + 1 pixel padding
const int MARGIN = 4;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 128;
const int MAX_COLS = (SCREEN_WIDTH - (MARGIN * 2)) / CHAR_WIDTH;
const int MAX_ROWS = (SCREEN_HEIGHT - (MARGIN * 2)) / CHAR_HEIGHT - 1; // leave 1 row for footer

void showMessage(const char* msg) {
    gfx->fillScreen(RGB565_BLACK);
    gfx->setCursor(MARGIN, MARGIN + 10);
    gfx->print(msg);
}

// Scans the text and calculates layout boundary positions
void calculatePages() {
    int len = current_joke.length();
    if (len == 0) return;

    current_page = 0;
    total_pages = 0;
    page_indices[0] = 0;

    int current_idx = 0;
    while (current_idx < len && total_pages < MAX_PAGES - 1) {
        int row = 0;
        int col = 0;
        int last_space_idx = -1;
        int scan_idx = current_idx;

        // Simulate page rendering space constraints
        while (row < MAX_ROWS && scan_idx < len) {
            char c = current_joke[scan_idx];

            if (c == '\n') {
                row++;
                col = 0;
                scan_idx++;
                current_idx = scan_idx;
                continue;
            }
            if (c == ' ') {
                last_space_idx = scan_idx;
            }

            col++;
            if (col >= MAX_COLS) {
                if (last_space_idx > current_idx) {
                    // Wrap smoothly at word boundary space marker
                    scan_idx = last_space_idx + 1;
                }
                row++;
                col = 0;
            } else {
                scan_idx++;
            }
        }

        // If space tracking ends without hitting newline rules
        if (scan_idx >= len) {
            current_idx = len;
        } else {
            current_idx = scan_idx;
        }

        total_pages++;
        page_indices[total_pages] = current_idx;
    }
}

void displayCurrentPage() {
    gfx->fillScreen(RGB565_BLACK);
    
    int start_idx = page_indices[current_page];
    int end_idx = (current_page + 1 < total_pages) ? page_indices[current_page + 1] : current_joke.length();
    
    gfx->setCursor(MARGIN, MARGIN);
    int col = 0;
    int line_count = 0;

    // Word wrap execution matching simulation calculations
    for (int i = start_idx; i < end_idx; i++) {
        char c = current_joke[i];
        if (c == '\n') {
            gfx->println();
            col = 0;
            line_count++;
            continue;
        }
        gfx->print(c);
        col++;
        if (col >= MAX_COLS) {
            gfx->println();
            col = 0;
            line_count++;
        }
    }

    // Print navigational user helper menu footer at the base of screen
    gfx->setCursor(MARGIN, SCREEN_HEIGHT - MARGIN - CHAR_HEIGHT);
    gfx->setTextColor(RGB565_GREEN);
    
    String footer = "P" + String(current_page + 1) + "/" + String(total_pages);
    if (current_page + 1 < total_pages) {
        footer += "  (B:Next)";
    } else {
        footer += "  (End)";
    }
    gfx->print(footer.c_str());
    gfx->setTextColor(RGB565_WHITE); // Reset text back to standard white
}

void loadConfiguration() {
    prefs.begin("joke-client", true);
    wifi_ssid = prefs.getString("ssid", "");
    wifi_pass = prefs.getString("pass", "");
    // Default to 'Any' JSON format URL if configuration preference does not exist yet
    joke_url = prefs.getString("url", "https://v2.jokeapi.dev/joke/Any?format=json");

    Serial.printf("LOAD: (%s:%s) %s\n", wifi_ssid.c_str(), wifi_pass.c_str(), joke_url.c_str());
    
    has_ip_cache = prefs.getBool("has_cache", false);
    cached_ip = prefs.getUInt("ip", 0);
    cached_gw = prefs.getUInt("gw", 0);
    cached_sn = prefs.getUInt("sn", 0);
    cached_dns = prefs.getUInt("dns", 0);
    prefs.end();
}

bool fetchJoke() {
    if (wifi_ssid == "") {
        showMessage("No Wi-Fi set.\nHold B on boot\nfor config.");
        delay(4000);
        return false;
    }

    showMessage("Connecting...");
    WiFi.mode(WIFI_STA);
    if (has_ip_cache) {
        WiFi.config(IPAddress(cached_ip), IPAddress(cached_gw), IPAddress(cached_sn), IPAddress(cached_dns));
    }
    
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 6000) {
        delay(50);
    }

    if (WiFi.status() != WL_CONNECTED) {
        showMessage("Wi-Fi Timeout!");
        delay(2000);
        return false;
    }

    // Capture standard DHCP info if it's the first connection
    if (!has_ip_cache) {
        Serial.println("IP not cached, setting up with DHCP");
        prefs.begin("joke-client", false);
        prefs.putBool("has_cache", true);
        prefs.putUInt("ip", uint32_t(WiFi.localIP()));
        prefs.putUInt("gw", uint32_t(WiFi.gatewayIP()));
        prefs.putUInt("sn", uint32_t(WiFi.subnetMask()));
        prefs.putUInt("dns", uint32_t(WiFi.dnsIP()));
        prefs.end();
    }

    showMessage("Fetching joke...");
    HTTPClient http;
    http.begin(joke_url); // Pass the direct custom JSON endpoint path saved from web UI
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String jsonPayload = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonPayload);
        
        if (error) {
            showMessage("JSON Parse Error");
            delay(2000);
            return false;
        }

        if (doc["error"].as<bool>() == true) {
            showMessage("JokeAPI reported\nan internal error.");
            delay(2000);
            return false;
        }

        String type = doc["type"].as<String>();

        // Dynamically unpack based on the JokeAPI structure rules
        if (type == "single") {
            current_joke = doc["joke"].as<String>();
        } 
        else if (type == "twopart") {
            String setup = doc["setup"].as<String>();
            String delivery = doc["delivery"].as<String>();
            // Combine with explicit structural double spacing formatting
            current_joke = setup + "\n\n" + delivery;
        }

        current_joke.trim();
        calculatePages(); // Feeds cleanly into text wrapping calculation loop logic
        return true;
    } else {
        String errorMsg = "HTTP Error:\n" + String(httpCode);
        showMessage(errorMsg.c_str());
        delay(3000);
        http.end();
        return false;
    }
}

void handlePortalRoot() {
    // 1. Scan for nearby networks
    int n = WiFi.scanNetworks();
    String optionsHtml = "";

    if (n == 0) {
        optionsHtml += "<option disabled>No networks found</option>";
    } else {
        // De-duplicate network SSIDs to clean up the dropdown menu
        String seenSSIDs[30];
        int uniqueCount = 0;

        for (int i = 0; i < n && uniqueCount < 30; ++i) {
            String currentScanned = WiFi.SSID(i);
            bool isDuplicate = false;

            for (int j = 0; j < uniqueCount; j++) {
                if (seenSSIDs[j] == currentScanned) {
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate && currentScanned.length() > 0) {
                seenSSIDs[uniqueCount++] = currentScanned;
                
                // Highlight the network if it matches the one currently stored
                String selected = (currentScanned == wifi_ssid) ? " selected" : "";
                optionsHtml += "<option value=\"" + currentScanned + "\"" + selected + ">" 
                               + currentScanned + " (" + String(WiFi.RSSI(i)) + " dBm)</option>\n";
            }
        }
    }
    // Delete scan results from memory
    WiFi.scanDelete();

    // 2. Inject results into HTML layout
    String html = String(PORTAL_HTML);
    html.replace("{{WIFI_OPTIONS}}", optionsHtml);
    html.replace("{{SSID}}", wifi_ssid);
    html.replace("{{PASS}}", wifi_pass);
    html.replace("{{URL}}", joke_url);

    server.send(200, "text/html", html);
}

void handlePortalSave() {
    // Check if the server received a valid JSON request body
    if (server.hasArg("plain") == false) {
        server.send(400, "text/plain", "Body Missing");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "text/plain", "Invalid JSON Format");
        return;
    }

    // Save variables directly out of your new JSON schema
    wifi_ssid = doc["ssid"].as<String>();
    wifi_pass = doc["password"].as<String>();
    
    // Save the entire customized URL constructed by your UI
    joke_url = doc["url"].as<String>(); 

    prefs.begin("joke-client", false);
    prefs.putString("ssid", wifi_ssid);
    prefs.putString("pass", wifi_pass);
    prefs.putString("url", joke_url); // Store full config URL
    prefs.putBool("has_cache", false); 
    prefs.end();

    Serial.printf("SAVE: (%s:%s) %s\n", wifi_ssid.c_str(), wifi_pass.c_str(), joke_url.c_str());

    server.send(200, "application/json", "{\"status\":\"success\"}");
    delay(2000);
    ESP.restart();
}

void startCaptivePortal() {
    showMessage("Scanning & Setting AP...");
    
    // Crucial step: AP_STA allows running an Access Point while scanning environment frequencies
    WiFi.mode(WIFI_AP_STA); 
    WiFi.softAP("Joke-Config");
    delay(100); 
    
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    
    server.on("/", HTTP_GET, handlePortalRoot);
    server.on("/save", HTTP_POST, handlePortalSave);
    server.on("/generate_204", handlePortalRoot); 
    server.onNotFound(handlePortalRoot);
    
    server.begin();
    showMessage("CONFIG MODE AP\nSSID: Joke-Config\nIP: 192.168.4.1");

    while (true) {
        dnsServer.processNextRequest();
        server.handleClient();
        delay(10);
    }
}


// BTN_A: next page; last page sleeps. BTN_B (held): enter config anytime.
void runJokeViewer() {
    if (!fetchJoke()) return;
    displayCurrentPage();

    // Wait for wakeup button to release before handling new presses
    while (digitalRead(PIN_BTN_A) == LOW) delay(10);
    delay(50);

    while (true) {
        if (digitalRead(PIN_BTN_B) == LOW) {
            delay(200); // debounce
            if (digitalRead(PIN_BTN_B) == LOW) {
                startCaptivePortal();
            }
        }
        if (digitalRead(PIN_BTN_A) == LOW) {
            delay(50); // debounce
            if (digitalRead(PIN_BTN_A) != LOW) { delay(30); continue; }
            while (digitalRead(PIN_BTN_A) == LOW) delay(10); // wait for release
            if (current_page + 1 < total_pages) {
                current_page++;
                displayCurrentPage();
            } else {
                return; // last page — fall through to enterDeepSleep
            }
        }
        delay(30);
    }
}

void enterDeepSleep() {
    showMessage("Going to sleep...");
    delay(800);
    gfx->displayOff();

    // BTN_B (GPIO 42) is not RTC-capable on ESP32-S3 — EXT1 requires RTC GPIO (0-21)
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_A, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    delay(50); // let pulldowns settle before reading buttons

    pinMode(PIN_PWR_CTL, OUTPUT);
    digitalWrite(PIN_PWR_CTL, HIGH);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);

    loadConfiguration();
    Serial.printf("LOAD ssid='%s' url='%s'\n", wifi_ssid.c_str(), joke_url.c_str());

    // esp_restart() preserves RTC memory, so wakeup_cause survives a software reset.
    // Only trust it when the reset reason confirms we actually came from deep sleep.
    esp_reset_reason_t reset_reason = esp_reset_reason();
    bool from_deep_sleep = (reset_reason == ESP_RST_DEEPSLEEP);
    esp_sleep_wakeup_cause_t wakeup_reason = from_deep_sleep
        ? esp_sleep_get_wakeup_cause()
        : ESP_SLEEP_WAKEUP_UNDEFINED;
    Serial.printf("reset_reason=%d from_deep_sleep=%d wakeup_reason=%d BTN_A=%d BTN_B=%d\n",
        reset_reason, from_deep_sleep, wakeup_reason, digitalRead(PIN_BTN_A), digitalRead(PIN_BTN_B));

    if (digitalRead(PIN_BTN_B) == LOW) {
        delay(200);
        if (digitalRead(PIN_BTN_B) == LOW) {
            Serial.println("-> config (BTN_B held at boot)");
            startCaptivePortal();
        }
    }

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pin_mask & (1ULL << PIN_BTN_B)) {
            Serial.println("-> config (EXT1 BTN_B)");
            startCaptivePortal();
        } else {
            Serial.println("-> joke viewer (EXT1 BTN_A)");
            runJokeViewer();
        }
    } else {
        if (wifi_ssid != "") {
            Serial.println("-> joke viewer (normal boot)");
            runJokeViewer();
        } else {
            Serial.println("-> idle (no ssid)");
            showMessage("Press A for Joke\nHold B for Config");
            delay(3000);
        }
    }

    enterDeepSleep();
}

void loop() {}
