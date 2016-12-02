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
  IPAddress hostIP(192, 168, 10, 1);
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
  void sendData(String data) {
    client.beginPacket(hostIP,port);
    client.print("{\"game\":\"SimonSays\",\"payload\":\"");
    client.print(data.c_str());
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
        player = 2;
      }
      delete data;
    }
    return response;
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

      // cleanup
      delete data;
    }

    return response;
  }

  void sendGameScore(int score) {
    sendData("S" + String(score));
  }

  int receiveGameScore() {
    String* data = readData();
    int score = -1;
    if(data != NULL && data->startsWith("S")) {
      data->remove(0,1);
      score = data->toInt();
      delete data;
    }

    return score;
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
volatile long lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;
int MSB, LSB, encoded, sum;

byte selectLed[500];
byte answerInput[500]; // for checking correct input on other player
int i = -1;
int cur = 0;
byte level = -1;
int timeKeeper;
int startTime;
String debugState;
int score = 0;
int p2Score = -1;
byte toggle;
int count = -1;

bool printDebug = false;
bool master = false; // Am I the one making the pattern

// All states that the game can be in
enum class State { Null, WaitingForButton, WaitingForData, Active, GameEnd };
State state = State::Null;

void setup() {
  Serial.begin (9600);
  // set input pinmode
  pinMode(encoderPin1, INPUT_PULLUP);
  pinMode(encoderPin2, INPUT_PULLUP);
  pinMode(encoderButton, INPUT_PULLUP);
  pinMode(buttonPin, INPUT);
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
    toggle = 2;
    master = false;
  } else if (Network::player == 2) {
    toggle = 2;
    master = true;
  }
}

