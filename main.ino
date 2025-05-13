
#include <Stepper.h>
#include <Servo.h>

#define NO_SPACES 8           // total slots including entrance slot  
#define ENTRANCE_INDEX 0      // index reserved for entrance
#define EXIT_INDEX 4
#define STEPS_PER_REVOLUTION 4096

const float pricePerMinute = 1.0;
const int stepsPerSpace = STEPS_PER_REVOLUTION / NO_SPACES;

Stepper stepper(STEPS_PER_REVOLUTION, 8, 10, 9, 11);

class Barrier {
public:
  Barrier(int pin, int closedAngle = 90, int openAngle = 160)
    : servoPin(pin), closedAngle(closedAngle), openAngle(openAngle) {}

  void begin() {
    servo.attach(servoPin);
    servo.write(closedAngle);
  }

  void openBarrier() {
    Serial.println("raising_barrier");
    for (int angle = closedAngle; angle <= openAngle; angle++) {
      servo.write(angle);
      delay(15);
    }
    delay(5000);  // keep barrier open
    Serial.println("lowering barrier.");
    for (int angle = openAngle; angle >= closedAngle; angle--) {
      servo.write(angle);
      delay(15);
    }
  }

private:
  Servo servo;
  int servoPin;
  int closedAngle;
  int openAngle;
};

Barrier barrier(6);

class ParkingSpace {
public:
  bool occupied;
  unsigned long entryTime;
  String cardCode;
  int spaceIndex;

  ParkingSpace() : occupied(false), entryTime(0), cardCode(""), spaceIndex(0) {}
  ParkingSpace(int index) : occupied(false), entryTime(0), cardCode(""), spaceIndex(index) {}

  // Toggle occupancy: if empty -> occupy and start timing, if occupied -> free and calculate price
  float toggle() {
    if (!occupied) {
      occupied = true;
      entryTime = millis();
      Serial.println("Car entered space " + String(spaceIndex) + ".");
      return 0;
    } else {
      occupied = false;
      unsigned long durationMillis = millis() - entryTime;
      float minutes = durationMillis / 60000.0;
      float price = minutes * pricePerMinute;
      //Serial.print("Car leaving space " + String(spaceIndex) + ". Time: ");
      Serial.print("result ");
      Serial.print(minutes, 2);
      Serial.print(" ");
      //Serial.print(" min. Price: $");
      Serial.println(price, 2);
      cardCode = "";
      return price;
    }
  }
};

class ParkingLot {
public:
  ParkingSpace spaces[NO_SPACES];
  int currentIndex;

  ParkingLot() : currentIndex(ENTRANCE_INDEX) {
    for (int i = 0; i < NO_SPACES; i++) {
      spaces[i] = ParkingSpace(i);
    }
  }

  void processCard(String cardKey) {
    int idx = findCard(cardKey);
    if (idx != -1) {
      // Existing car exiting
      rotateTo(idx);
      spaces[idx].toggle();            // compute price and free
      delay(4000);                     // allow driver to get in
      rotateTo(EXIT_INDEX);
      barrier.openBarrier();
    } else {
      int emptyIndex = findEmpty();
      if (emptyIndex != -1) {
        spaces[emptyIndex].cardCode = cardKey;
        // New car entering
        rotateTo(ENTRANCE_INDEX);
        Serial.println("CMD_welcome");  
        barrier.openBarrier();          // let car drive onto platform at 0
        rotateTo(emptyIndex);
        spaces[emptyIndex].toggle();   // mark occupied
      } else {
        Serial.println("No empty parking spaces available.");
      }
    }
  }

private:
  int findCard(const String &cardKey) {
    for (int i = 0; i < NO_SPACES; i++) {
      if (spaces[i].occupied && spaces[i].cardCode == cardKey) {
        return i;
      }
    }
    return -1;
  }

  int findEmpty() {
    for (int i = 1; i < NO_SPACES; i++) { // skip entrance slot 0
      if (!spaces[i].occupied) return i;
    }
    return -1;
  }

  void rotateTo(int targetIndex) {
    if (targetIndex == currentIndex) return;
    int diff = (targetIndex - currentIndex + NO_SPACES) % NO_SPACES;
    if (diff > NO_SPACES / 2) {
      int rev = NO_SPACES - diff;
      stepper.step(-stepsPerSpace * rev);
      Serial.print("Rotating counterclockwise to index ");
      Serial.println(targetIndex);
    } else {
      stepper.step(stepsPerSpace * diff);
      Serial.print("Rotating clockwise to index ");
      Serial.println(targetIndex);
    }
    currentIndex = targetIndex;
  }
};

ParkingLot lot;

void setup() {
  Serial.begin(9600);
  stepper.setSpeed(5);
  barrier.begin();
  Serial.println("parking-lot-ready");
}

void loop() {
  if (Serial.available()) {
    String cardKey = Serial.readStringUntil('\n');
    cardKey.trim();
    if (cardKey.length() > 0) {
      lot.processCard(cardKey);
    }
    while (Serial.available()) Serial.read();
  }
}
