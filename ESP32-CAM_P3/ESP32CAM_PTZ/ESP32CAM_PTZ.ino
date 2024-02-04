/* THƯ VIỆN CẦN CÀI ĐẶT
  1, https://github.com/Links2004/arduinoWebSockets
  => Tải zip về. Vào Arduino IDE => Tab Sketch  => Include Library => Add .ZIP Libray

  2, https://github.com/me-no-dev/ESPAsyncTCP
  => Tải zip về. Vào Arduino IDE => Tab Sketch  => Include Library => Add .ZIP Libray
  Đối với thư viện 2 có thể cài trực tiếp từ Arduino IDE
  => Tab Tools => Manage Libraries => tìm ESPAsyncTCP và Install

  3, Thư viện ESP32Servo
  => Tab Tools => Manage Libraries => tìm ESP32Servo và Install (install cả các thư viện
  được IDE recommend như PWM)

  4, Thư viện ArduinoJson
  => Tab Tools => Manage Libraries => tìm ArduinoJson và Install
*/

/***************************************************************************************
   TikTok: @Hieup25
   Ngoài ra, môi trường lập trình ESP32CAM, xem lại phần 1 của seri
 ***************************************************************************************/
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "web.h"
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define LED_PIN   4
#define LED_BRIGHTNESS 30 // 0 -> 255, không lên quá cao vì LED rất nóng
#define SPAN_PIN  14
#define STILT_PIN 15
/* Mặc định vị trí Pan ban đầu là góc 90 độ,
   Pan có khoảng từ 0 -> 180 */
#define POS_PAN_DEFAULT 90
#define POS_PAN_MIN     0
#define POS_PAN_MAX     180
/* Mặc định vị trí Tilt ban đầu là góc 45 độ,
   Pan có khoảng từ 0 -> 90 */
#define POS_TILT_DEFAULT  45
#define POS_TILT_MIN      0
#define POS_TILT_MAX      90

typedef struct current_camera_t {
  /* Biến lưu vị trí hiện tại của Pan và Tilt */
  int   pos_Pan = POS_PAN_DEFAULT;
  int   pos_Tilt = POS_TILT_DEFAULT;
  /* Biến lưu trạng thái (ON/OFF) hiện tại của LED*/
  bool  cur_status_LED = false;
  /* Biến lưu giá trị frame size mặc định */
  uint32_t width = 800; // px
  uint32_t height = 600; //px

} current_camera_t;
current_camera_t cur_camera;

