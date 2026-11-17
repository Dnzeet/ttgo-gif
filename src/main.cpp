// TTGO GIF Player — LilyGo T-Display (ESP32, ST7789 135x240)
//
//  - GIF diunggah lewat web UI (WiFi yang sama; fallback mode AP bila tidak tersambung)
//  - Resize/kompresi GIF dilakukan di browser, jadi ESP32 hanya memutar file kecil
//  - Tombol bawah (GPIO0)  : slide berikutnya
//  - Tombol atas  (GPIO35) : klik = info (IP, baterai), dobel klik = power off
//  - Nyalakan lagi        : tekan tombol atas sekali

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>
#include <algorithm>
#include <vector>
#include "esp_sleep.h"
#include "webui.h"

// ---------------------------------------------------------------- konfigurasi
#define BTN_TOP         35      // atas: info / dobel klik = power off (ext0 wake)
#define BTN_BOTTOM      0       // bawah: slide berikutnya
#define BAT_EN          14      // aktifkan pembagi tegangan baterai (HIGH hanya saat mengukur)
#define BAT_ADC         34
#define BAT_VALID_MV    2800    // di bawah ini dianggap tanpa baterai
#define DISP_W          240
#define DISP_H          135
#define ROTATION        1
#define GIF_DIR         "/gifs"
#define DOUBLE_CLICK_MS 450     // jarak antar-penekanan untuk dobel klik
#define INFO_MS         5000
#define WIFI_TRY_MS     12000
#define AP_SSID         "TTGO-GIF"
#define AP_PASS         "12345678"
#define HOSTNAME        "ttgogif"

TFT_eSPI tft;
AnimatedGIF gif;
WebServer server(80);
DNSServer dns;
Preferences prefs;

struct GifEntry { String name; uint32_t size; };
std::vector<GifEntry> gifs;

enum Mode { M_PLAYING, M_INFO, M_EMPTY, M_UPLOAD };
Mode mode = M_EMPTY;

int cur = 0;
bool gifOpen = false, staticGif = false;
uint32_t nextFrameAt = 0, infoUntil = 0;
uint16_t frameCount = 0;
int gOffX = 0, gOffY = 0;

bool apMode = false;
String staSsid;

bool pendingRescan = false;
String pendingName;
uint32_t restartAt = 0;

// upload
File uploadFile;
uint32_t uploadSeen = 0;
String uploadName, uploadErr;
bool uploadOk = false;

// ------------------------------------------------------------------- baterai
int readBatteryMv() {
  pinMode(BAT_EN, OUTPUT);
  digitalWrite(BAT_EN, HIGH);
  delay(10);
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) { sum += analogReadMilliVolts(BAT_ADC); delay(2); }
  digitalWrite(BAT_EN, LOW);
  return (int)(sum / 8) * 2;
}

int batPercent(int mv) {
  int p = (mv - 3300) * 100 / (4150 - 3300);
  return p < 0 ? 0 : (p > 100 ? 100 : p);
}

// ---------------------------------------------------------------- util teks
String jsonEsc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

String sanitizeName(String n) {
  int s = max(n.lastIndexOf('/'), n.lastIndexOf('\\'));
  if (s >= 0) n = n.substring(s + 1);
  n.toLowerCase();
  if (n.endsWith(".gif")) n = n.substring(0, n.length() - 4);
  String o;
  for (size_t i = 0; i < n.length() && o.length() < 24; i++) {
    char c = n[i];
    o += ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '_';
  }
  if (o.length() == 0) o = "gif";
  return o + ".gif";
}

bool validName(const String &n) {
  return n.length() > 4 && n.length() < 40 && n.indexOf('/') < 0 && n.indexOf('\\') < 0 && n.indexOf("..") < 0 && n.endsWith(".gif");
}

String currentIp() {
  if (apMode) return WiFi.softAPIP().toString();
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return "tidak tersambung";
}

