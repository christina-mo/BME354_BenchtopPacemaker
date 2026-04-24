//BME 354 Pacemaker Pacing & Added Functionality Arduino Code
//Authors: Christina Mo, Kabir Gupta
//Last edited: April 21, 2026

//ECG pins
const int ECG_amp_pin = A0; //amplified filtered ECG pin
const int ECG_comp_pin = A1; //ECG pin
const int ECG_Vs_pin = A2; //Voltage supply pin
const int outpin = 2; //LED's pin

//Voltage values
float ECG_amp = 0.0; //amplified filtered ECG voltage
float ECG_comp = 0.0; //comparator voltage
float ECG_Vs = 0.0; //Voltage supply voltage (battery)

//Timekeeping
unsigned long paceStartTime = 0;
const unsigned long samplePeriod_us = 5000;   //200 Hz sampling period
unsigned long lastSampleTime = 0;
unsigned long initial_time = 0; //Time of last R wave peak

//Original VVI functionality 
float prev_comp = 0.0; //voltage of previous comparator value, used to see if comparator turned on or if comparator was already on
float R_int = 0.0; //R interval in seconds
float HR = 0.0; //Heart rate (bpm)
float LRI = 0.5; //lowest rate interval in seconds, 90 bpm = 0.67 s, 120 bpm = 0.50 s, 180 bpm = 0.33 s
bool pace = false; //boolean to monitor if voltage is being delivered to pace

//For added functionality
bool Vs_warningSent = false; //if voltage is below threshold and a warning was already sent
bool HR_warningSent = false; //if heart rate is below threshold and a warning was already sent
float battery_threshold = 2.5; //Vs threshold to trigger warning
float HR_threshold = 30; //HR threshold to trigger warning
const int buffer_size = 50; //length of datalogs
unsigned long time_buffer[buffer_size]; //buffer logs of time, heart rate, voltage, and pacing
float HR_buffer[buffer_size];
float Vs_buffer[buffer_size];
bool pace_buffer[buffer_size];
bool paced_event = false; //detects if pacemaker paced, used to add 0 or 1 to pace_buffer datalog
int counter = 0;
int counter_threshold = 100;

void setup() {
  Serial.begin(115200);   // USB to computer
  Serial1.begin(9600);    // USB to HM-10
  
  //Initialize pins
  pinMode(ECG_amp_pin, INPUT);
  pinMode(ECG_comp_pin, INPUT);
  pinMode(ECG_Vs_pin, INPUT);
  pinMode(outpin, OUTPUT);

  lastSampleTime = micros();
}

//Buffer storage function
void storeData() {
  // Shifts data left
  for (int i = 0; i < buffer_size - 1; i++) {
    HR_buffer[i] = HR_buffer[i + 1];
    Vs_buffer[i] = Vs_buffer[i + 1];
    pace_buffer[i] = pace_buffer[i + 1];
    time_buffer[i] = time_buffer[i + 1];
  }

  //Adds new data to last index of arrays
  HR_buffer[buffer_size - 1] = HR;
  Vs_buffer[buffer_size - 1] = ECG_Vs;
  pace_buffer[buffer_size - 1] = paced_event;
  time_buffer[buffer_size - 1] = millis();
  paced_event = false; //Resets pacing event
}

//Wireless data transfer function
void sendData() {
  Serial.println("Sending data to phone:");
  Serial.println("DATA TRANSFER BEGIN");
  Serial.println("time (ms),HR (bpm),Vs (V),Pacing");

  Serial1.println("DATA TRANSFER BEGIN");
  Serial1.println("time (ms),HR (bpm),Vs (V),Pacing");

  //Iterates through each index of the buffer and prints values in CSV form to the BLE terminal
  for (int i = 0; i < buffer_size; i++) {
    Serial1.print(time_buffer[i]/1000);
    Serial1.print(",");
    Serial1.print(HR_buffer[i]);
    Serial1.print(",");
    Serial1.print(Vs_buffer[i]);
    Serial1.print(",");
    Serial1.println(pace_buffer[i]);

    //Send to serial monitor for debugging
    Serial.print(time_buffer[i]);
    Serial.print(",");
    Serial.print(HR_buffer[i]);
    Serial.print(",");
    Serial.print(Vs_buffer[i]);
    Serial.print(",");
    Serial.println(pace_buffer[i]);
  }
  Serial1.println("DATA TRANSFER END");
  Serial.println("DATA TRANSFER END");
}

