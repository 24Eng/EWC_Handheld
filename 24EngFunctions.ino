// Ensure that the note about to be played is within the limits.
int maintainNoteLimits(int funStartingPoint, int funRequestedChange, int funLowerLimit, int funUpperLimit){
  int safelyRangedChange;
  boolean unaltered = true;
  int requestedNote = funStartingPoint + minorPentatonicScale[funRequestedChange];
  // Assess if the requested note needs to be raised to keep it above the lower threshold.
  if (requestedNote < funLowerLimit){
    // If the requested change is already greater than six, the limits were probably
    // changed recently so just keep going to a higher note. Else, add six to make the change positive.
    if (funRequestedChange > 6){
      safelyRangedChange = funRequestedChange;
      unaltered = false;
    }else{
      safelyRangedChange = 6 + funRequestedChange;
      unaltered = false;
    }
  }
  // Assess if the requested note needs to be lowered to keep it below the upper threshold.
  if ((requestedNote > funUpperLimit) && unaltered){
    // If the requested change is already less than six, the limits were probably
    // changed recently so just keep going to a lower note. Else, subtract six to make the change negative.
    if (funRequestedChange < 6){
      safelyRangedChange = funRequestedChange;
    }else{
      safelyRangedChange = funRequestedChange - 6;
    }
  }
  // For an unknown reason, the next print statement must be intact for
  // the code to run properly.
  // DO NOT DELETE THIS PRINTLN STATEMENT
  Serial.print("");
//  Serial.println(safelyRangedChange);
  // For an unknown reason, the previous print statement must be intact for
  // the code to run properly.
  // DO NOT DELETE THIS PRINTLN STATEMENT
  return safelyRangedChange;
}

void MIDICommand(int statusByte, int data1Byte, int data2Byte){
  VS1053_MIDI.write(statusByte);
  VS1053_MIDI.write(data1Byte);
  VS1053_MIDI.write(data2Byte);
}

void checkInputs(){
  // Read the input to update the tempo
  updateTempo();
  mainVelocity = analogRead(velocityPot);
  mainVelocity = map(mainVelocity, 0, 4095, 0, 127);
  channelTweaks[activeChannels][4] = mainVelocity;
  for (int e=0; e <= 1; e++){
    primaryNotes[e][activeChannels][3] = mainVelocity;
  }
  // Check the input to randomize the current channel/instrument
  oldRandomizeChannel = randomizeChannelInput;
  randomizeChannelInput = digitalRead(randomizeChannelPin);
  if ((oldRandomizeChannel != randomizeChannelInput) && !randomizeChannelInput){
    randomizeChannelRisingEdge = HIGH;
    //Serial.println("Randomize");
  }else{
    randomizeChannelRisingEdge = LOW;
  }
  // Check the input to confirm the current channel/instrument
  oldConfirmChannel = confirmChannelInput;
  confirmChannelInput = digitalRead(confirmChannelPin);
  if ((oldConfirmChannel != confirmChannelInput) && !confirmChannelInput && randomizeChannelInput){
    confirmChannelRisingEdge = HIGH;
    delay(10);
    Serial.println("\n");
    Serial.println("Confirm");
  }else{
    confirmChannelRisingEdge = LOW;
  }
  // Check the input to manually play a note in the 0BPM mode
  manualPlayInput = digitalRead(manualPlayPin);
  if ((oldManualPlayInputValue != manualPlayInput) && (!manualPlayInput)){
    manualPlayRisingEdge = HIGH;
  }else{
    manualPlayRisingEdge = LOW;
  }
  if ((oldManualPlayInputValue != manualPlayInput) && (manualPlayInput)){
    manualPlayFallingEdge = HIGH;
  }else{
    manualPlayFallingEdge = LOW;
  }
  oldManualPlayInputValue = manualPlayInput;
  // Check for both buttons held together.
  if (!randomizeChannelInput && !confirmChannelInput){
    panic();
  }
}

void updateTempo(){
  // tempoPot controls the tempo and is reported in BPM
  int funPinData = analogRead(tempoPot);
  mainBPM = map(funPinData, 0, 4095, minTempo, maxTempo);
  mainTempo = (60000 / mainBPM);
}

void printInputs(){
  Serial.print("BPM: ");
  Serial.print(mainBPM);
  Serial.print("\t");
  Serial.print("Velocity: ");
  Serial.print(mainVelocity);
  Serial.print("\t");
  Serial.print(randomizeChannelInput);
  Serial.print("\t");
  Serial.print(confirmChannelInput);
  Serial.print("\t");
  Serial.print(manualPlayInput);
  Serial.print("\n");
}

void calculateMainNote(){
  // Generate the next note.
  noteDelta = random(0, 12);
  // Check to see that the note will be within the prescribed range.
  noteDelta = maintainNoteLimits(mainNote, noteDelta, lowerThreshold, upperThreshold);
  // Change the mainNote variable according to the selected scale.
  mainNote = mainNote + minorPentatonicScale[noteDelta];
  nextNoteIteration = timeThisCycle + (mainTempo);
}

