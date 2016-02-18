/*
Hardware version of midi sequencer


KEYS; pin mcp 0t/m7 & 8 t/m 12
OLED; SPI 128x64(col2-col129) SH1106,Like HeiTec 1.3' SPI OLED  SCK = 13 SCL = 11 CS = 24 (CS Don't care) D/C(A0)= 9 RST =12
enc pushbuttons; mcp 13 t/m 15
normal pushbuttons; pin 20 t/m 23
pwm menuled; 3 color; pin 10,25,32
pwm playpause led; 3 color; pin 27,31


7 tlc5940;
GSCLK; 5
XLAT;3
BLANK;4
SIN;6
SCLK;7

*/

// TODO:
//Hardware;
//

//Software;
// correct for better precision encoders

#include <Keypad.h>
#include <MIDI.h>
#include <Encoder.h>
#include <MIDIElements.h>
//#include <ShiftRegister74HC595.h>
#include <Keypad_MC17.h>    // I2C i/o library for Keypad
#include <Wire.h>           // I2C library for Keypad_MC17
#include "U8glib.h"         // OLED screen
#include "Tlc5940.h"        // All  PWM Leds located sat the keys


//**********************************************Keyboard**********************************
#define I2CADDR 0x20        // address of MCP23017 chip on I2C bus

const byte ROWS = 8; //8 rows
const byte COLS = 5; //5 columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {

  {'8', '@', 'H', 'P'},       // G1, D#2, B2, G3
  {'7', '?', 'G', 'O'},     // F#1, D2, A#2, F#3
  {'6', '>', 'F', 'N'},  // F1, C#2, A2, F3
  {'5', '=', 'E', 'M', 'U'},  // E1, C2, G#2, E3, C4

  {'4', '<', 'D', 'L', 'T'},  // D#1, B1, G2, D#3, B3
  {'3', ';', 'C', 'K'},     /// D1, A#1, F#2, D3, **de hoge A# (R) is kaduuk
  {'2', ':', 'B', 'J', 'R'}, // C#1 , A1 , F2 , C#3, A3
  {'1', '9', 'A', 'I', 'Q'} // C1 , G#1, E2 , C3, G3

};

byte rowPins[ROWS] = {7, 6, 5, 4, 3, 2, 1, 0};   //connect to the row pinouts of the keypad   original:  {7, 6, 5, 4, 3, 2, 1, 0};
byte colPins[COLS] = {12, 11, 10, 9, 8};        //connect to the column pinouts of the keypad original : {12, 11, 10, 9, 8};

// modify constructor for I2C i/o
Keypad_MC17 kpd = Keypad_MC17( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);
int currentKey;  // houdt altijd bij welke noot op dat moment is ingedrukt

//***********OLED

U8GLIB_SH1106_128X64 u8g(13, 11, 24, 9, 12);    // SPI 128x64(col2-col129) SH1106,Like HeiTec 1.3' SPI OLED    //in  mijn setup; SCK = 13 , SCL = 11, CS = 10, d/c = 9, RST =  12
#define MENU_ITEMS 4
const char *menu_strings[MENU_ITEMS] = { "Sequencer Mode", "Stepview Mode", "Keyboard", "Arpeggiator"  };
uint8_t menu_current = 0;
uint8_t menu_redraw_required = 0;

#define SETUPMENU_ITEMS 3
const char *setupmenu_strings[SETUPMENU_ITEMS] = { "Steplength", "Shuffle", "Midichannel"  };
uint8_t setupmenu_redraw_required = 0;
bool showMenu = 1;           // = 0 in default mode, 1 in encodermode
String stringBuffer;
int valueBuffer;

int ledColor[8][3] = {
  {23, 0, 0},   // Red
  {23, 1, 0},// Orange
  {54, 4, 0},// Yellow
  {28, 0, 3},// Purple
  {25, 0, 1},//   Pink
  {0, 19, 0},//    Green
  {0, 19, 3},//   Cyan
  {0, 0, 38}//    Blue
};

const byte RED = 0;
const byte ORANGE = 1;
const byte YELLOW = 2;
const byte PURPLE = 3;
const byte PINK = 4;
const byte GREEN = 5;
const byte CYAN = 6;
const byte BLUE = 7;


//  PWMLEDs
int ledBrightness = 50;
int ledAdressbuffer[37][3] = {0};
int ledAdressOldbuffer[37][3] = {0};

int ledAdress[37][3] = {
  {0, 1, 2},
  {3, 4, 5},
  {6, 7, 8},
  {9, 10, 11},
  {12, 13, 14}, //15 eruit
  {16, 17, 18},
  {19, 20, 21},
  {22, 23, 24},
  {25, 26, 27},
  {28, 29, 30},
  {32, 33, 34},
  {35, 36, 37},
  {38, 39, 40},
  {41, 42, 43},
  {44, 45, 46},
  {48, 49, 50},
  {51, 52, 53},
  {54, 55, 56},
  {57, 58, 59},
  {60, 61, 62},

  {63, 47, 31}, // rij 21 met   31,47,63
  {64, 65, 66},
  {67, 68, 69},
  {70, 71, 72},
  {73, 74, 75},
  {76, 77, 78},
  {79, 80, 81},
  {82, 83, 84},
  {85, 86, 87},
  {88, 89, 90},
  {91, 92, 93},
  {94, 95, 96},
  {97, 98, 99},
  {100, 101, 102},
  {103, 104, 105},
  {106, 107, 108},
  {109, 110, 111}
};

