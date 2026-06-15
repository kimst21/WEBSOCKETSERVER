/*
 * ESP32 WebSocket LED 토글 서버  (Arduino-ESP32 코어 3.x 기준)
 *
 * 필요 라이브러리 — 라이브러리 매니저에서 ESP32Async 버전으로 설치:
 *   - "Async TCP"            by ESP32Async
 *   - "ESP Async WebServer"  by ESP32Async
 *   (옛 me-no-dev 버전은 코어 3.x에서 컴파일 오류가 날 수 있음)
 */

#include <WiFi.h>               // WiFi 기능
#include <AsyncTCP.h>           // 비동기 TCP (ESP32Async)
#include <ESPAsyncWebServer.h>  // 비동기 웹서버 + 웹소켓 (ESP32Async)

// WiFi 설정 (본인 네트워크 정보 입력)
const char* ssid = "----";          // WiFi SSID
const char* password = "----";      // WiFi 비밀번호

// LED 상태 및 핀
bool ledState = 0;              // 0: OFF, 1: ON
const int ledPin = 35;          // LED가 연결된 GPIO

// 웹서버 + 웹소켓 객체
AsyncWebServer server(80);      // 80번 포트 웹서버
AsyncWebSocket ws("/ws");       // 웹소켓 경로 "/ws"

// ===== 웹 페이지 =============================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html lang="ko">
<head>
  <meta charset="UTF-8">
  <title>Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html{font-family:Arial,Helvetica,sans-serif;text-align:center;}
    h1{font-size:1.8rem;color:#fff;}
    h2{font-size:1.5rem;font-weight:bold;color:#143642;}
    .topnav{overflow:hidden;background-color:#143642;}
    body{margin:0;}
    .content{padding:30px;max-width:600px;margin:0 auto;}
    .card{background-color:#F8F7F9;box-shadow:2px 2px 12px 1px rgba(140,140,140,.5);
      padding-top:10px;padding-bottom:20px;}
    .button{padding:15px 50px;font-size:24px;text-align:center;outline:none;color:#fff;
      background-color:#0f8b8d;border:none;border-radius:5px;user-select:none;}
    .button:active{background-color:#0f8b8d;box-shadow:2px 2px #CDCDCD;transform:translateY(2px);}
    .state{font-size:1.5rem;color:#8c8c8c;font-weight:bold;}
  </style>
</head>
<body>
  <div class="topnav"><h1>WebSocket Server</h1></div>
  <div class="content">
    <div class="card">
      <h2>Output - GPIO 35</h2>
      <p class="state">state: <span id="state">%STATE%</span></p>
      <p><button id="button" class="button">Toggle</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);

  function initWebSocket(){
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function onOpen(event){ console.log('Connection opened'); }
  function onClose(event){ console.log('Connection closed'); setTimeout(initWebSocket, 2000); }
  function onMessage(event){
    document.getElementById('state').innerHTML = (event.data == "1") ? "ON" : "OFF";
  }
  function onLoad(event){ initWebSocket(); initButton(); }
  function initButton(){ document.getElementById('button').addEventListener('click', toggle); }
  function toggle(){ websocket.send('toggle'); }
</script>
</body></html>
)rawliteral";

// ===== 모든 웹소켓 클라이언트에 LED 상태 전송 ===============================
void notifyClients() {
  ws.textAll(String(ledState));
}

// ===== 웹소켓 메시지 처리 (클라이언트 → 서버) ==============================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;                                   // 문자열 끝 표시
    if (strcmp((char*)data, "toggle") == 0) {        // "toggle" 수신 시
      ledState = !ledState;                          // 상태 반전
      digitalWrite(ledPin, ledState);                // LED 즉시 반영
      notifyClients();                               // 모든 클라이언트에 알림
    }
  }
}

// ===== 웹소켓 이벤트 핸들러 =================================================
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n",
                    client->id(), client->remoteIP().toString().c_str());
      client->text(String(ledState));               // 접속 직후 현재 상태 전송
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);          // 이벤트 콜백 등록
  server.addHandler(&ws);       // 서버에 웹소켓 핸들러 추가
}

// ===== HTML 변수 치환 (%STATE% → ON/OFF) ===================================
String processor(const String& var){
  if (var == "STATE") return ledState ? "ON" : "OFF";
  return String();
}

void setup(){
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);    // 초기 OFF

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {   // 연결 대기
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());           // 이 IP를 브라우저에 입력

  initWebSocket();              // 웹소켓 초기화

  // 루트("/") 요청 → 페이지 제공 (send_P 대신 현재 API send 사용)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html, processor);
  });

  server.begin();               // 웹서버 시작
}

void loop() {
  ws.cleanupClients();          // 끊어진 웹소켓 클라이언트 정리
}