void calculateChannel00(){
  // Channel zero always has a time offset of zero, a pitch offset of
  // zero, and plays at full volume so it is calculated differently.
  int chanceToPlay = random(0, 100);
  if (mainBPM == minTempo){
    chanceToPlay = 100;
  }
  if (chanceToPlay < channelTweaks[0][2]){
    primaryNotes[0][0][0] = mainNote;
    primaryNotes[0][0][1] = timeThisCycle;
    primaryNotes[0][0][2] = timeThisCycle + mainTempo;
    primaryNotes[0][0][3] = channelTweaks[0][4];
    primaryNotes[0][0][4] = 1;
  }
}

void calculateChannels(boolean funEvenOdd){
  for (int i=1; i <= activeChannels; i++){
    if (i != 9){
      // Calculate the Pitch offset
      primaryNotes[funEvenOdd][i][0] = mainNote + channelTweaks[i][0];
      // Calculate the Time on value
      int funTimeOffset = (channelTweaks[i][1] * mainTempo) / 10;
      int funTimeOn = timeThisCycle + funTimeOffset;
      primaryNotes[funEvenOdd][i][1] = funTimeOn;
      // Calculate the Time off value
      int funDuration =  (channelTweaks[i][3]*mainTempo) / 10;
      primaryNotes[funEvenOdd][i][2] = funTimeOn + funDuration;
      // Calculate the note Velocity
      primaryNotes[funEvenOdd][i][3] = channelTweaks[i][4];
      // Set value [4] to one so it can begin playing
      primaryNotes[funEvenOdd][i][4] = 1;
    }
  }
}

void randomizeChannel(){
  stopActiveNotes();
  
  // Select an instrument
  prandomInstrument = random(0, 127);
  midiSetInstrument(activeChannels, prandomInstrument);
  channelTweaks[activeChannels][5] = prandomInstrument;
  
  // Record a prandom Pitch offset [0]
  prandomPitchOffset = random(-12, 12);
  channelTweaks[activeChannels][0] = prandomPitchOffset;
  
  // Record a prandom Time offset [1]
  prandomTimeOffset = random(0, 10);
  channelTweaks[activeChannels][1] = prandomTimeOffset;
  
  // Record a prandom Play frequency [2]
  prandomPlayFrequency = random(10, 100);
  channelTweaks[activeChannels][2] = prandomPlayFrequency;
  
  // Record a prandom Duration [3]
  prandomDuration = random(1, 10);
  channelTweaks[activeChannels][3] = prandomDuration;
  
  // Start with the system velocity
  channelTweaks[activeChannels][4] = mainVelocity;
}

void printChannelInformation(){
  Serial.print("\n");
  Serial.print("* * * * * Channel info * * * * *");
  Serial.print("\n");
  for (int i = 0; i<=activeChannels; i++){
    if (i == 9){
      i = 10;
    }
    if (i == 0){
      Serial.print("Channel: ");
      Serial.print(i);
      Serial.print("\tInstrument: ");
      Serial.print(channelTweaks[i][5]);
      Serial.print("\t\t\t\t\t");
      Serial.print("\tPlay frequency: ");
      Serial.print(channelTweaks[i][2]);
      Serial.print("\tVelocity: ");
      Serial.print(channelTweaks[i][4]);
      Serial.print("\n");
    }else{
      Serial.print("Channel: ");
      Serial.print(i);
      Serial.print("\tInstrument: ");
      Serial.print(channelTweaks[i][5]);
      Serial.print("\tPitch offset: ");
      Serial.print(channelTweaks[i][0]);
      Serial.print(" \tTime offset: ");
      Serial.print(channelTweaks[i][1]);
      Serial.print("\tPlay frequency: ");
      Serial.print(channelTweaks[i][2]);
      Serial.print("\tVelocity: ");
      Serial.print(channelTweaks[i][4]);
      Serial.print("\n");
    }
  }
  Serial.print("\n");
}

void panic(){
  for (int e=0; e <= 1; e++){
    for (int i = 0; i < 16; i++){
      for (int k = 0; k < 127; k++){
        midiNoteOff(i, k, 0);
      }
      midiSetChannelVolume(i, 127);
      primaryNotes[e][i][4] = 0;
    }
  }
  activeChannels = 0;
  Serial.println("Panic");
  delay(1000);
}

void stopAllNotes(){
  for (int e=0; e <= 1; e++){
    for (int i = 0; i < 16; i++){
      for (int k = 0; k < 127; k++){
        midiNoteOff(i, k, 0);
      }
      midiSetChannelVolume(i, 127);
      primaryNotes[e][i][4] = 0;
    }
  }
}

void stopActiveNotes(){
  for (int e=0; e <= 1; e++){
    for (int i = 0; i <= activeChannels; i++){
      for (int k = 0; k < 127; k++){
        midiNoteOff(i, k, 0);
      }
      midiSetChannelVolume(i, 127);
      primaryNotes[e][i][4] = 0;
    }
  }
}

void printMIDIToTerminal(uint8_t chan, uint8_t n, uint8_t vel){
  Serial.print("\n");
  Serial.print(MIDI_NOTE_ON | chan, HEX);
  Serial.print(", ");
  Serial.print(n);
  Serial.print(", ");
  Serial.print(vel);
}