void loop() {

  if (toggle == 0) {
    switch (state) {
      case State::WaitingForButton: {
        debugState = "WaitingForButton-T1";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
        }
        // Waiting for start button to be pressed, LED 1 blinking
        blinkLed(leds[0], 200);
        if (digitalRead(buttonPin)) {
          Serial.println("Start button pressed.");
          debug("WaitingForButton-BP");
          startTime = millis();
          state = State::Active;
          digitalWrite(leds[level], HIGH);
          cur = level;

          //-db-
          printDebug = false;
        }
      } break;

      case State::WaitingForData: {
        // Waiting to hear back from toggle in toggle 1 after pattern sent, All LEDs blinking
        blinkAll(200);
        debugState = "WaitingForData-T1";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          delay(5000);
          //-Enddebug-
        }
      } break;

      case State::Active: {
        debugState = "Active-T1";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
        }
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

        if (!digitalRead(encoderButton) && (timeKeeper - startTime < 8000)) {
          // Blink the LED
          i++;
          debug("Active-BP1");
          selectLed[i] = cur;
          blinkLed(leds[cur], 100);
          digitalWrite(leds[cur], HIGH);
        } else if (timeKeeper - startTime > 8000 || (digitalRead(buttonPin) && timeKeeper - startTime > 1000)) {
          // send data | switch state
          Serial.println("Pattern input done.");
          state = State::WaitingForData;
          selectLed[i+1] = 255;
          Network::sendGameData(selectLed, 500);
          Serial.println("Game data sent.");

          toggle = 2;
          i = -1;
          //level++;

          //-db-
          printDebug = false;
          Serial.println("Score checked and recorded toggle 0 -> toggle 1 and now should wait to recive patern.");
        }
      } break;

      case State::GameEnd: {
        debugState = "GameEnd-T1";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
          delay(1000);
        }
        // Display (using red or green led) winner/loser
        Network::sendGameScore(score);
        p2Score = Network::receiveGameScore();
        if (p2Score != -1) {
          debug("Inside if statement");

          digitalWrite(leds[0], LOW);
          digitalWrite(leds[1], LOW);
          digitalWrite(leds[2], LOW);
          digitalWrite(leds[3], LOW);

          if (score > p2Score)
            digitalWrite(leds[1], HIGH);
          else if (score < p2Score)
            digitalWrite(leds[0], HIGH);
          else
            digitalWrite(leds[2], HIGH);

          state = State::Null;
        }
      } break;
    }
  } else if (toggle == 1) {
    switch (state) {
      case State::WaitingForButton: {
        // Once patern recived, LED 2 blinking | Waiting for start button press
        blinkLed(leds[1], 200);
        debugState = "WaitingForButton-T2";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
        }
        if (digitalRead(buttonPin)) {
          debug("WaitingForButton-BP");
          Serial.println("Start button pressed.");
          startTime = millis();
          state = State::Active;
          digitalWrite(leds[level], HIGH);
          cur = level;

          //-db-
          printDebug = false;
        }
      } break;

      case State::WaitingForData: {
        // Waiting to recive pattern from toggle 1, All LEDs blinking
        blinkAll(200);
        debugState = "WaitingForData-T2";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
        }
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

          //-db-
          printDebug = false;
        }
      } break;

      case State::Active: {
        // Start button pressed
        // Inputing pattern
        debugState = "Active-T2";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
        }
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
          i++;
          debug("Active-BP2");
          answerInput[i] = cur;
          blinkLed(leds[cur], 100);
          digitalWrite(leds[cur], HIGH);
        }

        if (digitalRead(buttonPin) && timeKeeper - startTime > 1000) {
          // send data | switch state
          //(compear and score)
          allLow();
          int k = 0;
          while (selectLed[k] != 255) {
            // Compear answer and score based on accuracy and time
            if (answerInput[k] == selectLed[k]) {
              Serial.println(String("k(") + k + String("): answerInput[") + answerInput[k] + String("] == selectLed[") + selectLed[k] + String("] - Correct!"));
              score++;
            } else {
              Serial.println(String("k(") + k + String("): answerInput[") + answerInput[k] + String("] == selectLed[") + selectLed[k] + String("] - Wrong."));
              if (score > 0) {
                score--;
              }
            }
            k++;
          }

          toggle = 2;

          Serial.println("Answer input done.");
          state = State::WaitingForButton;
          i = -1;

          //-db-
          printDebug = false;

          /*/ Checks to see if you are going on level 3 and enter GameEnd state
          if ( level == 1) {
            state = State::GameEnd;
            break;
          }
          // progress level by 1 (should cap and end game after level 3)
          level++;*/
          Serial.println("Score checked and recorded toggle 1 -> toggle 0 and now should recored pattern.");
        }
      } break;

      case State::GameEnd: {
        // Display (using red or green led) winner/loser
        debugState = "GameEnd-T2";
        if (printDebug == false) {
          //--debug--
          debug(debugState);
          //-Enddebug-
          printDebug = true;
          delay(5000);
        }
        // Player 1 should already have this from above
        if (p2Score != -1) {
          if (score > p2Score)
            digitalWrite(leds[1], HIGH);
          else
            digitalWrite(leds[0], HIGH);

          state = State::Null;
        }

      } break;
    }
  } else if (toggle == 2) {
    Network::sendData("inSYNC");
    String* recived = Network::readData();
    if (recived != NULL && recived->startsWith("inS")) {
      debug(String("I player ") + Network::player + String(" am all syced up!"));

      count++;
      if (count % 2 == 0)
        level++;

      if (level == 2) {
        toggle = 0;
        state = State::GameEnd;
      } else if (master) {
        master = false;
        toggle = 1;
        state = State::WaitingForData;
      } else if (!master) {
        master = true;
        toggle = 0;
        state = State::WaitingForButton;
      }
    }
  }

  timeKeeper = millis();
  delay(10); // we like a little delay
}

void debug(String debugState) {
  Serial.println("---------------------Debug---------------------");
  Serial.print(String("toggle = ") + toggle + String("\nlevel = ") +
  level + String("\ncur = ") + cur + String("\ni = ") + i +
  String("\nscore = ") + score + String("\nstate = ") + debugState +
  String("\np2Score = ") + p2Score + String("\n") + String("\ncount = ")
  + count + String("\n"));
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

// Blinks all leds for a spesified delay (del)
void allLow() {
  digitalWrite(leds[0], LOW);
  digitalWrite(leds[1], LOW);
  digitalWrite(leds[2], LOW);
  digitalWrite(leds[3], LOW);
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
