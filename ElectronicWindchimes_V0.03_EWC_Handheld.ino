// Figure out which processor is being used and start setting up the serial
// for talking MIDI. Written by Adafruit. One line had to be commented out
// so this would work with the HUZZAH32 Feather board.
#if defined(__AVR_ATmega32U4__) || defined(ARDUINO_SAMD_FEATHER_M0) || defined(TEENSYDUINO) || defined(ARDUINO_STM32_FEATHER)
  #define VS1053_MIDI Serial1
#elif defined(ESP32)
//  HardwareSerial Serial1(2);
  #define VS1053_MIDI Serial1
#elif defined(ESP8266)
  #define VS1053_MIDI Serial
#endif

// Constants from Adafruit code
#define MIDI_NOTE_ON  0x90
#define MIDI_NOTE_OFF 0x80
#define MIDI_CHAN_MSG 0xB0
#define MIDI_CHAN_BANK 0x00
#define MIDI_CHAN_VOLUME 0x07
#define MIDI_CHAN_PROGRAM 0xC0


// Define the input pins
int tempoPot = A2;
int velocityPot = A3;
int randomizeChannelPin = 36;
int confirmChannelPin = 4;
int manualPlayPin = 21;
int onboardLED = 13;

// Create some useful variables
int currentNote = 60;
int tempoValue = 1;
int maxTempo = 600;
int minTempo = 25;
int velocityValue = 0;

int activeChannels = 0;
int runningChannels = 0;

boolean oldManualPlayInputValue = HIGH;
boolean manualPlayRisingEdge = LOW;
boolean manualPlayFallingEdge = LOW;
boolean oldRandomizeChannel = false;
boolean randomizeChannelInput = false;
boolean randomizeChannelRisingEdge = false;
boolean oldConfirmChannel = false;
boolean confirmChannelInput = false;
boolean confirmChannelRisingEdge = false;
boolean manualPlayInput= false;
int lowerThreshold = 48;
int upperThreshold = 72;

// Copy over some variables from old code
int noteDelta = 0;
int mainNote = 60;
int mainVelocity = 70;
int mainTempo = 500;
int mainBPM = 120;
long nextNoteIteration = 0;
int mainChannel = 0;
long timeThisCycle;

// We only need one scale and we don't have any chords.
const int minorPentatonicScale[] = {-12, -9, -8, -5, -2, 0, 3, 4, 7, 10, 12};

int prandomInstrument = 0;
int prandomPitchOffset = 0;
int prandomTimeOffset = 0;
int prandomPlayFrequency = 0;
int prandomDuration = 0;

// We are creating an array to contain all the notes which need to play and
// it will contain a slot for each channel, 0-15, although channel 0x9 (percussion), may
// need to be used in a different way.
// Note, Time on, Time off, Velocity, Expired
// 0000, 1111111, 22222222, 33333333, 4444444
long primaryNotes[16][5][2];
boolean evenOddNotePlaying = false;

// Each time a channel gets prandom values assigned to it, they
// get stored here. The instrument is assigned another way, but
// that will get prandomized another way.
// Velocity is on this list, but that value is set by the 
// potentiometer.
// Pitch offset, Time offset, Play frequency, Duration, Velocity, Instrument
// 000000000000, 11111111111, 22222222222222, 33333333, 44444444, 5555555555
int channelTweaks[16][6];

/////////////////////////////////////////////////
/////////2/4///H/O/U/R///E/N/G/I/N/E/E/R/////////
/////////////////////////////////////////////////

void setup(){
  // This delay is from the feather_midi example from Adafruit
  delay(1000);
  
  //  Set MIDI baud rate:
  VS1053_MIDI.begin(31250);
  Serial.begin(115200);

  // Pin 13 is the onboard LED.
  pinMode(onboardLED, OUTPUT);
  digitalWrite(onboardLED, LOW);

  // Declare the analog input purposes
  pinMode(tempoPot, INPUT);
  pinMode(velocityPot, INPUT);

  // Declare the digital input purposes. They already have pull-up resistors so
  // they do not need to use INPUT_PULLUP
  pinMode(randomizeChannelPin, INPUT);
  pinMode(confirmChannelPin, INPUT);
  pinMode(manualPlayPin, INPUT);
  
  // Send an "all notes off" command to each channel and
  // and set their volume levels to 0.
  panic();
  
  Serial.println("EWC_Handheld Running");
  for (int i = 0; i < 16; i++){
    midiSetChannelBank(i, 0x00);
    midiSetChannelVolume(i, 127);
  }
  midiSetInstrument(0, 4);
  channelTweaks[0][0] = 0;
  channelTweaks[0][1] = 0;
  channelTweaks[0][2] = 100;
  channelTweaks[0][3] = 10;
  channelTweaks[0][4] = mainVelocity;
}

void loop() {
  checkInputs();
//  printInputs();
  timeThisCycle = millis();
  
  if (randomizeChannelRisingEdge){
    randomizeChannel();
    printChannelInformation();
  }
  if (confirmChannelRisingEdge){
    activeChannels++;
    if (activeChannels == 9){
      activeChannels = 10;
    }
    randomizeChannel();
    printChannelInformation();
  }
  if (mainBPM != minTempo){
    // Scan the primaryNotes array for any notes which are ready to
    // play or stop. 
    for (int e=0; e <= 1; e++){
      for (int i=0; i <= activeChannels; i++){
        if (primaryNotes[e][i][4] > 0){
          // If the fourth value is two, that means the note
          // wants to stop when the time arrives.
          if ((primaryNotes[e][i][4] == 2) && (timeThisCycle >= primaryNotes[e][i][2])){
            midiNoteOff(i, primaryNotes[e][i][0], 0);
            primaryNotes[e][i][4] = 0;
          }
          // If the fourth value is one, that means the note
          // wants to play when the time arrives.
          if ((primaryNotes[e][i][4] == 1) && (timeThisCycle >= primaryNotes[e][i][1])){
            midiNoteOn(i, primaryNotes[e][i][0], primaryNotes[e][i][3]);
            primaryNotes[e][i][4] = 2;
          }
        }
      }
    }
  }
  
  // Generate the mainNote which will seed the rest of the playable notes
  if ((timeThisCycle >= nextNoteIteration) && (mainBPM != minTempo)){
    calculateMainNote();
    calculateChannel00();
    calculateChannels(evenOddNotePlaying);
    evenOddNotePlaying = !evenOddNotePlaying;
//    Serial.print("\n");
  }
  if ((manualPlayFallingEdge) && (mainBPM == minTempo)){
    midiNoteOff(mainChannel, mainNote, mainVelocity);
  }
  if ((manualPlayRisingEdge) && (mainBPM == minTempo)){
    // Do the same things as the tempo-controlled algorithm except
    // without turning off the previous note. That is handled elsewhere.
    noteDelta = random(0, 12);
    noteDelta = maintainNoteLimits(mainNote, noteDelta, lowerThreshold, upperThreshold);
    mainNote = mainNote + minorPentatonicScale[noteDelta];
    midiNoteOn(mainChannel, mainNote, mainVelocity);
  }
}