void loop() {
  unsigned long currentTime = micros();
  //Checks if phone asked for data transfer. If yes, send data.
  if (Serial1.available()) {
    String from_BLE = Serial1.readStringUntil('\n');
    if (from_BLE == "Send data") {
      sendData();
    }
    else if (from_BLE.startsWith("HR=")) {
      float pacing_HR = from_BLE.substring(4).toFloat();
      LRI = 60/pacing_HR;
      Serial1.print("LRI Updated: ");
      Serial1.println(LRI);
    }
  }

  // 200 Hz sampling rate
  if (currentTime - lastSampleTime >= samplePeriod_us) {
    lastSampleTime += samplePeriod_us;

    //Read voltages from comparator, filtered/amplified signal, and voltage supply pins 
    int ECG_amp_raw = analogRead(ECG_amp_pin);
    int ECG_comp_raw = analogRead(ECG_comp_pin);
    int ECG_Vs_raw = analogRead(ECG_Vs_pin);

    //Convert previous Arduino digital value to voltage value (V)
    ECG_amp = ECG_amp_raw * (5.0 / 1023.0);
    ECG_comp = ECG_comp_raw * (5.0 / 1023.0);
    ECG_Vs = ECG_Vs_raw * (5.0 / 1023.0);
    //Serial.print("ECG comp: ");
    //Serial.println(ECG_comp);

    //Sends notification to HM-10 BLE if battery falls below threshold (not already below threshold)
    if (ECG_Vs <= battery_threshold && !Vs_warningSent) {
        delay(1000);
        int ECG_Vs_raw = analogRead(ECG_Vs_pin);
        ECG_Vs = ECG_Vs_raw * (5.0 / 1023.0);
        Serial1.print("WARNING: LOW BATTERY ");
        Serial1.print(ECG_Vs);
        Serial1.println(" V");
        Vs_warningSent = true;
    }
    
    //Resets battery warning if voltage increases above threshold again
    if (Vs_warningSent && ECG_Vs > battery_threshold + 0.2) {
      Vs_warningSent = false;
    }

    //Delivers voltage if R interval is greater than LRI
    R_int = (currentTime - initial_time)/1000000.0; //distance between R waves (seconds)
    if (R_int > LRI){
      Serial.print("R int: ");
      Serial.println(R_int);
      Serial.print("LRI: ");
      Serial.println(LRI);
      HR = 60/R_int;
      Serial.print("Paced ");
      Serial.println(HR);
      digitalWrite(outpin, HIGH);
      pace = true;
      paced_event = true;
      paceStartTime = currentTime;
      initial_time = currentTime;
      counter = counter + 1;
      storeData(); //stores data whenever there is a pulse delivered from pacemaker
    }
    
    //Pace for 20 ms
    if (pace == true && currentTime - paceStartTime >= 20000){
      digitalWrite(outpin, LOW);
      pace = false;
    }

    //Identifies comparator turning on to determine R interval to calculate instantaneous heart rate
    if (ECG_comp > prev_comp && ECG_comp > 0.9 && pace == false){
      R_int = (currentTime - initial_time)/1000000.0;
      Serial.print("HR: ");
      Serial.println(HR);
      HR = 60/R_int;

      //Sends notification to HM-10 BLE if HR falls below threshold
      if (HR <= HR_threshold && !HR_warningSent) {
        Serial1.print("WARNING: LOW HEART RATE ");
        Serial1.print(HR);
        Serial1.println(" bpm");
        HR_warningSent = true;
      }

      if (HR > HR_threshold && HR_warningSent) {
        HR_warningSent = false;
      }

      Serial.print("HR:");
      Serial.println(HR);
      initial_time = currentTime;
    }
    if (counter < counter_threshold) {
      counter = counter + 1;
    }

    if (counter == counter_threshold) {
      storeData(); //Stores time, heart rate, voltage, pacing using helper function
      counter = 0;
    }


    prev_comp = ECG_comp; //resets comparator voltage to compare to for the next sampling cycle
  }
    // Print one sample per line for serial plotter(amp, comp format)
    //Serial.print(ECG_amp, 3);
    //Serial.print(",");
    //Serial.println(ECG_comp, 3);
}
