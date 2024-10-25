#include <Servo.h>

int sensor_pin = A0;             // Analog pin for pulse sensor
int led_pin = 13;                // LED pin for heartbeat indication
Servo myservo;

volatile int heart_rate;         // Heart rate (BPM)
volatile int time_between_beats = 600;
volatile int beat[10];           // Array to store time intervals between beats
volatile int peak_value = 512;
volatile int trough_value = 512;
volatile int thresh = 525;       // Dynamic threshold for detecting beats
volatile int amplitude = 100;

volatile boolean first_heartpulse = true;
volatile boolean second_heartpulse = false;
volatile unsigned long samplecounter = 0;  // Tracks pulse timing
volatile unsigned long lastBeatTime = 0;
volatile boolean pulse_signal = false;

void setup() {
    pinMode(led_pin, OUTPUT);
    Serial.begin(115200);
    myservo.attach(10);           // Attach servo to pin 10
    pinMode(0, INPUT);
    pinMode(1, INPUT);
    interruptSetup();
}

void loop() {
    Serial.print("BPM: ");
    Serial.println(heart_rate);

    if (heart_rate < 65) {
        if (digitalRead(0) == HIGH && digitalRead(1) == LOW) {
            myservo.write(118);
        } else if (digitalRead(1) == HIGH && digitalRead(0) == LOW) {
            myservo.write(62);
        } else if (digitalRead(1) == LOW && digitalRead(0) == LOW) {
            myservo.write(90);
        }
    }

    delay(200);  // Short delay for readability of serial output
}

void interruptSetup() {
    // Timer2 setup for 500Hz sampling rate
    TCCR2A = 0x02;   // Disable PWM on pin 3 and 11
    OCR2A = 0x7C;    // Set count top to 124 for 500Hz sample rate
    TCCR2B = 0x06;   // 256 prescaler
    TIMSK2 = 0x02;   // Enable interrupt on match between OCR2A and Timer
    sei();           // Enable global interrupts
}

// ISR for Timer2
ISR(TIMER2_COMPA_vect) {
    cli();  // Temporarily disable interrupts to avoid overlapping ISR execution

    // Read analog data with basic noise filtering
    int analog_data = analogRead(sensor_pin);
    if (analog_data < (trough_value + 10) || analog_data > (peak_value - 10)) {
        samplecounter += 2;
        int N = samplecounter - lastBeatTime;

        // Update peak and trough values dynamically for accurate thresholding
        if (analog_data < thresh && N > (time_between_beats / 5) * 3) {
            if (analog_data < trough_value) {
                trough_value = analog_data;
            }
        }
        if (analog_data > thresh && analog_data > peak_value) {
            peak_value = analog_data;
        }

        // Detect heartbeat if threshold conditions are met
        if (N > 250) {
            if (analog_data > thresh && pulse_signal == false && N > (time_between_beats / 5) * 3) {
                pulse_signal = true;
                digitalWrite(led_pin, HIGH);

                time_between_beats = samplecounter - lastBeatTime;
                lastBeatTime = samplecounter;

                if (second_heartpulse) {
                    second_heartpulse = false;
                    for (int i = 0; i < 10; i++) {
                        beat[i] = time_between_beats;
                    }
                }

                if (first_heartpulse) {
                    first_heartpulse = false;
                    second_heartpulse = true;
                    sei();
                    return;
                }

                // Calculate average BPM using moving average for smoother output
                unsigned long runningTotal = 0;
                for (int i = 0; i < 9; i++) {
                    beat[i] = beat[i + 1];
                    runningTotal += beat[i];
                }
                beat[9] = time_between_beats;
                runningTotal += beat[9];
                runningTotal /= 10;
                heart_rate = 60000 / runningTotal;
            }
        }

        // Adjust threshold based on peak-trough amplitude
        if (analog_data < thresh && pulse_signal == true) {
            digitalWrite(led_pin, LOW);
            pulse_signal = false;
            amplitude = peak_value - trough_value;
            thresh = amplitude / 2 + trough_value;
            peak_value = thresh;
            trough_value = thresh;
        }

        // Reset conditions if no beat detected in 2.5 seconds
        if (N > 2500) {
            thresh = 512;
            peak_value = 512;
            trough_value = 512;
            lastBeatTime = samplecounter;
            first_heartpulse = true;
            second_heartpulse = false;
        }
    }

    sei();  // Re-enable interrupts
}
