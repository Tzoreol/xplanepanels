const byte LED = 2;
const byte SWITCH = 3;

boolean prevSwitchState = HIGH;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);
  pinMode(SWITCH, INPUT_PULLUP);

  Serial.begin(9600);
}

void loop() {
  Serial.print("Prev: ");
  Serial.print(prevSwitchState);
  Serial.print(" Now: ");
  Serial.print(digitalRead(SWITCH));
  Serial.print('\n');
  
  if(prevSwitchState == LOW && digitalRead(SWITCH) == HIGH) {
    if(digitalRead(LED) == LOW) {
      digitalWrite(LED, HIGH);
    } else {
      digitalWrite(LED, LOW);
    }
  }

  prevSwitchState = digitalRead(SWITCH);
}