// --------------------------------------------------------------- tampilan teks
void drawMessage(const String &l1, const String &l2 = "") {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(l1, DISP_W / 2, DISP_H / 2 - (l2.length() ? 14 : 0), 4);
  if (l2.length()) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(l2, DISP_W / 2, DISP_H / 2 + 18, 2);
  }
  tft.setTextDatum(TL_DATUM);
}

void drawStatus() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("TTGO GIF", 8, 4, 4);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (apMode) tft.drawString(String("AP: ") + AP_SSID + "  sandi " + AP_PASS, 8, 34, 2);
  else {
    String s = staSsid;
    if (s.length() > 24) s = s.substring(0, 24);
    tft.drawString("WiFi: " + s, 8, 34, 2);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(currentIp(), 8, 52, 4);

  if (!apMode && WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String("http://") + HOSTNAME + ".local", 8, 82, 2);
  } else if (apMode) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("buka http://192.168.4.1", 8, 82, 2);
  }

  int mv = readBatteryMv();
  String foot = "GIF ";
  if (gifs.empty()) foot += "0";
  else { foot += String(cur + 1); foot += "/"; foot += String((int)gifs.size()); }
  if (mv > BAT_VALID_MV) {
    foot += "   Bat "; foot += String(mv / 1000.0, 2); foot += "V "; foot += String(batPercent(mv)); foot += "%";
  } else foot += "   USB";
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(foot, 8, 104, 2);

  if (gifs.empty()) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Belum ada GIF, upload lewat web", 8, 119, 2);
  }
}

// ------------------------------------------------------------- GIF: file & draw
static File gifFile;

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) { *pSize = gifFile.size(); return (void *)&gifFile; }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // membaca sampai byte terakhir membuat seek() berhenti bekerja pada beberapa FS
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// Dipanggil per baris. Piksel transparan dilewati (frame delta dari web UI).
void GIFDraw(GIFDRAW *pDraw) {
  static uint16_t line[DISP_W];
  int y = gOffY + pDraw->iY + pDraw->y;
  if (y < 0 || y >= DISP_H) return;
  int x0 = gOffX + pDraw->iX;
  int w = pDraw->iWidth;
  int skip = 0;
  if (x0 < 0) { skip = -x0; w -= skip; x0 = 0; }
  if (x0 + w > DISP_W) w = DISP_W - x0;
  if (w < 1) return;

  uint8_t *s = pDraw->pPixels + skip;
  uint16_t *pal = pDraw->pPalette;
  bool hasT = pDraw->ucHasTransparency;
  uint8_t t = pDraw->ucTransparent;

  if (pDraw->ucDisposalMethod == 2 && hasT) {   // kembalikan ke warna latar
    for (int i = 0; i < w; i++) if (s[i] == t) s[i] = pDraw->ucBackground;
    hasT = false;
  }

  if (hasT) {
    int i = 0;
    while (i < w) {
      while (i < w && s[i] == t) i++;
      int start = i;
      while (i < w && s[i] != t) { line[i - start] = pal[s[i]]; i++; }
      int n = i - start;
      if (n > 0) {
        tft.setAddrWindow(x0 + start, y, n, 1);
        tft.pushPixels(line, n);
      }
    }
  } else {
    for (int i = 0; i < w; i++) line[i] = pal[s[i]];
    tft.setAddrWindow(x0, y, w, 1);
    tft.pushPixels(line, w);
  }
}

// ---------------------------------------------------------------- daftar GIF
void scanGifs() {
  gifs.clear();
  File dir = LittleFS.open(GIF_DIR);
  if (!dir || !dir.isDirectory()) return;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String n = f.name();
      int sl = n.lastIndexOf('/');
      if (sl >= 0) n = n.substring(sl + 1);
      String l = n; l.toLowerCase();
      if (l.endsWith(".gif")) gifs.push_back({n, (uint32_t)f.size()});
    }
    f = dir.openNextFile();
  }
  std::sort(gifs.begin(), gifs.end(), [](const GifEntry &a, const GifEntry &b) { return a.name < b.name; });
}

