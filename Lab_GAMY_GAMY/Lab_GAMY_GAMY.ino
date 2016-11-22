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
byte seletLed[500];
byte duration[500];
volatile long lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;
int MSB, LSB, encoded, sum;

// My Variables
int i = -1;
int cur = 0;

bool blinking = false;

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
  Serial.println(""); // to get off first line
}

void loop() {
  if (!digitalRead(encoderButton) && !blinking) {
    // Blink the LED
    blinking = true;
    int k;
    for (k=0;k<3;k++) {
      digitalWrite(leds[cur], HIGH);
      delay(200);
      digitalWrite(leds[cur], LOW);
      delay(200);
    }
    digitalWrite(leds[cur], HIGH);
    blinking = false;
  }

  //[ileri]: This is mostly for testing purposes
  if (digitalRead(buttonPin) && !blinking) {
    // Blink all LEDs
    int k;
    for (k=0;k<3;k++) {
      digitalWrite(leds[0], HIGH);
      digitalWrite(leds[1], HIGH);
      digitalWrite(leds[2], HIGH);
      digitalWrite(leds[3], HIGH);
      delay(200);
      digitalWrite(leds[0], LOW);
      digitalWrite(leds[1], LOW);
      digitalWrite(leds[2], LOW);
      digitalWrite(leds[3], LOW);
      delay(200);
    }
    digitalWrite(leds[cur], HIGH);
    blinking = false;
  }

  if (lastencoderValue != encoderValue && !blinking) {
    Serial.println(String("ev: ") + encoderValue);
    Serial.println(String("cur: ") + cur);
    Serial.println();
    
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
  
  delay(10); // we like a little delay
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
