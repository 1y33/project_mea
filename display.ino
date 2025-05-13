#include <SPFD5408_Adafruit_GFX.h>    // Core graphics library  
#include <SPFD5408_Adafruit_TFTLCD.h> // Hardware-specific library  
#include <SPFD5408_TouchScreen.h>  
#include <SPI.h>  
#include <SD.h>  

#define LCD_RD    A0  
#define LCD_WR    A1  
#define LCD_CD    A2  
#define LCD_CS    A3  
#define LCD_RESET A4  
#define SD_CS     10  

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

uint8_t  currentAd    = 0;
uint32_t lastAdTime   = 0;
const uint32_t adInterval = 10000; // 10 seconds

char  recvBuf[64];
uint8_t recvPos       = 0;

void listDir(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    for (int i = 0; i < numTabs; i++) Serial.print('\t');
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listDir(entry, numTabs + 1);
    } else {
      Serial.print("\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);

  // Ensure SPI pins are inputs when not in use
  pinMode(MOSI, INPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK,  INPUT);

  SPI.begin();

  tft.reset();
  tft.begin(0x9341);
  tft.setRotation(3);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD.init failed!");
    while (1);
  }
  //File root = SD.open("/");
  //listDir(root, 0);
  //root.close();

  lastAdTime = millis();
  showNextAd();
}

void loop() {
  handleSerial();
  if (millis() - lastAdTime >= adInterval) {
    showNextAd();
    lastAdTime = millis();
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || recvPos >= sizeof(recvBuf) - 1) {
      recvBuf[recvPos] = '\0';
      processMessage(recvBuf);
      recvPos = 0;
    } else if (c != '\r') {
      recvBuf[recvPos++] = c;
    }
  }
}

void processMessage(const char* msg) {
  Serial.print("Msg: ");
  Serial.println(msg);

  bool recognized = false;

  // Wrap C-string into Arduino String for easy parsing
  String s = String(msg);
  s.trim();  // remove leading/trailing whitespace

  if (s.startsWith("result ")) {
    // Remove "result " prefix
    String args = s.substring(7); 
    int sep = args.indexOf(' ');
    if (sep > 0) {
      // Split into two numeric substrings
      String tStr = args.substring(0, sep);
      String pStr = args.substring(sep + 1);

      float t = tStr.toFloat();
      float p = pStr.toFloat();

      displayResult(t, p);
      recognized = true;
      delay(5000);
    }
  }
  else if (s == "CMD_welcome") {
    bmpDraw("CHECK.BMP", 0, 0);
    recognized = true;
    delay(5000);
  }

  if (recognized) {
    // Clear screen and reset ad timer
    tft.fillScreen(0x0000);
    lastAdTime = millis();
    showNextAd();
  }
}

void showNextAd() {
  switch (currentAd) {
    case 0: bmpDraw("PREC.BMP", 0, 0); break;
    case 2: bmpDraw("MC.BMP",   0, 0); break;
  }
  currentAd = (currentAd + 1) % 3;
}

void displayResult(float timeVal, float priceVal) {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(3);
  tft.setCursor(10, 50);
  tft.print("Time: ");  tft.print(timeVal, 2);
  tft.setCursor(10, 100);
  tft.print("Price: "); tft.print(priceVal, 2);
  tft.setCursor(100, 180);
  tft.setTextSize(2);
  tft.print("Drum bun!");
}

#define BUFFPIXEL 20

void bmpDraw(const char *filename, int x, int y) {
  if (!SD.exists(filename)) return;
  File bmpFile = SD.open(filename);
  uint16_t sig = read16(bmpFile);
  if (sig != 0x4D42) { bmpFile.close(); return; }

  (void)read32(bmpFile);
  (void)read32(bmpFile);
  uint32_t offset = read32(bmpFile);
  (void)read32(bmpFile);
  int32_t w = read32(bmpFile);
  int32_t h = read32(bmpFile);
  read16(bmpFile);
  read16(bmpFile);
  read32(bmpFile);

  if (h < 0) h = -h;
  uint32_t rowSize = (w * 3 + 3) & ~3;
  int16_t dw = min((int)w, tft.width()  - x);
  int16_t dh = min((int)h, tft.height() - y);

  tft.setAddrWindow(x, y, x + dw - 1, y + dh - 1);

  uint8_t  sdbuffer[3 * BUFFPIXEL];
  uint16_t lcdbuffer[BUFFPIXEL];
  uint8_t  buffidx = sizeof(sdbuffer), lcdidx = 0;
  boolean  first = true;

  for (int row = 0; row < dh; row++) {
    uint32_t pos = offset + (h - 1 - row) * rowSize;
    if (bmpFile.position() != pos) {
      bmpFile.seek(pos);
      buffidx = sizeof(sdbuffer);
    }
    for (int col = 0; col < dw; col++) {
      if (buffidx >= sizeof(sdbuffer)) {
        if (lcdidx) {
          tft.pushColors(lcdbuffer, lcdidx, first);
          lcdidx = 0;
          first = false;
        }
        bmpFile.read(sdbuffer, sizeof(sdbuffer));
        buffidx = 0;
      }
      uint8_t b = sdbuffer[buffidx++];
      uint8_t g = sdbuffer[buffidx++];
      uint8_t r = sdbuffer[buffidx++];
      lcdbuffer[lcdidx++] = tft.color565(r, g, b);
    }
  }
  if (lcdidx) tft.pushColors(lcdbuffer, lcdidx, first);
  bmpFile.close();
}

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}
