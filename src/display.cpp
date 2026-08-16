#include "display.h"
#include "config.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>

static MatrixPanel_I2S_DMA* panel = nullptr;
static PNG png;

// Full-resolution decode target (RGB565, already composited over black),
// allocated per sprite and freed once scaled onto the panel.
static uint16_t* g_img = nullptr;
static int g_w = 0, g_h = 0;

// Top row of the current name band (1 or 2 lines), pinned to the bottom of the
// panel. Set whenever the sprite or name is drawn; the seconds overlay uses it to
// clear exactly the name band and never the sprite above it.
static int s_nameTopY = PANEL_HEIGHT - TEXT_GLYPH_H;

static uint16_t white()  { return MatrixPanel_I2S_DMA::color565(255, 255, 255); }
static uint16_t yellow() { return MatrixPanel_I2S_DMA::color565(255, 210, 0); }
static uint16_t blue()   { return MatrixPanel_I2S_DMA::color565(110, 170, 255); }

// PNGdec callback: composite one decoded line over black into g_img.
static int pngDraw(PNGDRAW* pDraw) {
    if (!g_img) return 0;
    uint16_t* dst = &g_img[pDraw->y * g_w];
    png.getLineAsRGB565(pDraw, dst, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
    return 1;
}

void displayInit() {
    HUB75_I2S_CFG::i2s_pins pins = {
        PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
        PIN_A,  PIN_B,  PIN_C,  PIN_D,  PIN_E,
        PIN_LAT, PIN_OE, PIN_CLK
    };
    HUB75_I2S_CFG cfg(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN, pins);
    panel = new MatrixPanel_I2S_DMA(cfg);
    panel->begin();
    panel->setBrightness8(PANEL_BRIGHTNESS);
    panel->clearScreen();
    panel->setTextWrap(false);
}

void displayClear() {
    if (panel) panel->clearScreen();
}

void displaySetBrightness(uint8_t b) {
    if (panel) panel->setBrightness8(b);
}

// ---- Name layout helpers -----------------------------------------------------

// A name only needs the animated marquee if it can't fit on two static lines.
bool displayNameScrolls(const char* name) {
    return (int)strlen(name) > NAME_CHARS_PER_LINE * NAME_MAX_LINES;   // >20 chars
}

// Split `name` into up to two lines (each <= NAME_CHARS_PER_LINE). Prefer a space
// break nearest the middle with both halves fitting; else a balanced hard split.
// Returns the number of lines used (1 or 2). Assumes len <= 2*NAME_CHARS_PER_LINE.
static int wrapName(const char* name, char* l1, char* l2, size_t cap) {
    int n = (int)strlen(name);
    if (n <= NAME_CHARS_PER_LINE) {
        strncpy(l1, name, cap - 1); l1[cap - 1] = 0; l2[0] = 0;
        return 1;
    }
    const int mid = n / 2;
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (name[i] != ' ') continue;
        int left = i, right = n - i - 1;
        if (left <= NAME_CHARS_PER_LINE && right <= NAME_CHARS_PER_LINE &&
            (best < 0 || abs(i - mid) < abs(best - mid))) {
            best = i;
        }
    }
    int split;
    bool dropSpace;
    if (best >= 0) {
        split = best; dropSpace = true;          // break on the space
    } else {
        split = mid;                             // balanced hard split, clamped
        if (split > NAME_CHARS_PER_LINE) split = NAME_CHARS_PER_LINE;
        if (n - split > NAME_CHARS_PER_LINE) split = n - NAME_CHARS_PER_LINE;
        dropSpace = false;
    }
    int len1 = split;
    int start2 = split + (dropSpace ? 1 : 0);
    int len2 = n - start2;
    if (len1 > (int)cap - 1) len1 = cap - 1;
    if (len2 > (int)cap - 1) len2 = cap - 1;
    memcpy(l1, name, len1); l1[len1] = 0;
    memcpy(l2, name + start2, len2); l2[len2] = 0;
    return 2;
}

// Rows the name will occupy (1 or 2). Marquee names live on a single bottom row.
static int planLines(const char* name) {
    if (displayNameScrolls(name)) return 1;
    char a[24], b[24];
    return wrapName(name, a, b, sizeof(a));
}

// Visible height of the name band for a given line count (15px for 2, 7px for 1).
static int nameHeightFor(int lines) {
    return (lines == 2) ? (TEXT_LINE_H + TEXT_GLYPH_H) : TEXT_GLYPH_H;
}

static void drawCentered(const char* s, int y) {
    int w = (int)strlen(s) * TEXT_CHAR_W;
    panel->setCursor((PANEL_WIDTH - w) / 2, y);
    panel->print(s);
}

// ---- State 1: loading animation ---------------------------------------------
// Runs on core 0 so the dots keep cycling while the main loop blocks on a fetch.

static TaskHandle_t s_loadingTask = nullptr;
static volatile bool s_loadingRun = false;

static void loadingTaskFn(void*) {
    const int y = (PANEL_HEIGHT - TEXT_GLYPH_H) / 2;
    int step = 0;
    while (s_loadingRun) {
        int dots = step % 4;
        panel->fillRect(0, y - 1, PANEL_WIDTH, TEXT_GLYPH_H + 2, 0);
        panel->setTextSize(1);
        panel->setTextColor(blue());
        panel->setCursor(2, y);          // fixed anchor so "Loading" doesn't jump
        panel->print("Loading");
        for (int i = 0; i < dots; i++) panel->print('.');
        step++;
        vTaskDelay(pdMS_TO_TICKS(350));
    }
    s_loadingTask = nullptr;
    vTaskDelete(nullptr);
}