const char* ssid = "TP-Link_0D5E";
const char* password = "93853767";
Servo sTilt1;
Servo sTilt2;
Servo sPan;
Servo sTilt;
/*  Tạo 1 WebSocket(ws) Server có cổng là 81
    WS Server này dùng để truyền hình ảnh từ ESP32Cam đến các client
    và điều khiển Pan Tilt
    Mỗi 1 ws Server phục vụ tối đa 5 client
    Có thể sửa số lượng client trong thư viện, tuy nhiên không khuyến khích
    Client có thể là PC, Laptop, Phone, trình duyệt...
*/
WebSocketsServer wsStream = WebSocketsServer(81);
httpd_handle_t stream_httpd = NULL;
int num_client = 0;
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  /* Setup Servo */
  sPan.setPeriodHertz(50);  // Standard 50hz servo
  sTilt.setPeriodHertz(50); // Standard 50hz servo
  sPan.attach(SPAN_PIN);
  sTilt.attach(STILT_PIN);
  sPan.write(cur_camera.pos_Pan);
  sTilt.write(cur_camera.pos_Tilt);
  /* Setup Config Camera */
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
    //    config.frame_size = FRAMESIZE_SXGA;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    //    cur_camera.width = 1280;
    //    cur_camera.height = 1024;
    cur_camera.width = 800;
    cur_camera.height = 600;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    cur_camera.width = 800;
    cur_camera.height = 600;
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
  // Setup WebServer
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
  wsStream.begin();
  wsStream.onEvent(WebSocketEvent);
}
static esp_err_t index_handler(httpd_req_t *req) {
  return httpd_resp_send(req, htmlHomePage, strlen(htmlHomePage));;
}
void sendStream() {
  if (!num_client) {// num_client = 0
    delay(500);
    return;
  }
  /* Lấy hình ảnh từ camera */
  camera_fb_t *fb = esp_camera_fb_get();
  /* Nếu không lấy được hình ảnh do bất kỳ lỗi gì
     Khởi động lại ESP32-CAM
  */
  if (!fb) {
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    ESP.restart();
  }
  /* Gửi data ảnh cho tất cả client */
  wsStream.broadcastBIN(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void loop() {
  sendStream();
  wsStream.loop();
}
/* Hàm callback xử lý các sự kiện khi client gửi data đến ws Server Stream
   Data này sẽ được phân ra theo type mà client gửi xuống
   Ví dụ client gửi data dạng text, binary, ...

   Ngoài ra, type còn bắt được các sự kiện connected và disconnected
   Tùy vào project mà xử lý bất cứ kịch bản gì ở đây

*/
void WebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      num_client--;
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        num_client++;
        IPAddress ip = wsStream.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        /* Gửi tất cả cấu hình hiện tại của camera đến web để đồng bộ giữa các client */
        String out;
        DynamicJsonDocument doc(100);
        doc["width"]  = cur_camera.width;
        doc["height"] = cur_camera.height;
        doc["pan"]    = cur_camera.pos_Pan;
        doc["tilt"]   = cur_camera.pos_Tilt;
        doc["led"]    = cur_camera.cur_status_LED;
        serializeJson(doc, out);
        wsStream.broadcastTXT(out);
        break;
      }
    case WStype_TEXT:
      {
        Serial.printf("[%u] get Text: %s\n", num, payload);
        DynamicJsonDocument doc(100);
        deserializeJson(doc, payload);
        /* Check bản tin data từ ws client có chứa các trường 'mode' và 'data' cần thiết không */
        if (doc.containsKey("mode") && doc.containsKey("data")) {
          const char* _mode = doc["mode"].as<const char*>();
          /*Check nếu là mode 'PT', thì check xem có chứa trường 'Pan' và 'Tilt' không*/
          if (strcmp(_mode, "PT") == 0 &&
              doc["data"].containsKey("Pan") &&
              doc["data"].containsKey("Tilt")) {
            /*Điều khiển Pan Tilt*/
            int pos_Pan = doc["data"]["Pan"].as<int>();
            int pos_Tilt = doc["data"]["Tilt"].as<int>();
            smoothRotateServo(pos_Pan, pos_Tilt);
          }
          /*Check nếu là mode 'LED', thì check xe, có chứa trường 'status' không*/
          else if (strcmp(_mode, "LED") == 0 &&
                   doc["data"].containsKey("status")) {
            /*_status = true => LED ON, _status = false => LED OFF*/
            bool _status = doc["data"]["status"].as<bool>();
            cur_camera.cur_status_LED = _status;
            analogWrite(LED_PIN, (_status) ? LED_BRIGHTNESS : 0);
          }
        }
        break;
      }
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

/* Biến lưu khoảng thời gian smooth để servo quay
    Càng thấp thì servo sẽ quay càng liên tục => điện áp
    tiêu thụ sẽ lớn => ESP có thể restart, do trình bảo vệ
    >> "Brownout detector was triggered"
*/
const uint32_t INTERVAL_SMOOTH = 15;
static int pos_Pan_old = 0;
static int pos_Tilt_old = 0;
void smoothRotateServo(int pan, int tilt) {
  Serial.printf("pos_pan_old: %d, pos_tilt_old: %d\n", pos_Pan_old, pos_Tilt_old);
  delay(INTERVAL_SMOOTH);
  if (cur_camera.pos_Pan != pan && pan >= POS_PAN_MIN && pan <= POS_PAN_MAX)
    cur_camera.pos_Pan = pan;
  if (cur_camera.pos_Tilt != tilt && tilt >= POS_TILT_MIN && tilt <= POS_TILT_MAX)
    cur_camera.pos_Tilt = tilt;
  /*Check Pan*/
  if (pos_Pan_old < cur_camera.pos_Pan) {
    for (int i = pos_Pan_old; i <= cur_camera.pos_Pan; i++) {
      sPan.write(i);
      delay(INTERVAL_SMOOTH);
    }
  } else if (pos_Pan_old > cur_camera.pos_Pan) {
    for (int i = pos_Pan_old; i >= cur_camera.pos_Pan; i--) {
      sPan.write(i);
      delay(INTERVAL_SMOOTH);
    }
  }
  pos_Pan_old = cur_camera.pos_Pan;

  /* Check Tilt*/
  if (pos_Tilt_old < cur_camera.pos_Tilt) {
    for (int i = pos_Tilt_old; i <= cur_camera.pos_Tilt; i++) {
      sTilt.write(i);
      delay(INTERVAL_SMOOTH);
    }
  } else if (pos_Tilt_old > cur_camera.pos_Tilt) {
    for (int i = pos_Tilt_old; i >= cur_camera.pos_Tilt; i--) {
      sTilt.write(i);
      delay(INTERVAL_SMOOTH);
    }
  }
  pos_Tilt_old = cur_camera.pos_Tilt;
}
