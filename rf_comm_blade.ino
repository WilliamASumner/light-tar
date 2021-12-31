//#include <DigitalIO.h> // CHANGE RF24/RF24_config.h line 27: #define SOFTSPI line 28-31: define SPI pins
#include <Tlc5948.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "Colors.h"
#include "animation_demos.h" // animation demos

#ifdef ARDUINO_TEENSY40 // Teensy 4.0
// For anyone reading this, I found these by using the 4.0 schematic at the bottom of the
// Teensy 4.x page: https://www.pjrc.com/teensy/schematic.html
// and cross-referencing it with https://www.pjrc.com/teensy/IMXRT1060RM_rev2.pdf
//         SIGNAME      MANUAL PORT NAME    GPIO NAME         IRQ #
//        --------      ----------------    ----------        -----
//        SCLK, 13          B0_03           GPIO2_IO03          82
//        MOSI  11          B0_02           GPIO2_IO02          82
//        MISO  12          B0_01           GPIO2_IO01          82
const int IRQ_PIN = 6;   // B0_10           GPIO2_IO10          82  ***
const int CE_PIN = 8;    // B1_00           GPIO2_IO16          83
const int CSN_PIN = 9;   // B0_11           GPIO2_IO11          82
const int NOTEPIN1 = 3;  // EMC_05          GPIO2_IO03          82
const int NOTEPIN2 = 4;  // EMC_06          GPIO4_IO06          86
const int NOTEPIN3 = 5;  // EMC_08          GPIO4_IO08          86
const int MAG_OUT = 14;  // AD_B1_02        GPIO1_IO18          81  ***

const int RADIO_SPI_SPEED = 1000000; // 1Mhz; TODO try faster clocks

const int MAG_IRQ =   81;
const int RADIO_IRQ = 82;

const int MAG_PRIORITY =     127; //  //
const int RADIO_PRIORITY =   128; //  ||-- choose these carefully! Display should be highest (lowest num)
const int DISPLAY_PRIORITY = 126; //  //
#else
#error "Unimplemented"
const int CE_PIN = 9;
const int CSN_PIN = 10;
const int NOTEPIN1 = 3;
const int NOTEPIN2 = 5;
const int NOTEPIN3 = 6;
const int MAG_OUT = 3;
#endif

/*
       ****  Note Processing   ****
     ------------------------------
        * *       ******      * *
        *  *      *    *      *  *
     ****      **** ****   ****
     ****      **** ****   ****
     ------------------------------
 */
float freqMultiplier = 512.0; // multiplier used to control octave and adjust for PWM count
// in ESPWM it should be ~512, in normal PWM it should be ~65536
float offFrequency = 8000000; // supersonic (hopefully!)
inline void noteSetup() {
  pinMode(NOTEPIN1, OUTPUT);
  pinMode(NOTEPIN2, OUTPUT);
  pinMode(NOTEPIN3, OUTPUT);

  analogWriteFrequency(3, offFrequency); // set default frequencies ( PWM_REG -> 120Hz, ES_PWM -> 15Khz)
  analogWriteFrequency(4, offFrequency);
  analogWriteFrequency(5, offFrequency);

  analogWrite(3, 127);
  analogWrite(4, 127);
  analogWrite(5, 127);
}

inline void playFreq(int notePin, float noteFreq) {
  analogWriteFrequency(notePin, noteFreq * 512);
}

inline void stopFreq(int notePin) {
  analogWriteFrequency(notePin, offFrequency);
  //analogWrite(notePin,0); // do nothing for testing
}

/*
      \
        *****       ********     ****    /
        *****       ********     ******** 
     -- *****       *****        *********  --
        *****       *****        ********* 
        *********   ********     ******** 
     /  *********   ********     ****    \
 */
const int NUM_TLCS = 3;
Tlc5948 tlc;
const uint8_t rowSize = 16;
const uint8_t ringSize = 60; // 60; // aka columns
// This is a big variable... check if we need this
Pixel* displayBuffer   = (Pixel*)malloc(sizeof(Pixel)  * rowSize * ringSize);
Pixel* displayBufferTwo = (Pixel*)malloc(sizeof(Pixel)  * rowSize * ringSize);
Pixel* scratch = (Pixel*)malloc(sizeof(Pixel)  * rowSize * ringSize); // scratch pad for animation

