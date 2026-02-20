#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <stdlib.h>

// LCD config
LiquidCrystal_I2C lcd(0x20, 16, 2);
const byte LCD_COLS = 16;
const byte LCD_ROWS = 2;

// Keypad config
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','+'},
  {'4','5','6','-'},
  {'7','8','9','*'},
  {'.','0','=','/'}
};
byte rowPins[ROWS] = {9,8,7,6};
byte colPins[COLS] = {5,4,3,2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Equation buffer
char equation[32];
byte equationI = 0;

// Cursor position (for generic prints if needed)
byte pos[2] = {0, 0};

// ------------------- Helpers: pixel-perfect printing -------------------

void lcdClearAll() {
  lcd.clear();
  pos[0] = 0; pos[1] = 0;
}

void clearLine(byte row) {
  lcd.setCursor(0, row);
  for (byte i = 0; i < LCD_COLS; i++) lcd.print(' ');
  lcd.setCursor(0, row);
}

void printAt(byte col, byte row, const char* msg) {
  // Truncate at 16 cols
  lcd.setCursor(col, row);
  for (byte i = 0; msg[i] != '\0' && i + col < LCD_COLS; i++) {
    lcd.print(msg[i]);
  }
}

void printFixed(byte row, const char* msg) {
  // Pad or truncate to exactly 16 columns
  byte len = strlen(msg);
  lcd.setCursor(0, row);
  for (byte i = 0; i < LCD_COLS; i++) {
    char ch = (i < len) ? msg[i] : ' ';
    lcd.print(ch);
  }
}

void printRightAligned(byte row, const char* msg) {
  byte len = strlen(msg);
  byte col = (len >= LCD_COLS) ? 0 : (LCD_COLS - len);
  clearLine(row);
  printAt(col, row, msg);
}

void printToLCD(const char msg[]) {
  for (byte i = 0; msg[i] != '\0'; i++) {
    lcd.setCursor(pos[0], pos[1]);
    lcd.print(msg[i]);
    pos[0]++;
    if (pos[0] >= LCD_COLS) {
      pos[0] = 0;
      pos[1]++;
      if (pos[1] >= LCD_ROWS) pos[1] = 0;
    }
  }
}

void error(const char message[]) {
  lcdClearAll();
  printFixed(0, "Error:");
  printFixed(1, message);
  delay(1500);
  lcdClearAll();
  equationI = 0;
}

// ------------------- Array removal -------------------

byte OperationLimit = 32;

bool isNum(char ch) {
    return (ch >= '0' && ch <= '9');
}
char *substring(const char *src, int start, int len) {
    if (start < 0 || len < 0 || start + len > strlen(src)) {
        return NULL; // invalid range
    }
    char *dest = malloc(len + 1); // +1 for null terminator
    if (!dest) return NULL; // allocation failed
    strncpy(dest, src + start, len);
    dest[len] = '\0';
    return dest;
}
void removeFromArr(void *arr, byte *size, byte index, size_t elemSize) {
    if (index >= *size || index < 0) return;
    char *base = (char *)arr;
    memmove(base + index * elemSize,
            base + (index + 1) * elemSize,
            (*size - index - 1) * elemSize);
    (*size)--;
}

float evaluateEquation(float nums[], byte oprs[], byte l) {
    // Find highest-precedence op: 4(/),3(*),2(+),1(-)
    byte operationIndex = 0;
    for (byte p = 4; p >= 1; p--) {
        bool found = false;
        for (byte i = 0; i < l; i++) {
            if (oprs[i] == p) { operationIndex = i; found = true; break; }
        }
        if (found) break;
    }

    float n1 = nums[operationIndex];
    float n2 = nums[operationIndex + 1];

    if (n2 == 0 && oprs[operationIndex] == 4) {
        error("Division by 0");
        return 0;
    }

    float result = 0;
    switch (oprs[operationIndex]) {
        case 1: result = n1 - n2; break; // -
        case 2: result = n1 + n2; break; // +
        case 3: result = n1 * n2; break; // *
        case 4: result = n1 / n2; break; // /
    }

    if (l == 1) return result;

    nums[operationIndex] = result;
    byte numsL = l + 1;
    removeFromArr(nums, &numsL, operationIndex + 1, sizeof(float));
    removeFromArr(oprs, &l, operationIndex, sizeof(byte));

    return evaluateEquation(nums, oprs, l);
}
void giveStacks(char str[], byte l, float numStack[], byte oprStack[], byte *operation_length) {
    byte numI = 0, tempI = 0, oprI = 0;
    char temp[100];

    for (byte i = 0; i <= l; i++) {
        char ch = str[i];
        if (isNum(ch) || (ch == '.' )) { // allow decimals
            if (tempI < sizeof(temp)-1) temp[tempI++] = ch;
        } else if (ch == '-' && (i == 0 || !isNum(str[i-1]))) {
            // unary minus: start a negative number
            temp[tempI++] = ch;
        } else {
            if (tempI > 0) {
                temp[tempI] = '\0';
                numStack[numI++] = atof(temp);
                tempI = 0;
            }
            if (ch == '\0') break;
            oprStack[oprI++] = (ch == '/') ? 4 :
                               (ch == '*') ? 3 :
                               (ch == '+') ? 2 : 1;
        }
    }

    if (tempI > 0) {
        temp[tempI] = '\0';
        numStack[numI++] = atof(temp);
    }

    *operation_length = oprI;
}
float evaluateEquationWithBrackets(char equation[]) {
    byte start = 0, end = 0;
    byte eqLen = strlen(equation);

    for (byte i = 0; i < eqLen; i++) {
        if (equation[i] == '(') {
            start = i;
        }
        if (equation[i] == ')') {
            end = i;

            // Extract substring inside parentheses into a fixed buffer
            char inner[OperationLimit];
            byte l = 0;
            for (byte j = start + 1; j < end && l < OperationLimit - 1; j++) {
                inner[l++] = equation[j];
            }
            inner[l] = '\0';

            // Evaluate the inner expression
            float numStack[OperationLimit/2+1];
            byte oprStack[OperationLimit/2-1];
            byte length = 0;

            giveStacks(inner, l, numStack, oprStack, &length);
            float res = evaluateEquation(numStack, oprStack, length);

            // Convert result to string in a fixed buffer
            char buffer[16];
            dtostrf(res, 1, 6, buffer);

            // Build new equation in a fixed buffer
            char newEquation[OperationLimit * 2];
            newEquation[0] = '\0';

            // Copy before part
            strncat(newEquation, equation, start);

            // Append result
            strncat(newEquation, buffer, sizeof(newEquation) - strlen(newEquation) - 1);

            // Append after part
            strncat(newEquation, equation + end + 1, sizeof(newEquation) - strlen(newEquation) - 1);

            Serial.println(newEquation); // Debug print

            // Recurse
            return evaluateEquationWithBrackets(newEquation);
        }
    }

    // ✅ Base case: no parentheses left
    float numStack[OperationLimit/2+1];
    byte oprStack[OperationLimit/2-1];
    byte length = 0;

    giveStacks(equation, eqLen, numStack, oprStack, &length);
    return evaluateEquation(numStack, oprStack, length);
}

void appendInput(char c) {
  if (equationI < 31) {
    equation[equationI++] = c;
    char view[33] = {0};
    byte start = (equationI <= LCD_COLS) ? 0 : (equationI - LCD_COLS);
    byte vlen = equationI - start;
    for (byte i = 0; i < vlen && i < LCD_COLS; i++) view[i] = equation[start + i];
    printFixed(0, view);
    Serial.print(c);
  } else {
    error("Too long");
  }
}

// ------------------- Arduino setup -------------------

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcdClearAll();
  printFixed(0, "Calculator");
  clearLine(1);
}

