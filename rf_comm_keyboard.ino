#include <nRF24L01.h>
#include <RF24.h>

// nrf24l01 RF Board setup
const int CE_PIN  = 9;
const int CSN_PIN = 10;
RF24 radio(CE_PIN, CSN_PIN);
const uint64_t pipe = 0xB0B15ADEADBEEFAA;

// Keyboard
const int key_rows = 6;
const int key_cols = 2;
int const  col_pins[] = {A1, A0};
int const row_pins[] = {3, 4, 5, 6, 7, 8};

/* Keys are defined as:
       Col0    Col1
      [ 0 ]   [ 1 ]  // Row 0
      [ 2 ]   [ 3 ]  // Row 1
      [ 4 ]   [ 5 ]  ...
      [ 6 ]   [ 7 ]
      [ 8 ]   [ 9 ]
      [ 10 ]  [ 11 ] // Row 5
*/
inline void initKeys() {
  for (int i = 0; i < key_cols; i++) {
    pinMode(col_pins[i], INPUT_PULLUP);
  }

  for (int j = 0; j < key_rows; j++) {
    pinMode(row_pins[j], OUTPUT);
    digitalWrite(row_pins[j], HIGH);
  }
}

// takes ~ 167us -> ~6000Hz
inline uint16_t scanKeys() {
  uint16_t scanResult = 0x0;
  //digitalWrite(col_pins[i], HIGH);
  for (int j = 0; j < key_rows; j++) {
    digitalWrite(row_pins[j], LOW);
    for (int i = 0; i < key_cols; i++) {
      int result = digitalRead(col_pins[i]);
      if (result == LOW) {
        scanResult |= 0x1000;
      }
      scanResult >>= 1;
    }
    digitalWrite(row_pins[j], HIGH);
    //digitalWrite(col_pins[i], LOW);
  }
  return scanResult;
}

// ~ 50 us -> 20Kh
// this may be vulnerable to switch bounce, the switches I'm using don't need debouncing
inline uint16_t scanKeys_optimized() { // basically an unrolled-port manipulation version
  uint16_t scanResult = 0x0, resultCol0 = 0, resultCol1 = 0;

  //digitalWrite(3, LOW);
  PORTD &= 0b11110111;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(3, HIGH);
  PORTD |= 0b00001000;
  if (resultCol0 == LOW) {
    scanResult |= 0x1;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x2;
  }

  //digitalWrite(4, LOW);
  PORTD &= 0b11101111;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(4, HIGH);
  PORTD |= 0b00010000;
  if (resultCol0 == LOW) {
    scanResult |= 0x4;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x8;
  }

  //digitalWrite(5, LOW);
  PORTD &= 0b11011111;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(5, HIGH);
  PORTD |= 0b00100000;
  if (resultCol0 == LOW) {
    scanResult |= 0x10;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x20;
  }

  //digitalWrite(6, LOW);
  PORTD &= 0b10111111;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(6, HIGH);
  PORTD |= 0b01000000;
  if (resultCol0 == LOW) {
    scanResult |= 0x40;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x80;
  }

  //digitalWrite(7, LOW);
  PORTD &= 0b01111111;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(7, HIGH);
  PORTD |= 0b10000000;
  if (resultCol0 == LOW) {
    scanResult |= 0x100;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x200;
  }

  //digitalWrite(8, LOW);
  PORTB &= 0b11111110;
  resultCol0 = digitalRead(A1);
  resultCol1 = digitalRead(A0);
  //digitalWrite(8, HIGH);
  PORTB |= 0b00000001;
  if (resultCol0 == LOW) {
    scanResult |= 0x400;
  }
  if (resultCol1 == LOW) {
    scanResult |= 0x800;
  }
  return scanResult;
}

void setup() {
  Serial.begin(9600);
  initKeys();
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate( RF24_250KBPS );
  radio.openWritingPipe(pipe);
}

uint16_t keyData = 0x0;
uint16_t prevKeyData = 0x0;
void loop() {
  keyData = scanKeys_optimized();
  if (keyData != prevKeyData) {
    //delay(10); // add a delay to remove the effects of bounce
    Serial.print("Key result: 0x");
    Serial.println(keyData,HEX);
    radio.write(&keyData, sizeof(keyData) );
    uint8_t pipe = 0x0;
    if (radio.available(&pipe)) { // received an ACK, try to process it
      uint32_t rotationPeriod = 0;
      radio.read(&rotationPeriod,sizeof(rotationPeriod));
    //  Serial.print("Received packet of (rotation period): ");
    //  Serial.print(rotationPeriod);
    //  Serial.println(" us");
    }
  }
  prevKeyData = keyData;
  delay(10); // maybe give the teensy some time to breathe
}