// Display refresh rate notes:
// According to Quora (still need to test): house fan 1300 RPM -> 22 RPS -> ~20 FPS
// 20 FPS means 0.05s per rotation / 60 columns -> 833us per column
// 30 FPS -> 555us per column
// 60 FPS -> 277us per column
// Currently, SPI tlc5948 transfer takes 39us per column! Should work c:

IntervalTimer displayTimer; // how we display at the 'right' time for each column
volatile unsigned int displayColumn = 0;

void updateDisplayIsr() {
  tlc.writeGsBufferSPI16((uint16_t*)(displayBuffer + displayColumn), rowSize * NUM_TLCS, NUM_TLCS);
  displayColumn = (displayColumn + 1) % ringSize;
  asm("dsb");
}

inline void ledSetup() {
  SPI.begin();
  tlc.begin(true);

  tlc.writeGsBufferSPI16((uint16_t*)colorPalette, 3, 3); // clear out the gs data (likely random)
  tlc.setDcData(Channels::out1, 0xff); // dot correction
  tlc.setBcData(0x7f); // global brightness

  Fctrls fSave = tlc.getFctrlBits();
  fSave &= ~(Fctrls::tmgrst_mask);
  fSave |= Fctrls::tmgrst_mode_1; // set timing reset
  fSave &= ~(Fctrls::dsprpt_mask);
  fSave |= Fctrls::dsprpt_mode_1; // set autodisplay repeat

  fSave &= ~(Fctrls::espwm_mask);
  fSave |= Fctrls::espwm_mode_1; // set ES PWM mode on, basically breaks up
  // long ON/OFF periods into 128 smaller segments
  // with even distribution
  tlc.setFctrlBits(fSave);
  tlc.writeControlBufferSPI(NUM_TLCS);
}

inline void testFlash() {
  // check that the radio and drivers are getting along
  tlc.writeGsBufferSPI16((uint16_t*)(colorPalette + 1), 3, NUM_TLCS);
  delay(500);
  tlc.writeGsBufferSPI16((uint16_t*)colorPalette, 3, NUM_TLCS);
}

inline void displayTimerSetup() {
  unsigned int timerDurationUs = 200000 / ringSize; // 5Hz (200ms) / 60 cols = 3333 us / col
  //\ This will get updated by hall-effect
  displayTimer.priority(DISPLAY_PRIORITY);
  displayTimer.begin(updateDisplayIsr, timerDurationUs); // start timer with a guess-timate of column time
  displayTimer.end(); // this is to stop it while debugging
}

/*   
     ***********           ***********
     **   +   **           **   -   **
     ***********           ***********
     ************         ************
     *********************************
      ****** Hall-Effect Setup ******
       *****************************
*/

// Estimated rotation of about 30 rotations per s ~> 0.033 s or 33 us / rotation to do stuff in general
// (animation, updating audio etc)
volatile uint32_t prevTimeUs = 0;
volatile uint32_t newDurationUs = 0;
volatile uint32_t tmpDurationUs = 0;
void hallIsr() {
  // Create a timer with a duration that will give 60 even segments using the time
  // Because a rotation likely won't go over 1s and micros() overflows at 1hr, we should be ok
  tmpDurationUs = newDurationUs; 
  newDurationUs = (micros() - prevTimeUs) / ringSize; // convert elapsed time to col time
  if (newDurationUs < 2 || newDurationUs > 1000) {
    newDurationUs = tmpDurationUs; // restore old value on "bad" calculations, might need a filter for this
  }
  displayTimer.update(newDurationUs); // new displayTimer duration
  displayColumn = 0; // reset to beginning of display
  prevTimeUs = micros(); // reset elapsed time
  asm("dsb"); // wait for ISR bit to be cleared before exiting
}