// menu & playpause led
int menuLed[3] = {27, 32, 25};            // PWM :mode led geeft aan of sequencer speelt, pauzed of non actief is
int modeLed[2] = {31, 10};       // 10=  NC 31 = rood                //** PWM :menu Led geeft met fancy kleuren aan welk menu actief is

// ** encoders
Encoder enc1(8, 2);     // far left setup encoder
Encoder enc2(15, 14);   // volume encoder
Encoder enc3(16, 17);  // note length encoder




unsigned long int encOld[3] = {0, 0, 0};
unsigned long int encNew[3] = {999, 999, 999};

const byte RIGHT = 0;
const byte LEFT = 1;
//**********************************************************************

// Midi sync stuff //
byte counter;
const byte  CLOCK = 248;
const byte  START = 250;
const byte  CONTINUE = 251;
const byte STOP = 252;


////////////button debounce ///////////////
int but[4] = {23, 22, 21, 20};                  // de 4 drukknoppen
#define DFE(signal, state) (state=((state<<1)|(signal&1))&15)==8      // check if button pressed function
#define DRE(signal, state) (state=((state<<1)|(signal&1))&15)==7
int buttonState [5];

word portState;             // encoder pushbuttons at mcp chip
bool encPres[3] = {false};


//// sequencer steps & notes ////
unsigned long timerOld[127];    // start position note, second array is for set notelength
unsigned long timerNew;    // end position note
int timerEnc;
int timeLength = 2000;     // the total time of display of encoder value after it has been adjusted
int noteLengthBuffer[127];      // stores notelengths of played notes

int stepCounter = 1;             // determines what the play position is
int stepSelect = 1;              // determines what the note edit position is
int stepLength = 16;             // determines amounts of steps

int arpLength = 0;              // determines amount of arp steps

boolean sequencerPaused = true; // pause or play, controlled by external software
boolean Pause = true;           // pause or play, triggered by controller pause button
int Mode;                   // the workmode the sequencer is in
int shuffle   = 0;
int midiChannel = 1;

const byte SEQVIEW = 0;
const byte STEPVIEW = 1;
const byte KEYS = 2;
const byte ARP = 3;
const byte STEPLENGTH = 4;
const byte SHUFFLE = 5;
const byte MIDICHANNEL = 6;

// mode 0; sequencerview|  mode 1; stepview|  mode 2; keyboard |
// mode 3; arpeggiator | mode 4; setup steplength |
//mode 5; setup shuffle | mode 6; setup midiChannel |

byte notes[32][10][5] = {0};             // 32 kolommen, 10 rijen, 5 lagen : 0, mute, 1, velocity, 2, length, 3 pitch, 4 octave
byte arpNotes[33][5] = {0};             // 32 steps, 5 lagen 0, mute, 1, velocity, 2, length, 3 pitch, 4 octave
byte arpSequence = 0;

const byte MUTE = 0;
const byte VELOCITY = 1;
const byte LENGTH = 2;
const byte PITCH = 3;
const byte OCTAVE = 4;

//////// ENABLE OR DISABLE DEBUG
#define Sprintln(a)  (Serial.println(a))


void setup() {

  for (int i = 0 ; i < 2; i++)  {
    pinMode(modeLed[i], OUTPUT);     //  de analogwrite Leds als output
  }
  for (int i = 0 ; i < 3; i++)  {
    pinMode(menuLed[i], OUTPUT);     //  de analogwrite Leds als output
  }
  for (int i = 0 ; i < 4; i++)  {
    pinMode(but[i], INPUT_PULLUP);   //** de drukknoppen zijn high tenzij ze ingedrukt zijn
  }
  kpd.begin( );            // also starts wire library
  kpd.setDebounceTime(2);
  Tlc.init();

  usbMIDI.setHandleRealTimeSystem(RealTimeSystem); // step sequencer timing
  usbMIDI.setHandleNoteOff(OnNoteOff); //set event handler for note off
  usbMIDI.setHandleNoteOn(OnNoteOn); //set event handler for note on
  Serial.begin(9600);

  updateLed(stepCounter);
  updateOLED();

}

