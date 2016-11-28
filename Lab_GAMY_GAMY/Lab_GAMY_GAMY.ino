#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Network related stuff goes in here
namespace Network {
  //////////////////////
  // WiFi Definitions
  //////////////////////
  const char WiFiSSID[] = "chop";
  const char WiFiPSK[] = "butch3rs";

  //////////////////////
  // Host and Port Definitions
  //////////////////////
  IPAddress hostIP(192, 168, 1, 74); // (original)hostIP(192, 168, 10, 1);
  const uint16_t port = 13100;

  // Use WiFiUDP class to create UDP connections
  WiFiUDP client;

  // Which player are we?
  byte player;

  void connect() {
    Serial.println();
    Serial.println("Connecting to: " + String(WiFiSSID));
    // Set WiFi mode to station (as opposed to AP or AP_STA)
    WiFi.mode(WIFI_STA);

    // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
    // to the stated [ssid], using the [passkey] as a WPA, WPA2,
    // or WEP passphrase.
    WiFi.begin(WiFiSSID, WiFiPSK);

    // Use the WiFi.status() function to check if the ESP8266
    // is connected to a WiFi network.
    while (WiFi.status() != WL_CONNECTED)
      delay(100);

    // success
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    delay(500);

    Serial.print("Listening on port: ");
    Serial.println(port);

    client.begin(port);
  }

  // For internal use
  void sendData(char* data) {
    client.beginPacket(hostIP,port);
    client.print("{\"game\":\"SimonSays\",\"payload\":\"");
    client.print(data);
    client.print("\"}");
    client.endPacket();
  }

  // For internal use
  void sendData(String* data) {
    client.beginPacket(hostIP,port);
    client.print("{\"game\":\"SimonSays\",\"payload\":\"");
    client.print(data->c_str());
    client.print("\"}");
    client.endPacket();
  }

  // For internal use
  String* readData() {
    // Read all the lines of the reply from server and print them to Serial
    if(client.parsePacket() != 0) {
      String* data = new String("");
      while(client.peek() != -1)
        *data += (char)client.read();

      Serial.println(*data);
      return data;
    }
    return NULL;
  }

  void sendInitialHandshake() {
    sendData("HLO");
  }

  boolean didReceiveHandshake() {
    String* data = readData();
    boolean response = false;
    if(data != NULL) {
      if(data->startsWith("HLO")) {
        response = true;
        player = 1;
      } else if(data->startsWith("HI")) {
        response = true;
        player = 0;
      }
      delete data;
      return response;
    }
  }

  void sendHandshakeResponse() {
    sendData("HI");
  }

  // Use this to actually send the game data
  void sendGameData(byte* selections,int length) {
    String* data = new String("?");

    *data += "selections=[";
    for(int i = 0; i < length; ++i) {
      if(selections[i] == 255)
        break;
      if(i != 0) *data += ',';
      *data += selections[i];
    }
    *data += "]";

    sendData(data);
    delete data;
  }

  // Use this to receive the game data
  boolean receiveGameData(byte* selections) {
    String* data = readData();
    boolean response = false;
    if(data != NULL && data->startsWith("?")) {
      // Read in selections
      String temp = "";
      int dataIndex = data->indexOf("[");
      int selectionsIndex = 0;
      // Is selections in the query?
      if(dataIndex != 0) {
        // Loop over each character until the end of the query array
        for(int i = dataIndex; data->charAt(i) != ']'; i++) {
          if (isDigit(data->charAt(i))) {
            temp = data->charAt(i);
            selections[selectionsIndex++] = temp.toInt();
          }
        }
      }
      selections[selectionsIndex] = 255;

      // We did receive data
      response = true;
    }

    // cleanup
    delete data;
    return response;
  }
}

// declaration section
int encoderPin1 = 2;
int encoderPin2 = 14;
int encoderButton = 5;
int buttonPin = 15;
int ledPin_1 = 4;
int ledPin_2 = 13;
int ledPin_3 = 12;
int ledPin_4 = 16;
int leds[] = {ledPin_1, ledPin_2, ledPin_3, ledPin_4};
byte selectLed[500];
byte answerInput[500]; // for checking correct input on player 2(0)
volatile long lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;
int MSB, LSB, encoded, sum;