inline void magSetup() {
  pinMode(MAG_OUT, INPUT_PULLDOWN);
  NVIC_SET_PRIORITY(MAG_IRQ, MAG_PRIORITY);
  attachInterrupt(digitalPinToInterrupt(MAG_OUT), hallIsr, RISING); // TODO check if there's a better way to do this? VVV
  // A couple of possible improvements: change VREF for RISING (check manual)
  //                   switch to polling with ADC (would that really be better though?)
  //                       \ Add Kalman filter to ADC data and trigger on threshold
}

/*
                 ***************************
                 *** Keyboard Processing ***
                 ***************************
 +----------------------------------------------------------------+         
 |     [q] [w] [e] [r] [t] [y] [u] [i] [o] [p] [[] []] [\]        |
 |       [a] [s] [d] [f] [g] [h] [j] [k] [l] [;] ['] [enter]      |
 | [shift] [z] [x] [c] [v] [b] [n] [m] [,] [.] [/]  [ shift]      |
 | [fn] [ctrl] [alt] [cmd] [      space     ] [cmd] [alt] [ctrl]  |
 +----------------------------------------------------------------+

 */

/* Keys are defined as:
       Col0    Col1
      [ 0 ]   [ 1 ]  // Row 0  \___ Note Group 1
      [ 2 ]   [ 3 ]  // Row 1  /
      -------------
      [ 4 ]   [ 5 ]  ... \_________ Note Group 2
      [ 6 ]   [ 7 ]  ... /
      -------------
      [ 8 ]   [ 9 ]  ...      \____ Note Group 3
      [ 10 ]  [ 11 ] // Row 5 /

*/

// Current frequencies are just two strings an octave apart
const float keysToFreq[12] = {164.81, 329.63, 174.61, 349.23,
                              185.00, 369.99, 196.00, 392.00,
                              207.65, 415.30, 220.00, 440.00
                             };
const uint8_t keysToPin[12] = {NOTEPIN1, NOTEPIN1, NOTEPIN1, NOTEPIN1,
                               NOTEPIN2, NOTEPIN2, NOTEPIN2, NOTEPIN2,
                               NOTEPIN3, NOTEPIN3, NOTEPIN3, NOTEPIN3
                              };
const uint8_t keysToGroup[12] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
bool groupPlaying[3] = {false, false, false};
uint16_t keyData = 0x0;
uint16_t prevKeys = 0x0; // holder for previous value of keys
const int nKeys = 12;
const int NO_KEY_CHANGE = 0x1 << nKeys; // TODO check if nKeys > sizeof(int)*8

// TODO share constants through header...

inline uint16_t processKeys(uint16_t keys) { // timed at 0.24us @ 600Mhz
  if (prevKeys == keys) {
    return NO_KEY_CHANGE; // nothing to do
  }
  return prevKeys ^ keys;
}

void audioUpdate(uint16_t keys, uint16_t diff) {
  if (keys == prevKeys)
    return NO_KEY_CHANGE;
  for (int i = 0; i < 12; i++) {
    if (diff & 0x1) { // if there's a difference here
      uint8_t pinNum = keysToPin[i];

      // if a key is on and no other note in group playing
      if ((keys & 0x1) && !groupPlaying[keysToGroup[i]]) {
        playFreq(pinNum, keysToFreq[i]); //note on
      } else if (!(keys & 0x1)) {
        stopFreq(pinNum); // note off
      }
      diff >>= 1;
      keys >>= 1;
    }
  }
}

// Ring-Buffer and functions for storing keypresses
// Note: This really should use atomic operations... however the time it takes a person
// to make a keypress and send it is *significantly* slower than the time it is to add/push a key
// so I'm leaving it out for now...
// This might create issues with rapid consecutive keypresses or keys pushed "at the same time"
// and if so then atomic ops would be the way to fix that
// [keypress1] [keypress2] [keypress3] [keypress4]
const int keyBufferSize = 16; // Actual buffer size
volatile int insertIndex = 0;
volatile int readIndex = 0;
volatile int bufferCount = 0; // Number of valid items in buffer
uint16_t keyBuffer[keyBufferSize] = { 0 };

inline void pushKey(uint16_t key) {
  keyBuffer[insertIndex] = key;
  insertIndex = (insertIndex + 1 ) % keyBufferSize;
  if (bufferCount == keyBufferSize) { // if full
    readIndex = (readIndex + 1 ) % keyBufferSize;
  } else {
    bufferCount++;
  }
}

