/*
A arduino code that converts keypad input into analog output
input (x, y) -> key
*/

#include <Keypad.h>

const byte ROWS = 4, COLS = 4;
const char keymap[ROWS][COLS] = {
  {'(', ')', 'X', 'X'},
  {'X', 'X', 'X', 'X'},
  {'X', 'X', 'X', 'X'},
  {'X', 'X', 'X', 'X'}
};

const byte rowPins[] = {2, 3, 4, 5};
const byte colPins[] = {6, 7, 8, 9};

Keypad keypad (makeKeymap(keymap), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(9600);
}

void loop() {
  char ch = keypad.getKey();
  if (ch && ch != 'X') {
    Serial.print(ch);
  }
}