void displayLoadingStart() {
    if (!panel || s_loadingTask) return;
    panel->fillScreen(0);
    s_loadingRun = true;
    xTaskCreatePinnedToCore(loadingTaskFn, "loading", 4096, nullptr, 1, &s_loadingTask, 0);
}

void displayLoadingStop() {
    if (!s_loadingTask) return;
    s_loadingRun = false;
    while (s_loadingTask) vTaskDelay(pdMS_TO_TICKS(5));   // wait for it to exit
}

// ---- State 2: sprite + name -------------------------------------------------

bool displayDrawSprite(const uint8_t* data, size_t len, const char* name) {
    if (!panel) return false;
    if (png.openRAM((uint8_t*)data, (int)len, pngDraw) != PNG_SUCCESS) return false;

    g_w = png.getWidth();
    g_h = png.getHeight();
    if (g_w <= 0 || g_h <= 0) { png.close(); return false; }

    size_t bytes = (size_t)g_w * g_h * sizeof(uint16_t);
    g_img = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!g_img) g_img = (uint16_t*)malloc(bytes);
    if (!g_img) { png.close(); return false; }

    int rc = png.decode(nullptr, 0);
    png.close();
    if (rc != PNG_SUCCESS) { free(g_img); g_img = nullptr; return false; }

    displayLoadingStop();   // sprite is ready — end State 1 before we draw

    // Layout: name pinned to the bottom, sprite fills everything above it. A
    // 1-line name leaves a 56px sprite; a 2-line name leaves 48px.
    const int lines = planLines(name);
    s_nameTopY = PANEL_HEIGHT - nameHeightFor(lines);
    const int spriteAreaH = s_nameTopY - LAYOUT_GAP;

    panel->fillScreen(0);   // fresh frame (clears prior sprite / loading pixels)

    // Nearest-neighbor scale to a centered square (keeps pixel-art crisp).
    const int size = spriteAreaH < PANEL_WIDTH ? spriteAreaH : PANEL_WIDTH;
    const int offx = (PANEL_WIDTH - size) / 2;
    const int offy = (spriteAreaH - size) / 2;   // 0 when height-limited
    for (int dy = 0; dy < size; dy++) {
        int sy = dy * g_h / size;
        for (int dx = 0; dx < size; dx++) {
            int sx = dx * g_w / size;
            panel->drawPixel(offx + dx, offy + dy, g_img[sy * g_w + sx]);
        }
    }

    free(g_img);
    g_img = nullptr;
    return true;
}

void displayDrawName(const char* name) {
    if (!panel) return;
    const int lines = planLines(name);
    s_nameTopY = PANEL_HEIGHT - nameHeightFor(lines);
    panel->fillRect(0, s_nameTopY, PANEL_WIDTH, PANEL_HEIGHT - s_nameTopY, 0);
    panel->setTextSize(1);
    panel->setTextColor(white());

    // Too long for two lines: single-line marquee on the bottom row.
    if (displayNameScrolls(name)) {
        const int y = PANEL_HEIGHT - TEXT_GLYPH_H;
        const int w = (int)strlen(name) * TEXT_CHAR_W;
        const int total = w + 12;
        const int off = (int)((millis() / 40) % total);   // ~25 px/s
        panel->setCursor(-off, y);         panel->print(name);
        panel->setCursor(-off + total, y); panel->print(name);
        return;
    }

    char l1[24], l2[24];
    if (wrapName(name, l1, l2, sizeof(l1)) == 1) {
        drawCentered(l1, PANEL_HEIGHT - TEXT_GLYPH_H);     // single bottom line
    } else {
        drawCentered(l1, s_nameTopY);                      // top line
        drawCentered(l2, s_nameTopY + TEXT_LINE_H);        // bottom line
    }
}

// ---- State 3: duration overlay ----------------------------------------------

void displayDrawDuration(int seconds) {
    if (!panel) return;
    // Clear only the current name band, leaving the sprite above intact.
    panel->fillRect(0, s_nameTopY, PANEL_WIDTH, PANEL_HEIGHT - s_nameTopY, 0);
    char buf[8];
    snprintf(buf, sizeof(buf), "%ds", seconds);
    panel->setTextSize(1);
    panel->setTextColor(yellow());
    drawCentered(buf, PANEL_HEIGHT - TEXT_GLYPH_H);        // bottom-most row
}

void displaySetup(const char* ssid, const char* ip) {
    displayLoadingStop();
    if (!panel) return;
    panel->fillScreen(0);
    panel->setTextSize(1);
    panel->setTextWrap(true);            // long SSID / IP flow onto a second line
    panel->setCursor(0, 1);
    panel->setTextColor(yellow()); panel->print("WiFi setup\n");
    panel->setTextColor(white());  panel->print("join AP:\n");
    panel->setTextColor(blue());   panel->print(ssid); panel->print("\n");
    panel->setTextColor(white());  panel->print("open:\n");
    panel->setTextColor(blue());   panel->print(ip);
    panel->setTextWrap(false);
}

void displayStatus(const char* msg) {
    displayLoadingStop();
    if (!panel) return;
    panel->fillScreen(0);
    panel->setTextSize(1);
    panel->setTextWrap(true);
    panel->setTextColor(blue());
    panel->setCursor(2, 26);
    panel->print(msg);
    panel->setTextWrap(false);
}
