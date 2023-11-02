
/* THƯ VIỆN CẦN CÀI ĐẶT
 *1, https://github.com/Links2004/arduinoWebSockets 
 *=> Tải zip về. Vào Arduino IDE => Tab Sketch  => Include Library => Add .ZIP Libray
 *
 *2, https://github.com/me-no-dev/ESPAsyncTCP
 *=> Tải zip về. Vào Arduino IDE => Tab Sketch  => Include Library => Add .ZIP Libray
 *Đối với thư viện 2 có thể cài trực tiếp từ Arduino IDE
 *=> Tab Tools => Manage Libraries => tìm ESPAsyncTCP và Install
 *
 *3, Thư viện ESP32Servo
 *=> Tab Tools => Manage Libraries => tìm ESP32Servo và Install (install cả các thư viện 
 được IDE recommend như PWM)
*/

/***************************************************************************************
 * TikTok: @Hieup25
 * Ngoài ra, môi trường lập trình ESP32CAM, xem lại phần 1 của seri
 ***************************************************************************************/
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>

#define FRAMESIZE_DEFAULT FRAMESIZE_SVGA

const char* ssid = "TP-Link_0D5E";
const char* password = "93853767";

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define S_T1_PIN  12 // not use
#define S_T2_PIN 13 // not use
#define SPAN_PIN  14
#define STILT_PIN 15

Servo servo_T1; // not use
Servo servo_T2; // not use
Servo sPan;
Servo sTilt;

/*  Tạo 1 WebSocket(ws) Server có cổng là 81
 *  WS Server này dùng để truyền hình ảnh từ ESP32Cam đến các client
 *  Mỗi 1 ws Server phục vụ tối đa 5 client 
 *  Có thể sửa số lượng client trong thư viện, tuy nhiên không khuyến khích
 *  Client có thể là PC, Laptop, Phone, trình duyệt...
*/
WebSocketsServer wsStream = WebSocketsServer(81);

/* Biến lưu số lượng client đang kết nối đến WS Server Stream 
 * Bài toán: Mỗi 1 thiết bị (client) kết nối đến WS Server và được server tự động
    đánh ID là 1 số nguyên ví dụ là 1, lúc này server sẽ chỉ gửi data thiết bị có ID là 1.
    Nếu 1 thiết bị khác cũng kết nối đến server này nhưng ID nó lại là 2 => Thiết bị ID là 2
    không được truyền data => không xem được stream.
 * Giải quyết: Cấu hình server cũng truyền data cho client có ID là 2
 * Sử dụng biến numClientStream để biết số lượng client đã kết nối đến. Khi gọi hàm senBIN(id, ...)
   sẽ gửi tới 2 id là 1 và 2
*/
uint32_t numClientStream = 0;

/*  Tạo 1 WebSocket(ws) Server có cổng là 82
 *  WS Server này dùng để điều khiển Servo Pan Tilt, data gửi từ WS Client đến WS Server
 *  Mỗi 1 ws Server phục vụ tối đa 5 client 
 *  Có thể sửa số lượng client trong thư viện, tuy nhiên không khuyến khích
 *  Client có thể là PC, Laptop, Phone, trình duyệt...
*/
WebSocketsServer wsPanTilt = WebSocketsServer(82);
uint32_t numClientStream = 0;

httpd_handle_t stream_httpd = NULL;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);

      // send message to client
      // webSocket.sendTXT(num, "message here");

      // send data to all connected clients
      // webSocket.broadcastTXT("message here");
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\n", num, length);
      // send message to client
      // webSocket.sendBIN(num, payload, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }

}
void setup() {
  Serial.begin(115200);
  // Setup Servo
  sPan.setPeriodHertz(50);  // Standard 50hz servo
  sTilt.setPeriodHertz(50); // Standard 50hz servo
  //  servo_T1.attach(S_T1_PIN, 1000, 2000);
  //  servo_T2.attach(S_T2_PIN, 1000, 2000);
  sPan.attach(SPAN_PIN, 1000, 2000);
  sTilt.attach(STILT_PIN, 1000, 2000);
  // Setup Camera
  camera_config_t config;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV2640_PID) {
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  s->set_framesize(s, FRAMESIZE_DEFAULT);

  // Setup WebServer
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Camera at address: http://");
  Serial.println(WiFi.localIP());

  // Setup handle
  httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&stream_httpd, &httpd_config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
  //
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t index_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        // 80% là chất lượng ảnh
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}
void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    ESP.restart();
  }
  webSocket.sendBIN(0, fb->buf, fb->len);
  webSocket.sendBIN(1, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  webSocket.loop();
  delay(1);
}