/////// LOOP ///////////////
void loop() {
  usbMIDI.read();    // sync midi clockm

  if (kpd.getKeys())  {
    // check if keys have been pressed
    keyPressed();
  }

  ////// buttonread and debug
  for (int m = 0; m < 4; m++) if (DFE(digitalRead((but[m])), buttonState[m])) {   //**straks uitbreiden naar 7
      buttonRead(m);
    }

  portState = kpd.pinState_set();       // read pushbuttons that are located on  mcp pins GPB5 t/m GPB7
  //Sprintln(portState);
  if (portState == 49151) {
    encPres[1] = true;
    Sprintln("knopje 1");
  }
  if (portState == 32767) {
    encPres[0] = true;
    Sprintln("knopje 0");
  }
  if (portState == 57343) {
    encPres[2] = true;
    Sprintln("knopje 2");
  }

  else {
    buttonState[4] = 0;
  }

  if (DRE(encPres[2], buttonState[4])) {   //**straks uitbreiden naar 7
    buttonRead(5);
  }

  ///////----- Encoder read and debug

  encNew[0] = enc3.read();
  encNew[1] = enc2.read();
  encNew[2] = enc1.read();

  for (int i = 0; i < 2; i++)   if (encNew[i] < encOld[i]  | encNew[i] > encOld[i]  )  {

      if (encNew[i] > encOld[i]) {
        /// encoder moves right
        encAdjust(i, RIGHT);      // send encoder rotation and number
      }

      ///// encoder moves left
      if (encNew[i] < encOld[i]) {
        encAdjust(i, LEFT);      // send encoder rotation and number
      }
      encOld [i] = encNew [i];
    }

  if ((encNew[2] < (encOld[2] - 2))   | (encNew[2] > (encOld[2] + 2))  )  {
    if (encNew[2] > encOld[2]) {
      /// encoder moves right
      encAdjust(2, RIGHT);
    }
    if (encNew[2] < encOld[2]) {
      // encoder moves left
      encAdjust(2, LEFT);
    }
    encOld [2] = encNew [2];
  }

  // Stop note when the notelength has been exceeded
  timerNew = millis();

  for (int i = 0; i < 127; i++) {
    if (noteLengthBuffer[i] > 0) {                     // only look for notes, currently playing
      if (timerNew - timerOld[i] > (noteLengthBuffer[i] * 8 )) {     //  noteLengthBuffer * 8 to overcome max 255 notelength limit
        usbMIDI.sendNoteOff((i + 35), 0, midiChannel);            // stop this certain note
        noteLengthBuffer[i] = 0;                         // if note has been stopped once, not necessary to do this proces again
        Sprintln("note timer stopt noot");
      }
    }
  }

  if ((timerEnc != 0) && (timerNew > (timerEnc + timeLength))) {
    timerEnc = 0;
    showMenu = 1;
    updateOLED();
    Sprintln("geef weer menu weer");
  }
}
//////// end loop

///// update oled screen
void updateOLED() {
  /// decide wat to draw on OLED screen

  u8g.firstPage();
  do {
    usbMIDI.read();
    if (showMenu == 1) {
      drawMenu();
    }
    /* if (showMenu == 1 && Mode == STEPVIEW) {                  // hier nog aan werken
       draw(stringBuffer, valueBuffer);
     }
     */else {
      draw(stringBuffer, valueBuffer);
    }
  } while ( u8g.nextPage() );

}

//// draw Oled menu
void drawMenu(void) {
  menu_current = Mode;
  uint8_t i, h;
  u8g_uint_t w, d;

  u8g.setFont(u8g_font_ncenB10);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();

  h = u8g.getFontAscent() - u8g.getFontDescent();
  w = u8g.getWidth();

  if (Mode <= 3) {
    for ( i = 0; i < MENU_ITEMS; i++ ) {
      d = (w - u8g.getStrWidth(menu_strings[i])) / 2;
      u8g.setDefaultForegroundColor();
      if ( i == menu_current ) {
        u8g.drawBox(0, i * h + 1, w, h);
        u8g.setDefaultBackgroundColor();
      }
      u8g.drawStr(d, i * h, menu_strings[i]);
    }
  }
  if (Mode >= 4) {
    for ( i = 0; i < SETUPMENU_ITEMS; i++ ) {
      d = (w - u8g.getStrWidth(setupmenu_strings[i])) / 2;
      u8g.setDefaultForegroundColor();
      if ( i == (menu_current - 4)) {    // correct current menu for the 4-6 setup mode
        u8g.drawBox(0, i * h + 1, w, h);
        u8g.setDefaultBackgroundColor();
      }
      u8g.drawStr(d, i * h, setupmenu_strings[i]);
    }
  }

}

void draw(String i, int n) {


  // graphic commands to redraw the complete screen should be placed here
  u8g.setFont(u8g_font_helvR18);                  // check ugglib font size pages
  u8g.setPrintPos(20, 20);

  u8g.print(i);
  u8g.setPrintPos(40, 42);

  u8g.print(n);
  u8g.drawTriangle(0, 64, n, 64, n, (64 - (n / 4)));

  //u8g.drawTriangle(0, 64, 128, 64, fadeValue, (fadeValue / 4));            // teken een driehoek met 3 punten. Voor elk de x,y coordinaten vanaf linker tophoek
  // een driehoek die waarde weergeeft is bijv;    u8g.drawTriangle(0, 64, 128, 64, a, (a/4));   waar a de variable waarde is

}