void closeGif() {
  if (gifOpen) { gif.close(); gifOpen = false; }
}

void enterEmpty() {
  closeGif();
  mode = M_EMPTY;
  drawStatus();
}

bool openGif(int idx) {
  closeGif();
  if (gifs.empty()) { enterEmpty(); return false; }
  int n = gifs.size();
  cur = ((idx % n) + n) % n;
  String path = String(GIF_DIR) + "/" + gifs[cur].name;
  tft.fillScreen(TFT_BLACK);
  if (!gif.open(path.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    drawMessage("GIF rusak", gifs[cur].name);
    mode = M_PLAYING;       // klik tombol bawah untuk lanjut
    return false;
  }
  gOffX = (DISP_W - gif.getCanvasWidth()) / 2;
  gOffY = (DISP_H - gif.getCanvasHeight()) / 2;
  frameCount = 0;
  staticGif = false;
  nextFrameAt = 0;
  gifOpen = true;
  mode = M_PLAYING;
  return true;
}

void playerTick() {
  if (mode != M_PLAYING || !gifOpen || staticGif) return;
  uint32_t now = millis();
  if ((int32_t)(now - nextFrameAt) < 0) return;
  int d = 0;
  tft.startWrite();
  int r = gif.playFrame(false, &d);
  tft.endWrite();
  frameCount++;
  if (d < 20) d = 20;
  nextFrameAt = now + d;
  if (r == 0) {                       // frame terakhir
    if (frameCount == 1) staticGif = true;   // GIF 1 frame: cukup digambar sekali
    else { gif.reset(); frameCount = 0; }
  } else if (r < 0) {                 // error decode: tampilkan pesan, tunggu klik
    closeGif();
    drawMessage("Gagal memutar", gifs[cur].name);
  }
}

// ------------------------------------------------------------------ power off
void powerOff() {
  closeGif();
  while (digitalRead(BTN_TOP) == LOW) delay(10);   // tunggu tombol dilepas
  delay(80);
  digitalWrite(TFT_BL, LOW);
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
  delay(120);
  WiFi.mode(WIFI_OFF);
  LittleFS.end();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);    // bangun cukup dengan satu kali tekan
  esp_deep_sleep_start();
}

// ---------------------------------------------------------------------- tombol
struct Btn { uint8_t pin; bool raw, stable; uint32_t t; };
Btn bTop = {BTN_TOP, false, false, 0}, bBot = {BTN_BOTTOM, false, false, 0};
uint32_t lastTopClick = 0;

void btnInit(Btn &b) {
  b.raw = b.stable = (digitalRead(b.pin) == LOW);   // baca ulang: klik pembangun tidak dihitung
  b.t = millis();
}

bool btnPressed(Btn &b) {                            // true sekali saat penekanan (debounce 25 ms)
  bool r = (digitalRead(b.pin) == LOW);
  uint32_t now = millis();
  if (r != b.raw) { b.raw = r; b.t = now; }
  if ((now - b.t) > 25 && b.stable != b.raw) {
    b.stable = b.raw;
    if (b.stable) return true;
  }
  return false;
}

void showInfo() {
  mode = M_INFO;
  infoUntil = millis() + INFO_MS;
  drawStatus();
}

void leaveInfo() {
  if (gifs.empty()) enterEmpty();
  else openGif(cur);
}

void handleButtons() {
  if (mode == M_UPLOAD) return;
  uint32_t now = millis();

  if (btnPressed(bTop)) {
    if (lastTopClick && (now - lastTopClick) < DOUBLE_CLICK_MS) {
      powerOff();                                    // dobel klik, dideteksi saat penekanan kedua
    } else {
      lastTopClick = now;                            // klik pertama langsung beraksi
      if (mode == M_INFO && !gifs.empty()) leaveInfo();
      else showInfo();
    }
  }

  if (btnPressed(bBot)) {
    if (gifs.empty()) showInfo();
    else openGif(cur + 1);
  }

  if (mode == M_INFO && infoUntil && (int32_t)(millis() - infoUntil) > 0) leaveInfo();
}