// ------------------- Arduino loop -------------------

void loop() {
  char key = keypad.getKey();
  if (key) {

    if (key == '=') {
      // Guard: terminate buffer for safe tokenization
      if (equationI >= 0 && equationI < 32) equation[equationI] = '\0';

      byte numI = 0;
      byte oprI = 0;

      char temp[16] = {0};
      byte tempI = 0;

      // Validation: no two operators in a row, allow parentheses
      for (byte i = 1; i < equationI; i++) {
          char ch = equation[i], prevCh = equation[i - 1];
          if (!isNum(ch) && !isNum(prevCh) &&
              ch != '(' && ch != ')' &&
              prevCh != '(' && prevCh != ')') {
              error("Invalid Expr");
              return;
          }
      }

      // Tokenize into nums/oprs
      for (byte i = 0; i <= equationI; i++) {
        char ch = equation[i];
        if (isNum(ch)) {
          if (tempI < 15) temp[tempI++] = ch;
        } else {
          if (tempI > 0) {
            temp[tempI] = '\0';
            numI++;
            tempI = 0;
          }
          if (ch == '\0') break;
          oprI++;
        }
      }

      // Evaluate
      float result = (oprI > 0 || numI == 1) ? evaluateEquationWithBrackets(equation)
                                            : 0;

      // Display result on bottom row, right-aligned
      char buf[16];
      dtostrf(result, 1, 6, buf);
      clearLine(1);
      printRightAligned(1, buf);

      Serial.print("Result: ");
      Serial.println(result);

      // Reset input line and buffer
      clearLine(0);
      equationI = 0;
      pos[0] = pos[1] = 0;

    } else {
      appendInput(key);
    }
  }

  while (Serial.available()) {
    char ch = Serial.read();
    if (ch) appendInput(ch);
  }
}