//***** Send MIDI ********
void midiNote(int i, int j) {// int i to determine noteposition, int j to determine to send note on or off message
  if (j == 1) {
    Sprintln("stepCounter");
    Sprintln(stepCounter);
    if (Mode < ARP) {
      for (int q = 0; q < 10; q++)                      // usbmidi.sendnoteon(pitch, velocity, midichannel)
      {
        if (notes[stepCounter][q][VELOCITY] > 0 ) {
          int noteVelBuffer[10];
          noteVelBuffer[q] = notes[stepCounter][q][VELOCITY];

          int notePitchBuffer[10];
          notePitchBuffer[q] = notes[stepCounter][q][PITCH];
          noteLengthBuffer[notePitchBuffer[q]] = notes[stepCounter][q][LENGTH];    // stores the notelength of the played note in the buffer with the corresponding pitch

          if (notePitchBuffer[q] > 0 && notePitchBuffer[q] <= 127) {
            usbMIDI.sendNoteOn(((notePitchBuffer[q]) + 35), noteVelBuffer[q] , midiChannel);    // add in octave correction to pitch
            timerOld[notePitchBuffer[q]] = timerNew;

            Sprintln("noot gespeeld");
            Sprintln(notePitchBuffer[q]);

          }
        }
      }

    }
    if (Mode == ARP && arpNotes[stepCounter][PITCH] > 0 && arpNotes[stepCounter][PITCH] <= 127) {
      usbMIDI.sendNoteOn((arpNotes[stepCounter][PITCH] + 35) , arpNotes[stepCounter][VELOCITY] , midiChannel);   // add in octave correction to pitch
      timerOld[arpNotes[stepCounter][PITCH]] = timerNew;
      noteLengthBuffer[arpNotes[stepCounter][PITCH]] = arpNotes[stepCounter][LENGTH];
      Sprintln("noot gespeeld");
      Sprintln(arpNotes[stepCounter][PITCH]);

    }
  }

  if (j == 0) {
    Sprintln("noot gestopt");
    for (int q = 0; q < 10; q++)                      // usbmidi.sendnoteon(pitch, velocity, midichannel)
    {
      int noteVelBuffer[10];
      noteVelBuffer[q] = notes[stepCounter][q][VELOCITY];

      int notePitchBuffer[10];
      notePitchBuffer[q] = notes[stepCounter][q][PITCH];

      if (notePitchBuffer[q] > 0 && notePitchBuffer[q] <= 127) {
        //    usbMIDI.sendNoteOff((notePitchBuffer[q] + 35), noteVelBuffer[q] , 1);      // add in octave correction to pitch
      }
    }
  }
}