// ------------------------------------------------------------------- web server
void handleInfoApi() {
  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  String j = "{\"files\":[";
  for (size_t i = 0; i < gifs.size(); i++) {
    if (i) j += ',';
    j += "{\"n\":\"" + gifs[i].name + "\",\"s\":" + gifs[i].size + "}";
  }
  j += "],\"total\":" + String((uint32_t)total) + ",\"used\":" + String((uint32_t)used);
  j += ",\"cur\":" + String(cur);
  j += ",\"ip\":\"" + currentIp() + "\"";
  j += ",\"ssid\":\"" + jsonEsc(apMode ? String(AP_SSID) : staSsid) + "\"";
  j += ",\"ap\":" + String(apMode ? "true" : "false");
  j += ",\"bat\":" + String(readBatteryMv());
  j += ",\"w\":" + String(DISP_W) + ",\"h\":" + String(DISP_H) + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleUploadData() {
  HTTPUpload &up = server.upload();
  uploadSeen = millis();
  if (up.status == UPLOAD_FILE_START) {
    uploadOk = false;
    uploadErr = "";
    uploadName = sanitizeName(up.filename);
    closeGif();
    mode = M_UPLOAD;
    drawMessage("Menerima GIF...", uploadName);
    String path = String(GIF_DIR) + "/" + uploadName;
    if (LittleFS.exists(path)) LittleFS.remove(path);
    uploadFile = LittleFS.open(path, "w");
    if (uploadFile) uploadOk = true;
    else uploadErr = "Gagal membuat file";
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && uploadFile) {
      if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
        uploadOk = false;
        uploadErr = "Penyimpanan penuh";
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    if (uploadOk) {
      File chk = LittleFS.open(String(GIF_DIR) + "/" + uploadName, "r");
      if (!chk || (size_t)chk.size() != up.totalSize) { uploadOk = false; uploadErr = "Ukuran file tidak cocok"; }
      if (chk) chk.close();
    }
    if (!uploadOk) LittleFS.remove(String(GIF_DIR) + "/" + uploadName);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    LittleFS.remove(String(GIF_DIR) + "/" + uploadName);
    uploadOk = false;
    uploadErr = "Upload dibatalkan";
  }
}

void handleUploadDone() {
  pendingName = uploadOk ? uploadName : "";
  pendingRescan = true;                  // pindah ke GIF baru di loop utama
  if (uploadOk) server.send(200, "application/json", "{\"ok\":true}");
  else server.send(500, "application/json", "{\"ok\":false,\"err\":\"" + jsonEsc(uploadErr.length() ? uploadErr : String("Upload gagal")) + "\"}");
}

void handleFileApi() {
  String n = server.arg("name");
  if (!validName(n)) { server.send(400, "text/plain", "nama tidak valid"); return; }
  File f = LittleFS.open(String(GIF_DIR) + "/" + n, "r");
  if (!f) { server.send(404, "text/plain", "tidak ada"); return; }
  server.sendHeader("Cache-Control", "max-age=300");
  server.streamFile(f, "image/gif");
  f.close();
}

void handleDeleteApi() {
  String n = server.arg("name");
  if (!validName(n)) { server.send(400, "text/plain", "nama tidak valid"); return; }
  if (cur < (int)gifs.size() && gifs[cur].name == n) closeGif();
  LittleFS.remove(String(GIF_DIR) + "/" + n);
  pendingName = "";
  pendingRescan = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleShowApi() {
  String n = server.arg("name");
  if (!validName(n)) { server.send(400, "text/plain", "nama tidak valid"); return; }
  pendingName = n;
  pendingRescan = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  std::vector<String> seen;
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0 || std::find(seen.begin(), seen.end(), s) != seen.end()) continue;
    seen.push_back(s);
    if (!first) j += ',';
    first = false;
    j += "{\"s\":\"" + jsonEsc(s) + "\",\"r\":" + String(WiFi.RSSI(i)) + ",\"o\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  j += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleWifiSave() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (ssid.length() == 0) { server.send(400, "text/plain", "ssid kosong"); return; }
  prefs.begin("net", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  server.send(200, "application/json", "{\"ok\":true}");
  restartAt = millis() + 800;
}

void handleWifiReset() {
  prefs.begin("net", false);
  prefs.clear();
  prefs.end();
  server.send(200, "application/json", "{\"ok\":true}");
  restartAt = millis() + 800;
}

void setupServer() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", WEBUI_HTML); });
  server.on("/wifi", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", WIFI_HTML); });
  server.on("/api/info", HTTP_GET, handleInfoApi);
  server.on("/api/file", HTTP_GET, handleFileApi);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/api/delete", HTTP_POST, handleDeleteApi);
  server.on("/api/show", HTTP_POST, handleShowApi);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
  server.onNotFound([]() {
    if (apMode) {                                    // captive portal
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else server.send(404, "text/plain", "tidak ditemukan");
  });
  server.begin();
}

// ------------------------------------------------------------------------ WiFi
void startAp() {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);                            // STA ikut aktif agar bisa scan jaringan
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  apMode = true;
}

void startNetwork() {
  prefs.begin("net", true);
  staSsid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);

  if (staSsid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSsid.c_str(), pass.c_str());
    drawMessage("Menyambung WiFi...", staSsid + "  (tekan bawah = lewati)");
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TRY_MS) {
      delay(100);
      if (digitalRead(BTN_BOTTOM) == LOW) break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    WiFi.setAutoReconnect(true);
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  } else {
    startAp();
  }
}