int i = -1;
int cur = 0;
byte level = 0;
int timeKeeper;
int startTime;
String debugState;
int score = 0;

// All states that the game can be in
enum class State { Null, WaitingForButton, WaitingForData, Active, GameEnd };
State state = State::Null;

void setup() {
  Serial.begin (9600);
  // set input pinmode
  pinMode(encoderPin1, INPUT_PULLUP);
  pinMode(encoderPin2, INPUT_PULLUP);
  pinMode(buttonPin, INPUT);
  pinMode(encoderButton, INPUT_PULLUP);
  //turn pull-up resistors on
  digitalWrite(encoderPin1, HIGH);
  digitalWrite(encoderPin2, HIGH);
  digitalWrite(encoderButton, HIGH);
  // set output pinmode
  pinMode(ledPin_1, OUTPUT);
  pinMode(ledPin_2, OUTPUT);
  pinMode(ledPin_3, OUTPUT);
  pinMode(ledPin_4, OUTPUT);
  //call updateEncoder() when any high/low changed seen
  attachInterrupt(digitalPinToInterrupt(encoderPin1), updateEncoder, RISING);
  attachInterrupt(digitalPinToInterrupt(encoderPin2), updateEncoder, RISING);

  initialize();
}

void initialize() {
  digitalWrite(ledPin_1, HIGH);
  i = -1;
  cur = 0;
  timeKeeper = millis();
  Serial.println(""); // to get off first line

  Network::connect();
  Network::sendInitialHandshake();
  while(true) {
    if(Network::didReceiveHandshake()) {
      Network::sendHandshakeResponse();
      break;
    }

    delay(10);
  }
  // If connected and player 1 goto WaitingForButton state
  if (Network::player == 1) {
    state = State::WaitingForButton;
  } else if (Network::player == 0) {
    state = State::WaitingForData;
  }
}