///// Do things with keypresses ***************o
void keyPressed() {
  for (int i = 0; i < LIST_MAX; i++) // Scan the whole key list.
  {
    if ( kpd.key[i].stateChanged )   // Only find keys that have changed state.
    {
      Sprintln(kpd.key[i].kchar);
      if ((kpd.key[i].kchar >= '0') && (kpd.key[i].kchar <= 'U')) {
        currentKey = (kpd.key[i].kchar - 48);
        switch (kpd.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED

          case PRESSED:
            Sprintln("Key pressed ");
            Sprintln("char ");
            Sprintln(kpd.key[i].kchar);
            Sprintln("stepSelect ");
            Sprintln(stepSelect);
            Sprintln("currentKey pressed ");
            Sprintln(currentKey);

            Sprintln("Mode ");
            Sprintln(Mode);
            if (Mode == SEQVIEW ) {
              if (currentKey <= stepLength) {                   // stepLength invoegen //in sequencerview, if selected step is equal to selected note, mute that note

                if (stepSelect == currentKey) {
                  Sprintln("step muted ");
                  Sprintln(stepSelect);
                  /*
                  for (int q = 0 ; q < 10; q++) {
                    notes[stepSelect][q][VELOCITY] = 0;   // reduce the velocity to 0, to mute sounds
                  }
                  */
                }

                else {                                        // in sequencerview, if step is not selected, select now
                  stepSelect = currentKey;                    // select the step which is pressed
                  Sprintln("stepSelect pressed and becomes ");
                  Sprintln(stepSelect);
                }
              }
            }

            if (Mode == STEPVIEW ) {
              if (currentKey <= 37) {
                bool found = false;
                for (int q = 0 ; q < 10; q++) {
                  if (notes[stepSelect][q][PITCH] == currentKey) {   //if the current selected note corresponds with an already played note
                    removeNote(currentKey);
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  addNote(currentKey);
                }
              }
            }
            if  (Mode == KEYS ) {
              usbMIDI.sendNoteOn((kpd.key[i].kchar + 11), 100, midiChannel);
            }

            if (Mode == ARP) {
              if (currentKey <= 37) {
                bool found = false;
                for (int q = 0 ; q <= 32; q++) {
                  if (arpNotes[q][PITCH] == currentKey) {   //if the current selected note corresponds with an already played note
                    arpNotes[q][PITCH] = 0;
                    arpLength --;
                    found = true;
                    sortArpNotes(arpLength);
                    break;
                  }
                }
                if (!found) {
                  addNote(currentKey);
                }
              }
            }

            if ((Mode == STEPLENGTH) && (currentKey > 0) && (currentKey < 33)) {
              stepLength = currentKey;
            }
            if ((Mode == SHUFFLE) && (currentKey > 0) && (currentKey < 5)) {
              shuffle = (currentKey - 1); // shuffle can also be 0, minimal key is 1
            }
            if ((Mode ==  MIDICHANNEL) && (currentKey > 0) && (currentKey < 17)) {
              midiChannel = currentKey;
            }


            break;   // end of PRESSED statement

          case HOLD:

            if (Mode == SEQVIEW ) {
              if (currentKey <= stepLength ) {
                stepSelect = currentKey;      // select the step, and go to stepview mode
                Mode = STEPVIEW;
                Sprintln("stepSelect HOLD ");
                Sprintln(stepSelect);
              }
            }
            Sprintln("HOLD");

            break;

          case RELEASED:
            currentKey = 0;

            if  (Mode == KEYS ) {
              usbMIDI.sendNoteOff((kpd.key[i].kchar + 11), 0, midiChannel);
            }
            break;

          case IDLE:
            break;
        }
        updateLed(stepCounter);
        updateOLED();
      }
    }
  }
  if (currentKey = ! 0) {
    currentKey = 0;
  }
}


// Take action with buttons **************
void buttonRead(int m) {

  if (m == 0) {/// if button 0 is pressed,pause sequencer
   
    if (sequencerPaused == true) {    // play pause gedoe
      counter = 0;
      stepCounter = 1;
      sequencerPaused = false;
      Sprintln("play");
    
    }
    else {
      sequencerPaused = false;
      Sprintln("pause");
      Sprintln("stepCounter ");
      Sprintln(stepCounter);
    }
    Pause = ! Pause; // reverse state of Pause
  }

  if (m == 1) {
    if (Mode == SEQVIEW ) { // if button 1 is pressed; if in sequencer mode(0); go to stepview mode(1),
      Mode = STEPVIEW ;
      Sprintln("button pressed, STEPVIEW");
    }
    else if (Mode == STEPVIEW) {
      Mode = SEQVIEW ;
      Sprintln("button pressed, SEQVIEW");
    }

    if (Mode == ARP) {
      for (int i = 0; i <= 32; i++) {
        arpNotes[i][PITCH] = 0;
      }
      arpLength = 0;
    }
  }

  if (m == 2) {
    if ((Mode <= STEPVIEW) && (stepSelect > 1)) { // decrease stepSelect when in sequencerview or stepview
      stepSelect --;
      Sprintln("stepSelect ");
      Sprintln(stepSelect);
    }
    if (Mode == ARP && arpSequence > 0) {
      arpSequence --;
      sortArpNotes(arpLength);
    }
  }

  if (m == 3) {
    if ((Mode <= STEPVIEW) && (stepSelect < stepLength))  { // increase stepSelect when in sequencerview or stepview
      stepSelect ++;
      Sprintln("stepSelect ");
      Sprintln(stepSelect);
    }
    if (Mode == ARP && arpSequence < 1) {
      arpSequence++;
      sortArpNotes(arpLength);
    }
  }

  if (m == 5) {
    if (Mode < 4) {
      Mode = 4;
      Sprintln("Mode");
      Sprintln(Mode);
    }
    else {
      Mode = 0;
    }
  }
  updateLed(stepCounter);
  updateOLED();
}


// Add a note
void addNote(int n) {     //  n is the key that is pressed

  int found = -1;               // the counter that goes looking for a free spot

  if (Mode == ARP) {
    for (int i = 1; i <= 32; i++) {   // slot 0 up to 10(max assigned array size at this moment)
      if (arpNotes[i][PITCH] == 0) {   // if a free spot is found
        found = i;
        Sprintln("arpnoot toegevoegd aan positie ");
        Sprintln(found);
        break;
      }
    }
    if (found >= 0) {
      arpNotes[found][MUTE] = 0;
      arpNotes[found][VELOCITY] = 80;
      arpNotes[found][LENGTH] = 40;
      arpNotes[found][OCTAVE] = 3;
      arpNotes[found][PITCH] = n;
      Sprintln("arpnote added ");
      arpLength ++;
      Sprintln("arplength");
      Sprintln(arpLength);
      sortArpNotes(arpLength);
    }
  }

  else {
    for (int i = 0; i < 10; i++) {   // slot 0 up to 10(max assigned array size at this moment)
      if (notes[stepSelect][i][PITCH] == 0) {   // if a free spot is found
        found = i;
        Sprintln("toegevoegd aan positie ");
        Sprintln(found);
        break;
      }
    }

    // slot 0 up to 10(max assigned array size at this moment)   // all values are bytes; max 255 min 0;
    // add en remove note, samenvoegen in mutate note.
    if (found >= 0) {
      int q = found;
      notes[stepSelect][q][MUTE] = 0;   // mute
      notes[stepSelect][q][VELOCITY] = 80;   // velocity
      notes[stepSelect][q][LENGTH] = 40;   // length
      notes[stepSelect][q][OCTAVE] = 3;   // octave 3; is the default position
      notes[stepSelect][q][PITCH] = n;  // pitch
      Sprintln("note added ");
      Sprintln(notes[stepSelect][q][4]);
    }
  }
  updateLed(stepCounter);
}

void removeNote(int n) {       // n is the key that is pressed
  int found = -1;
  for (int i = 0; i < 10; i++) {
    if (notes[stepSelect][i][PITCH] == n) {    //if there is an array row with this certain key played
      found = i;
      break;
    }
  }
  if (found >= 0) {                              // use this key, and
    int q = found;
    for (int m = 0; m < 5; m++) {
      notes[stepSelect][q][m] = 0;           // delete all data
    }
    Sprintln("Note removed ");
    Sprintln(notes[stepSelect][q][0]);
  }
  updateLed(stepCounter);

}

// Update all visual things *************
void updateLed(int x) {               // change to write shift register and the modeled
  /*
   const byte RED = 0;
  const byte ORANGE = 1;
  const byte YELLOW = 2;
  const byte PURPLE = 3;
  const byte PINK = 4;
  const byte GREEN = 5;
  const byte CYAN = 6;
  const byte BLUE = 7;
   */
  Tlc.clear();    //sets all the grayscale values to zero
  for (int i = 0 ; i < 3; i ++) {      // set all menuledpins to zero
    digitalWrite(menuLed[i], LOW);
  }


  ///
  if (Mode == SEQVIEW) {
    digitalWrite(menuLed[1], 1);
    digitalWrite(menuLed[0], 1);

    for (int i = 0; i < stepLength; i = i + 4) {   //write every first beat of measure high
      Tlc.set(ledAdress[i][2], 1);
      Tlc.set(ledAdress[i][1], 1);
    }

    Tlc.set(ledAdress[stepCounter - 1][1], 19 );   // write the current stepCounter value high
    ledAdressOldbuffer[stepCounter - 1][1] = 19;

    Tlc.set(ledAdress[stepSelect - 1][2], 30 );   // write the current stepSelect value high
    ledAdressOldbuffer[stepSelect - 1][2] = 30;

    for (int i = 0; i < stepLength; i++) {
      if (notes[i][0][1] > 0) {
        Tlc.set(ledAdress[i - 1][0], 23 );   // write steps that contain notes high
        ledAdressOldbuffer[i - 1][0] = 23;
      }
    }
  }
  if (Mode == STEPVIEW) {
    digitalWrite(menuLed[0], 1);
    for (int r = 0 ; r < 10; r ++) {
      if ((notes[stepSelect][r][PITCH] > 0) && (notes[stepSelect][r][PITCH] <= 37)) {
        Tlc.set(ledAdress[(notes[stepSelect][r][PITCH]) - 1][0], 23 );   // write the current written notes value high
        ledAdressOldbuffer[(notes[stepSelect][r][PITCH]) - 1][0] = 23;
      }
    }
  }
  if (Mode == ARP) {
    for (int r = 0 ; r <= 32; r ++) {
      if (arpNotes[r][PITCH] > 0) {
        Tlc.set(ledAdress[arpNotes[r][PITCH] - 1][0], 23 );
      }
    }
  Tlc.set(ledAdress[arpNotes[stepCounter][PITCH] - 1][1], 1 );      // write the current played note high
  }
  
  if (Mode == KEYS) {
    digitalWrite(menuLed[1], HIGH);
  }
  if (Mode == STEPLENGTH) {

    for (int i = 7; i < 33; i = i + 8) {   //write every 8th step high
      Tlc.set(ledAdress[i][2], 1);
      Tlc.set(ledAdress[i][1], 1);
    }

    Tlc.set(ledAdress[stepLength - 1][0], 23); // write the stepLength high
    ledAdressOldbuffer[stepLength - 1][0] = 23;

    Tlc.set(ledAdress[stepLength - 1][1], 19);
    ledAdressOldbuffer[stepLength - 1][1] = 19;
  }
  if (Mode == SHUFFLE) {
    Tlc.set(ledAdress[shuffle][2], 30); // write the shuffle high
    ledAdressOldbuffer[shuffle][2] = 30;

    Tlc.set(ledAdress[shuffle][1], 19);
    ledAdressOldbuffer[shuffle][1] = 19;
  }
  if (Mode ==  MIDICHANNEL) {
    Tlc.set(ledAdress[midiChannel - 1][0], 23); // write the midiChannel high
    ledAdressOldbuffer[midiChannel - 1][0] = 23;

    Tlc.set(ledAdress[midiChannel - 1][2], 30);
    ledAdressOldbuffer[midiChannel - 1][2] = 30;

  }
  Tlc.update();
}

void updateTlc() {     ///// Reset the TLC and update writing to pin if necessary////
  Sprintln("updateTlc");

  for (int a = 0 ; a < 37; a++) {
    for (int b = 0 ; b < 3; b++) {

      if (ledAdressOldbuffer[a][b] != ledAdressbuffer[a][b]) {
        Tlc.set(ledAdress[a][b], ledAdressbuffer[a][b]);
      }
      ledAdressOldbuffer[a][b] = ledAdressbuffer[a][b];
    }
  }
  Tlc.update();
}

void encAdjust(int i, int n) {     // i; welke encoder command wordt gestuurd, n; rechtsom(0) of linksom(1)
  Sprintln("encAdjust ");
  Sprintln(i);
  Sprintln(n);

  if ((Mode <= STEPVIEW ) && (currentKey == 0)) { // if sequencer mode or step mode is activated and no key pressed

    for (int r = 0 ; r < 10; r++) {
      if (notes[stepSelect][r][PITCH] > 0) {   // are there actually notes present
        if (n == RIGHT) {  // turn right?

          if ((i == 0) && (notes[stepSelect][r][VELOCITY] < 121)) {  //127 max velocity number
            notes[stepSelect][r][VELOCITY] += 6;
            Sprintln("Velocity meer ");
            stringBuffer = "Velocity";
            valueBuffer = notes[stepSelect][0][VELOCITY];
          }
          if ((i == 1) && (notes[stepSelect][r][LENGTH] < 121)) {  // some length number as limit
            notes[stepSelect][r][LENGTH] += 6;
            Sprintln("Length meer ");
            stringBuffer = "noteLength";
            valueBuffer = notes[stepSelect][0][LENGTH];
          }
          if ((i == 0) && (encPres[0] == true) && (notes[stepSelect][r][OCTAVE] < 5)) {  // 5 max octave number
            notes[stepSelect][r][OCTAVE] ++;
            stringBuffer = "Octave";
            stringBuffer = "Octave";
            valueBuffer = notes[stepSelect][0][OCTAVE];
          }
        }
        if (n == LEFT) {   // turn left

          if ((i == 0) && (notes[stepSelect][r][VELOCITY] > 5)) {  //0 min velocity number
            notes[stepSelect][r][VELOCITY] -= 6;
            Sprintln("Velocity minder ");
            stringBuffer = "Velocity";
            valueBuffer = notes[stepSelect][0][VELOCITY];
          }
          if ((i == 1) && (notes[stepSelect][r][LENGTH] > 5)) { // min length number as limit
            notes[stepSelect][r][LENGTH] -= 6;
            Sprintln("Length minder ");
            stringBuffer = "noteLength";
            valueBuffer = notes[stepSelect][0][LENGTH];
          }
          if ((i == 0) && (encPres[0] == true) && (notes[stepSelect][r][OCTAVE] > 1)) {  // 1 min octave number
            notes[stepSelect][r][OCTAVE] --;
            stringBuffer = "Octave";
            stringBuffer = "Octave";
            valueBuffer = notes[stepSelect][0][OCTAVE];
          }
        }
        if (i < 2) {
          showMenu = 0;     // stop displaying menu, some encoder has been turned
          timerEnc = millis();
        }
      }
    }
  }
  /*
  else if (Mode == STEPVIEW) { // there is a key pressed, adjust specific note variables
     int found = -1;               // search for i, the counter that goes looking for a spot with this key
     for (int q = 0; q < 10; q++) {   // slot 0 up to 10(max assigned array size at this moment)
       if (notes[stepSelect][i][PITCH] == currentKey) { // where 4, pitch is equal to a stored note
         found = q;
         break;
       }
     }
     if (found >= 0) {
       int q = found;
       if (n == RIGHT) {   // turn right
         if ((i == 0) && (notes[stepSelect][q][VELOCITY] < 127)) {  //127 max velocity number
           notes[stepSelect][q][VELOCITY] ++;
           Sprintln("specifieke velocity noot meer ");
         }
         if ((i == 1) && (notes[stepSelect][q][LENGTH] < 255)) {  // some length number as limit
           notes[stepSelect][q][LENGTH] ++;
           Sprintln("specifieke length meer ");
         }
         if ((i == 0) && (encPres[0] == true) && (notes[stepSelect][q][OCTAVE] < 5)) { // 3 max octave number
           notes[stepSelect][q][OCTAVE] ++;
         }
       }

       if (n == LEFT) {   // turn left
         if ((i == 0) && (notes[stepSelect][q][VELOCITY] > 0)) {  //0 min velocity number
           notes[stepSelect][q][VELOCITY] --;
           Sprintln("specifieke Velocity noot minder ");
         }
         if ((i == 1) && (notes[stepSelect][q][LENGTH] > 0 )) { // min length number as limit
           notes[stepSelect][q][LENGTH] --;
           Sprintln("specifieke length minder ");
         }
         if ((i == 0) && (encPres[0] == true) && (notes[stepSelect][q][OCTAVE] > 1)) {  // -3 min octave number
           notes[stepSelect][q][OCTAVE] --;
         }
       }
       if (i < 2) {
         showMenu = 0;     // stop displaying menu, some encoder has been turned
         timerEnc = millis();
       }
     }
   }
  */
  // change the viewmode and setupmode ******
  if (i == 2) {
    //if ((timerNew - encTimer) > encDebounce) {
    if (Mode <= ARP) {
      if ((n == RIGHT) && (Mode < ARP)) {
        Mode ++;
        Sprintln("Mode");
        Sprintln(Mode);
      }
      if ((Mode > SEQVIEW) && (n == LEFT)) {
        Mode --;
      }
    }
    if (Mode > ARP) {
      if ((n == RIGHT) && (Mode < MIDICHANNEL)) {
        Mode ++;
        Sprintln("Mode");
        Sprintln(Mode);

      }
      if ((Mode > ARP) && (n == LEFT)) {
        Mode --;
      }
    }
    Sprintln("Mode");
    Sprintln(Mode);
    //timerEnc = 1;
    if (Mode == ARP) {
      stepCounter = 0;
    }
  }

  updateLed(stepCounter);
  updateOLED();
}

// Watch time *****************************
void RealTimeSystem(byte realtimebyte) {
  if ((Mode <= ARP ) && (realtimebyte == 248)) {
    counter ++;
    if ( counter == 12) {  // 8th notes stepswitch, counter resets
      counter = 0;
      if ((Pause == false) && (sequencerPaused == false)) {
        // Only play when clock is running
        stepCounter ++;
        if (Mode < ARP && stepCounter > stepLength) {   // if stepcounter reaches the maximum defined steps
          stepCounter = 1;
        }
        if (Mode == ARP && stepCounter > arpLength) {   // if stepcounter reaches the maximum defined steps, dependent on amoun of added notes
          stepCounter = 1;
        }
        midiNote(stepCounter, 1);
        updateLed(stepCounter);
        if (Mode == STEPVIEW && showMenu == 1) {     // TODO; draw steps in screen whenm in stepview mode
          draw("Step", stepCounter);
        }
      }
    }
    if (counter == (6 + shuffle)) {      // 16th note, counter does not reset
      if ((Pause == false) && (sequencerPaused == false)) {// Only play when clock is running
        stepCounter ++;

        if (Mode < ARP && stepCounter > stepLength) {  // if stepcounter reaches the maximum defined steps
          stepCounter = 1;
        }
        if (Mode == ARP && stepCounter > arpLength) { // if stepcounter reaches the maximum defined steps, dependent on amoun of added notes
          stepCounter = 1;
        }
        midiNote(stepCounter, 1);
        updateLed(stepCounter);
        if (Mode == STEPVIEW && showMenu == 1) {
          draw("Step", stepCounter);
        }
      }
    }
  }

  if (realtimebyte == START || realtimebyte == CONTINUE) {
    counter = 0;
    digitalWrite(modeLed[1], HIGH);
    digitalWrite(modeLed[0], LOW);
    Sprintln("start");
    sequencerPaused = false;
  }

  if (realtimebyte == STOP) {
    digitalWrite(modeLed[1], LOW);
    digitalWrite(modeLed[0], HIGH);
    Sprintln("stop");
    stepCounter = 1;
    sequencerPaused = true;
    Pause = true;
  }
  if (realtimebyte == START) {
    Pause = false;
  }
}

void OnNoteOn(byte channel, byte note, byte velocity) {
  // add all your output component sets that will trigger with note ons
}

void OnNoteOff(byte channel, byte note, byte velocity) {
  // add all your output component sets that will trigger with note ons
}

void sortArpNotes(int x) {

  int i;
  int j;
  int temp[5];

  switch (arpSequence) {
    case 0:
      for (j = 0; j < x; j++) {
        for (i = 1; i < (x - j); i++) {
          if (arpNotes[i - 1][PITCH] > arpNotes[i][PITCH]) {
            for (int n = 0; n < 5; n++) {
              temp[n] = arpNotes[i][n];
              arpNotes[i][n] = arpNotes[i - 1][n] ;
              arpNotes[i - 1][n]  = temp[n];
            }
          }
        }
      }
      break;
    case 1:
      for (j = (x - 1); j > 0 ; j--) {
        for (i = x; i > (0 + j); i--) {
          if (arpNotes[i + 1][PITCH] > arpNotes[i][PITCH]) {
            for (int n = 0; n < 5; n++) {
              temp[n] = arpNotes[i][n];
              arpNotes[i][n] = arpNotes[i + 1][n] ;
              arpNotes[i + 1][n]  = temp[n];
            }
          }
        }
      }
      break;
   }
  Sprintln("arpnotesequence");
  Sprintln(arpSequence);
  for (int y = 0; y > x; y++) {
    Sprintln(arpNotes[y][PITCH]);
  }
}