// ------------------------------------------------------------------------ loop
void handlePending() {
  if (mode == M_UPLOAD && !pendingRescan && (millis() - uploadSeen) > 8000) {   // upload terputus
    if (uploadFile) uploadFile.close();
    pendingName = "";
    pendingRescan = true;
  }
  if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
  if (!pendingRescan) return;
  pendingRescan = false;
  String keep = pendingName;
  pendingName = "";
  scanGifs();
  if (gifs.empty()) { enterEmpty(); return; }
  int idx = cur;
  if (keep.length()) {
    for (size_t i = 0; i < gifs.size(); i++) if (gifs[i].name == keep) { idx = i; break; }
  }
  if (idx >= (int)gifs.size()) idx = 0;
  openGif(idx);
}

void setup() {
  pinMode(BTN_TOP, INPUT);              // GPIO35 input-only, pull-up eksternal di board
  pinMode(BTN_BOTTOM, INPUT_PULLUP);
  pinMode(BAT_EN, OUTPUT);
  digitalWrite(BAT_EN, LOW);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(ROTATION);
  tft.fillScreen(TFT_BLACK);
#if defined(BIG_ENDIAN_PIXELS)
  gif.begin(BIG_ENDIAN_PIXELS);
#else
  gif.begin(GIF_PALETTE_RGB565_BE);
#endif

  drawMessage("TTGO GIF", "memulai...");
  if (!LittleFS.begin(true)) drawMessage("Gagal mount LittleFS");
  LittleFS.mkdir(GIF_DIR);

  startNetwork();
  setupServer();
  scanGifs();

  btnInit(bTop);
  btnInit(bBot);
  lastTopClick = 0;

  showInfo();                            // tampilkan IP beberapa detik
  if (gifs.empty()) { mode = M_EMPTY; }
}

void loop() {
  server.handleClient();
  if (apMode) dns.processNextRequest();
  handleButtons();
  handlePending();
  playerTick();
  delay(1);
}