void loop() {

  if (Network::player == 1) {
    switch (state) {
      case State::WaitingForButton: {
        // Waiting for start button to be pressed, LED 1 blinking
        blinkLed(leds[0], 200);
        if (digitalRead(buttonPin)) {
          Serial.println("Start button pressed.");
          debug("WaitingForButton");
          startTime = millis();
          state = State::Active;
          digitalWrite(leds[level], HIGH);
          cur = level;
        }
      } break;

      case State::WaitingForData: {
        // Waiting to hear back from player 0 after pattern sent, All LEDs blinking
        blinkAll(200);
      } break;

      case State::Active: {
        // Start button pressed
        // Creating pattern
        if (lastencoderValue != encoderValue) {
          if (lastencoderValue < encoderValue && encoderValue % 2 == 0) {
            // change lights
            digitalWrite(leds[cur], LOW);
            cur++;
            if (cur > 3) {
              cur = 0;
            }
            digitalWrite(leds[cur], HIGH);
          } else if (lastencoderValue > encoderValue && encoderValue % 2 == 0) {
            // change lights
            digitalWrite(leds[cur], LOW);
            cur--;
            if (cur < 0) {
              cur = 3;
            }
            digitalWrite(leds[cur], HIGH);
          }
          lastencoderValue = encoderValue;
        }

        if (!digitalRead(encoderButton) && (timeKeeper - startTime < 10000)) {
          // Blink the LED
          debug("Active");
          i++;
          selectLed[i] = cur;
          blinkLed(leds[cur], 100);
          digitalWrite(leds[cur], HIGH);
        } else if (timeKeeper - startTime > 10000) {
          // send data | switch state
          Serial.println("Pattern input done.");
          state = State::WaitingForData;
          selectLed[i+1] = 255;
          Network::sendGameData(selectLed, 500);
          Serial.println("Game data sent.");

          // progress level by 1 (should cap and end game by level 3 TODO)
          level++;
          Network::player = 0;
          Serial.println("Score checked and recorded player 1 -> player 2 and now should wait to recive patern.");
        }
      } break;

      case State::GameEnd: {
        // Display (using red or green led) winner/loser
      } break;
    }
  } else if (Network::player == 0) {
    switch (state) {
      case State::WaitingForButton: {
        // Once patern recived, LED 2 blinking | Waiting for start button press
        blinkLed(leds[1], 200);
        if (digitalRead(buttonPin)) {
          Serial.println("Start button pressed.");
          startTime = millis();
          state = State::Active;
          digitalWrite(leds[level], HIGH);
          cur = level;
        }
      } break;

      case State::WaitingForData: {
        // Waiting to recive pattern from player 1, All LEDs blinking
        blinkAll(200);
        bool dataRecived = Network::receiveGameData(selectLed);
        if (dataRecived) {
          // Signal dataRecived
          blinkAll(900);
          // Pattern playback
          int k = -1;
          while (selectLed[k+1] != 255) {
            k++;
            Serial.println(selectLed[k]);
            digitalWrite(leds[selectLed[k]], HIGH);
            delay(600);
            digitalWrite(leds[selectLed[k]], LOW);
            delay(600);
          }
          delay(1000);
          state = State::WaitingForButton;
        }
      } break;

      case State::Active: {
        // Start button pressed
        // Inputing pattern
        if (lastencoderValue != encoderValue) {
          if (lastencoderValue < encoderValue && encoderValue % 2 == 0) {
            // change lights
            digitalWrite(leds[cur], LOW);
            cur++;
            if (cur > 3) {
              cur = 0;
            }
            digitalWrite(leds[cur], HIGH);
          } else if (lastencoderValue > encoderValue && encoderValue % 2 == 0) {
            // change lights
            digitalWrite(leds[cur], LOW);
            cur--;
            if (cur < 0) {
              cur = 3;
            }
            digitalWrite(leds[cur], HIGH);
          }
          lastencoderValue = encoderValue;
        }

        if (!digitalRead(encoderButton)) {
          // Blink the LED
          debug("Active");
          i++;
          selectLed[i] = cur;
          blinkLed(leds[cur], 100);
          digitalWrite(leds[cur], HIGH);
        }

        if (digitalRead(buttonPin)) {
          // send data | switch state
          //(compear and score)
          int k = -1;
          while (selectLed[k] != 255) {
            // Compear answer and score based on accuracy and time //TODO
          }

          Serial.println("Answer input done.");
          state = State::WaitingForButton;
          Network::player = 1;
          Serial.println("Score checked and recorded player 2 -> player 1 and now should recored pattern.");
        }
      } break;

      case State::GameEnd: {
        // Display (using red or green led) winner/loser
        debugState = "GameEnd";
      } break;
    }
  }

  timeKeeper = millis();
  delay(10); // we like a little delay
}

void debug(String debugState) {
  Serial.println("---------------------Debug---------------------");
  Serial.print(String("player = ") + Network::player + String("\nlevel = ") +
  level + String("\ncur = ") + cur + String("\ni = ") + i +
  String("\nstate = ") + debugState + String("\n"));
  Serial.println("--------------------EndDebug-------------------");
}

// Blinks a spesified led for a spesified delay (del)
void blinkLed(int led, int del) {
  digitalWrite(led, HIGH);
  delay(del);
  digitalWrite(led, LOW);
  delay(del);
}

// Blinks all leds for a spesified delay (del)
void blinkAll(int del) {
  digitalWrite(leds[0], HIGH);
  digitalWrite(leds[1], HIGH);
  digitalWrite(leds[2], HIGH);
  digitalWrite(leds[3], HIGH);
  delay(del);
  digitalWrite(leds[0], LOW);
  digitalWrite(leds[1], LOW);
  digitalWrite(leds[2], LOW);
  digitalWrite(leds[3], LOW);
  delay(del);
}

void updateEncoder() {
  cli(); //stop interrupts happening before we read pin values
  MSB = digitalRead(encoderPin1); //MSB = most significant bit
  LSB = digitalRead(encoderPin2); //LSB = least significant bit
  sei(); //restart interrupts
  encoded = (MSB << 1) | LSB; //converting the 2 pin value to single number
  sum = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;
  lastEncoded = encoded; //store this value for next time
}
