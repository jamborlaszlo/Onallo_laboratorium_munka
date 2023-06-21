#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <AccelStepper.h>

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
const int fullLength = 2 * stepsPerRevolution + 42;
int value = 0;
bool blinked = false;
bool isStopped = false;
bool quick = false;
// ULN2003 Motor Driver Pins
#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4
#define UP 0
#define DOWN 1
#define STOP 2
#define NONE 3
#define AUTO25 4
#define AUTO50 5
#define AUTO75 6

int position = fullLength;
int direction = NONE;
double quickDirection = NONE;
int runningDirection = NONE;
int formerDTG;

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "HUWEI P smart";
const char* password = "12345678";

const int output = 2;

void blink(int pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(100);
  digitalWrite(pin, LOW);
  delay(100);
  return;
}

int calculatePercentage(double value, double percent)
{
  return (int) percent * value / 100;
}
int getPercentage(double value, double percentageBase)
{
  return (int) value / percentageBase * 100;
}

// initialize the stepper library
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head>
    <title>Electric shutter mover</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body {background-image: url("https://media.istockphoto.com/id/1248543652/vector/dreamy-smooth-abstract-green-background.jpg?s=612x612&w=0&k=20&c=ZOnZHfWOJryY4sGeliLbQeueF77svXg7iZjmE__GIoM="); background-repeat: no-repeat; background-size: 100% 1000%; font-family: Lucida Console; text-align: center; margin:0px auto; padding-top: 30px; /*background-color: #800040;*/}
      .button {
        padding: 20px 20px;
        font-size: 24px;
        text-align: center;
        outline: none;
        color: #d279a6;
        background-color: #660033;
        border: none;
        border-radius: 20px;
        box-shadow: 0 6px #003300;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }  
      .quickbutton {
        padding: 20px 20px;
        font-size: 24px;
        text-align: center;
        outline: none;
        color: #d279a6;
        background-color: #800040;
        border: none;
        border-radius: 20px;
        box-shadow: 0 6px #003300;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);        
      }
      .button:hover {background-color: #1f2e45}
      .button:active {
        background-color: #1f2e45;
        box-shadow: 0 4px #666;
        transform: translateY(2px);
      }
    </style>
  </head>
  <body>
    <h1>Move shutter</h1>
    <button class="button" onmousedown="toggleCheckbox('up');" ontouchstart="toggleCheckbox('up');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">UP</button>
    <button class="button" onmousedown="toggleCheckbox('down');" ontouchstart="toggleCheckbox('down');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">DOWN</button>
    <button class="button" onmousedown="toggleCheckbox('stop');" ontouchstart="toggleCheckbox('stop');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">STOP</button>
    <h2>Quick set</h2>
    <button class="button" onmousedown="toggleCheckbox('25');" ontouchstart="toggleCheckbox('25');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">25%</button>
    <button class="button" onmousedown="toggleCheckbox('50');" ontouchstart="toggleCheckbox('50');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">50%</button>
    <button class="button" onmousedown="toggleCheckbox('75');" ontouchstart="toggleCheckbox('75');" onmouseup="toggleCheckbox('none');" ontouchend="toggleCheckbox('none');">75%</button>
   <script>
   function toggleCheckbox(x) {
     var xhr = new XMLHttpRequest();
     xhr.open("GET", "/" + x, true);
     xhr.send();
    }
  </script>
  </body>
</html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

void setup() {
pinMode(D8, INPUT_PULLUP);
  // initialize the serial port
  Serial.begin(115200);
  
  // set the speed and acceleration
  stepper.setMaxSpeed(500);
  stepper.setAcceleration(1000);

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  else
    Serial.print("Conncted to Wifi.");
  Serial.println();
  Serial.print("ESP IP Address: http://");
  Serial.println(WiFi.localIP());
  
  pinMode(output, OUTPUT);
  digitalWrite(output, LOW);
  
  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Receive an HTTP GET request
  server.on("/down", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, HIGH);
    direction = DOWN;
    request->send(200, "text/plain", "ok");
  });

   // Receive an HTTP GET request
  server.on("/up", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, HIGH);
    direction = UP;
    request->send(200, "text/plain", "ok");
  });

   // Receive an HTTP GET request
  server.on("/stop", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, HIGH);
    direction = STOP;
    request->send(200, "text/plain", "ok");
  });

  // Receive an HTTP GET request
  server.on("/none", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, LOW);
    direction = NONE;
    request->send(200, "text/plain", "ok");
  });

    server.on("/25", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, LOW);
    direction = 25;
    quickDirection = 25.0;
    request->send(200, "text/plain", "ok");
  });

    server.on("/50", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, LOW);
    direction = 50;
    quickDirection = 50.0;
    request->send(200, "text/plain", "ok");
  });  

    server.on("/75", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //digitalWrite(output, LOW);
    direction = 75;
    quickDirection = 75.0;
    request->send(200, "text/plain", "ok");
  });  

  /*server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", index_html, processor);
  }
  );*/

  server.onNotFound(notFound);
  server.begin();
}

void loop() {
 if(stepper.distanceToGo() == 0 && !blinked)
  {
    blink(D6);
    blinked = true;
  }
  //value = digitalRead(D8);
  if(direction == DOWN)
  {
    blink(D5);
    stepper.move(-position);
    //position = LOW_POSITION;
    blinked = false;
    isStopped = false;
    quick = false;
    runningDirection = DOWN;
    delay(300);
  }
  //value = digitalRead(D8);
  else if(direction == UP)
  {
    blink(D7);
    stepper.move(fullLength - position);
    //position = HIGH_POSITION;
    blinked = false;
    isStopped = false;
    quick = false;
    runningDirection = UP;
    delay(300);
  }
  else if(direction == STOP)
  {
    blink(D6); blink(D6);
    stepper.stop();
    isStopped = true;
    quick = false;
  }

  else if(direction == 25 || direction == 50 || direction == 75)
  {
    blink(D7); blink(D7); blink(D7);
    quick = true;
    if(position <= (quickDirection / 100) * fullLength)
    {
      //Serial.print("UP");
      //Serial.println((quickDirection / 100));
      stepper.move(((quickDirection / 100) * fullLength) - position);
      //Serial.println(calculatePercentage((double)fullLength, (double)direction));
      blinked = false;
      isStopped = false;
      runningDirection = UP;
      delay(300);
    }
    if(position >= (quickDirection / 100) * fullLength)
    {
      //Serial.print("DOWN");
      //Serial.println(calculatePercentage((double)fullLength, (double)direction));
      stepper.move(- (position - (quickDirection / 100) * fullLength));
      //Serial.println(calculatePercentage((double)fullLength, (double)direction));
      blinked = false;
      isStopped = false;
      runningDirection = DOWN;
      delay(300);
  }
}

  if(!isStopped)
    stepper.run();
  if(stepper.distanceToGo() != formerDTG && !isStopped && runningDirection == DOWN)
  {
    if(quick)
    {
      position = -stepper.distanceToGo() + (quickDirection / 100) * fullLength;
      formerDTG = stepper.distanceToGo();
    }
    else
    {
      position = -stepper.distanceToGo();
      formerDTG = stepper.distanceToGo();
    }
  }
  if(stepper.distanceToGo() != formerDTG && !isStopped && runningDirection == UP)
  {
    if(quick)
    {
      position = (quickDirection / 100) * fullLength - stepper.distanceToGo();
      formerDTG = stepper.distanceToGo();
    }
    else
    {
      position = fullLength - stepper.distanceToGo();
      formerDTG = stepper.distanceToGo();
    }
  }
 
  Serial.println(position);
}