inline uint16_t popKey() {
  if (bufferCount == 0) { // can only be empty
    return 0x0; // return no key press
  }
  uint16_t tempVal = keyBuffer[readIndex];
  readIndex = (readIndex + 1) % keyBufferSize;
  bufferCount--;
  return tempVal;
}

inline bool keyBufferNotEmpty() {
  return bufferCount > 0;
}

/*                      o
                      //
   ******           //  ******
   ***************************
   ***O**| Radio Setup |**O***
   ***************************
   v                         v
*/

RF24 radio(CE_PIN, CSN_PIN, RADIO_SPI_SPEED);
const uint64_t pipe = 0xB0B15ADEADBEEFAA;

void radioIsr() {
  bool tx_ok, tx_fail, rx_ready;
  uint16_t keyVal = 0x0;
  radio.whatHappened(tx_ok, tx_fail, rx_ready); // figure out why we're getting a call
  if (rx_ready) { // we only want to do sth for new incoming data
    // Read in the value and add it to buffer of things to be processed
    radio.read(&keyVal, sizeof(keyVal));
    pushKey(keyVal);

    // Write the acknowledgement
    // This gives a compiler warning because the newDuration can be changed in an ISR,
    // however it's not catastrophic - this is just to get an idea of rotation speed,
    // so there's no harm if it sends an incorrect value once in a while
    radio.writeAckPayload(1, &newDurationUs, sizeof(newDurationUs));
  }
  asm("dsb"); // wait for the ISR bit to be cleared in case this is too short of an ISR (necessary?)
}

inline void radioSetup() {
  radio.begin();
  radio.openReadingPipe(1, pipe);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate( RF24_250KBPS );

  // Enable Ackpayloads for communicating info back (RPM data, etc)
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.writeAckPayload(1, &newDurationUs, sizeof(newDurationUs));

  NVIC_SET_PRIORITY(RADIO_IRQ, RADIO_PRIORITY); // set the priority so it isn't too late/take too much cpu time

  radio.startListening();
  delay(200);
  if (radio.isChipConnected()) {
    Serial.println("Radio is connected");
    radio.printPrettyDetails();
  } else {
    Serial.println("Error, radio not available");
  }
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), radioIsr, FALLING); // IRQ pin goes LOW when it has something to say

}


//  *************************
// *******  Animation  *******
//  *************************

// Animation Ideas:
// Worm (mom)
// Solid-color pulse
// Fireworks
// Oscillating rings

RainbowWheel myFlash;

Demo* anim = &myFlash; // Set this to the desired animation object

inline void animationSetup() {
  anim->setup(displayBuffer, rowSize, ringSize);
}

inline void animationUpdate(uint16_t keys, uint16_t diff) {
  if (diff != NO_KEY_CHANGE)
    anim->processKeypress(keys,diff);
  anim->nextFrame(displayBuffer, rowSize, ringSize);
}

void setup() {
  Serial.begin(9600);

  noteSetup();
  Serial.println("Setup note pins");
  ledSetup();
  Serial.println("Setup LED drivers");
  testFlash();
  Serial.println("Tested LED drivers");
  radioSetup();
  Serial.println("Setup radio");

  magSetup();
  Serial.println("Setup magnet sensor");
  animationSetup();
  Serial.println("Setup display buffer");
  Serial.println("finished setup");
}


// This loop is set up like an event loop of a game
// animationUpdate() is where the graphics happen
// looking at keys/responding to input should be done
// in this loop

void loop() {
  uint16_t keys = NO_KEY_CHANGE, diff = NO_KEY_CHANGE;
  // Get input
  if (keyBufferNotEmpty()) {
    keys = popKey();
    diff = processKeys(keys);
    audioUpdate(keys,diff); // update frequencies of PWM
// Testing keypresses + LEDs
//    uint16_t index = (uint16_t)log(diff)/log(2); 
//    tlc.writeGsBufferSPI16((uint16_t*)(colorPalette + index), 3, NUM_TLCS);
  }
  animationUpdate(keys,diff);
}
