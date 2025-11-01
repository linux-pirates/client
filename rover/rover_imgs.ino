#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ap_ssid     = "roverAP";
const char* ap_password = "12345678";

WebServer httpServer(8000);
WebSocketsServer wsServer(81);

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const int IN1 = 12;
const int IN2 = 13;
const int IN3 = 14;
const int IN4 = 15;

bool camera_ok = false;

// buffer for latest JPEG
uint8_t *latest_buf = nullptr;
size_t   latest_len = 0;
unsigned long last_capture_ms = 0;
const unsigned long CAPTURE_INTERVAL_MS = 3000;

unsigned long lastCmdMs = 0;
const unsigned long CMD_TIMEOUT_MS = 500;

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
void motorForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void motorBackward() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}
void motorLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void motorRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

void translateCMD(const String& cmd) {
  Serial.print("CMD: "); Serial.println(cmd);
  if (cmd == "S") motorStop();
  else if (cmd == "F") motorForward();
  else if (cmd == "B") motorBackward();
  else if (cmd == "L") motorLeft();
  else if (cmd == "R") motorRight();
  else if (cmd == "FL") {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (cmd == "FR") {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  } else {
    motorStop();
  }
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg;
    for (size_t i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();
    lastCmdMs = millis();
    translateCMD(msg);
    wsServer.sendTXT(num, "OK");
  }
}

void handleRoot() {
  httpServer.send(200, "text/plain", "rover ok, see /image");
}

void handleImage() {
  if (!camera_ok) {
    httpServer.send(500, "text/plain", "camera not ready");
    return;
  }
  if (latest_buf == nullptr || latest_len == 0) {
    httpServer.send(503, "text/plain", "no frame yet");
    return;
  }

  httpServer.setContentLength(latest_len);
  httpServer.send(200, "image/jpeg", "");
  WiFiClient client = httpServer.client();
  client.write(latest_buf, latest_len);
}

void do_capture_to_ram() {
  if (!camera_ok) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("capture failed");
    return;
  }

  // allocate or reallocate buffer
  if (latest_buf == nullptr || fb->len > latest_len) {
    // free old
    if (latest_buf != nullptr) {
      free(latest_buf);
      latest_buf = nullptr;
      latest_len = 0;
    }
    // allocate new (PSRAM if available)
    latest_buf = (uint8_t*) malloc(fb->len);
    if (!latest_buf) {
      Serial.println("malloc failed");
      esp_camera_fb_return(fb);
      return;
    }
  }

  memcpy(latest_buf, fb->buf, fb->len);
  latest_len = fb->len;

  esp_camera_fb_return(fb);

  Serial.printf("captured frame %u bytes\n", (unsigned)latest_len);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-CAM periodic capture");

  // disable brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // motors
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  motorStop();

  // camera init
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // to keep it responsive:
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x\n", err);
    camera_ok = false;
  } else {
    Serial.println("Camera OK");
    camera_ok = true;
  }

  // Wi-Fi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password, 6, 0, 4);
  WiFi.setSleep(false);   // keep AP alive
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // HTTP
  httpServer.on("/", handleRoot);
  httpServer.on("/image", handleImage);
  httpServer.begin();

  // WS
  wsServer.begin();
  wsServer.onEvent(onWsEvent);
}

void loop() {
  httpServer.handleClient();
  wsServer.loop();

  unsigned long now = millis();
  if (camera_ok && now - last_capture_ms > CAPTURE_INTERVAL_MS) {
    last_capture_ms = now;
    do_capture_to_ram();
  }

  if (millis() - lastCmdMs > CMD_TIMEOUT_MS) {
    motorStop();
  }

  delay(1);
}