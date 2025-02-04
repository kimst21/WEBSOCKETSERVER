#include <WiFi.h>  // WiFi 기능을 사용하기 위한 라이브러리
#include <AsyncTCP.h>  // 비동기 TCP 통신을 위한 라이브러리
#include <ESPAsyncWebServer.h>  // 비동기 웹 서버 라이브러리

// WiFi 네트워크 정보 (사용자가 자신의 네트워크 정보를 입력해야 함)
const char* ssid = ""; // WiFi SSID
const char* password = ""; // WiFi 비밀번호

// LED 상태 및 핀 설정
bool ledState = 0; // LED 상태 저장 (0: OFF, 1: ON)
const int ledPin = 41; // 빨강 LED가 연결된 GPIO 핀

// 웹 서버 및 웹소켓(WebSocket) 객체 생성
AsyncWebServer server(80); // 포트 80에서 웹 서버 실행
AsyncWebSocket ws("/ws");  // 웹소켓 경로 "/ws" 설정

// 웹 인터페이스 HTML 코드 (ESP32의 플래시 메모리에서 직접 제공)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    user-select: none;
   }
   .button:active {
     background-color: #0f8b8d;
     box-shadow: 2px 2px #CDCDCD;
     transform: translateY(2px);
   }
   .state {
     font-size: 1.5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>WebSocket Server</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2>Output - GPIO 41</h2>
      <p class="state">state: <span id="state">%STATE%</span></p>
      <p><button id="button" class="button">Toggle</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);

  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var state;
    if (event.data == "1"){
      state = "ON";
    }
    else{
      state = "OFF";
    }
    document.getElementById('state').innerHTML = state;
  }
  function onLoad(event) {
    initWebSocket();
    initButton();
  }
  function initButton() {
    document.getElementById('button').addEventListener('click', toggle);
  }
  function toggle(){
    websocket.send('toggle');
  }
</script>
</body>
</html>
)rawliteral";

// 모든 웹소켓 클라이언트에게 LED 상태 전송
void notifyClients() {
  ws.textAll(String(ledState));
}

// 웹소켓 메시지 처리 함수 (클라이언트로부터 메시지를 수신할 때 실행됨)
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) { // "toggle" 명령 수신 시 LED 상태 변경
      ledState = !ledState;
      notifyClients(); // 모든 클라이언트에게 LED 상태 전송
    }
  }
}

// 웹소켓 이벤트 핸들러
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
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

// 웹소켓 초기화 함수
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// HTML 변수 처리 함수 (LED 상태 값 반환)
String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    return ledState ? "ON" : "OFF";
  }
  return String();
}

void setup(){
  Serial.begin(115200); // 디버깅용 시리얼 출력 시작

  pinMode(ledPin, OUTPUT); // LED 핀을 출력 모드로 설정
  digitalWrite(ledPin, LOW); // 초기 상태를 OFF로 설정
  
  // Wi-Fi 연결 설정
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { // WiFi가 연결될 때까지 대기
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP()); // 연결된 IP 주소 출력

  initWebSocket(); // 웹소켓 초기화

  // 루트 URL("/") 요청 시 웹 페이지 제공
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin(); // 웹 서버 시작
}

// 메인 루프 (웹소켓 클라이언트 정리 및 LED 제어)
void loop() {
  ws.cleanupClients(); // 끊어진 웹소켓 클라이언트 정리
  digitalWrite(ledPin, ledState); // LED 상태에 따라 GPIO 출력 조정
}
