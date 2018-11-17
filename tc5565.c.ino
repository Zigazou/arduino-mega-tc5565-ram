/*
 * Using a Toshiba TC5565A*L 8192 bytes CMOS RAM chip with an Arduino MEGA 2560.
 *
 * The Arduino MEGA and the Toshiba RAM chip must be connected according to the
 * following schematics.
 *
 *    Arduino MEGA 2560
 *      .-----.-----.                           .-----._.-----.
 *   5V |     |     | 5V                   NC -[|1          28|]- VDD
 *      +-----+-----+                     A12 -[|2          27|]- R/W
 *   22 | IO1 | IO2 | 23                   A7 -[|3          26|]- CE2
 *      +-----+-----+                      A6 -[|4          25|]- A8
 *   24 | IO3 | IO4 | 25                   A5 -[|5          24|]- A9
 *      +-----+-----+                      A4 -[|6  TOSHIBA 23|]- A11
 *   26 | IO5 | IO6 | 27                   A3 -[|7          22|]- \OE
 *      +-----+-----+                      A2 -[|8  TC5565  21|]- A10
 *   28 | IO7 | IO8 | 29                   A1 -[|9          20|]- \CE1
 *      +-----+-----+                      A0 -[|10         19|]- IO8
 *   30 |  A7 | A6  | 31                  IO1 -[|11         18|]- IO7
 *      +-----+-----+                     IO2 -[|12         17|]- IO6
 *   32 |  A5 | A4  | 33                  IO3 -[|13         16|]- IO5
 *      +-----+-----+                     GND -[|14         15|]- IO4
 *   34 |  A3 | A2  | 35                        `-------------'
 *      +-----+-----+
 *   36 |  A1 | A0  | 37       Registers:
 *      +-----+-----+          IO1..IO8 -> PORTA
 *   38 |     |     | 39       A7..A0   -> PORTC
 *      +-----+-----+          A12..A8  -> PORTL
 *   40 |     |     | 41       R/W, CE2, \CE1, \OE -> PORTB
 *      +-----+-----+
 *   42 |     |     | 43       Operation modes:
 *      +-----+-----+          Bits   3     2    1    0 in PORTB
 *   44 |     | A12 | 45             \OE  \CE1  CE2  R/W
 *      +-----+-----+          Read   L     L    H    H
 *   46 | A11 | A10 | 47       Write  *     L    H    L
 *      +-----+-----+          Desel  H     L    H    H
 *   48 |  A9 | A8  | 49
 *      +-----+-----+
 *   50 | \OE |\CE1 | 51
 *      +-----+-----+
 *   52 | CE2 | R/W | 53
 *      +-----+-----+
 *  GND |     |     | GND
 *      `-----+-----'
 *
 */

/* For faster communication, bit combinations are precalculated. */
#define OPERATION_READ B00000011
#define OPERATION_WRITE B00000010
#define OPERATION_DESELECT B00001011

void setup() {
  Serial.begin(115200);

  /* Address pins and operation modes pins are output only. */
  DDRB = B11111111;
  DDRC = B11111111;
  DDRL = B11111111;

  /* The deselect operation mode does no read or write. */
  PORTB = OPERATION_DESELECT;

  /* Show information on the serial monitor. */
  Serial.println("Controlling Toshiba TC5565A*L 8192 bytes CMOS RAM.");
  Serial.println("Each dot means a complete test set has been passed.");

  memory_bandwidth();
}

/* Put a value in RAM at the specified address */
void poke(unsigned int address, unsigned char value) {
  /* Every pin of port A is set to output mode */
  DDRA = B11111111;

  /* Set the address to write */
  PORTC = (unsigned char)(address & 0x00ff);
  PORTL = (unsigned char)(address >> 8);

  /* Set the value */
  PORTA = value;

  /* Set the RAM chip in write mode */
  PORTB = OPERATION_WRITE;

  /* Give some time to the RAM chip */
  /*delayMicroseconds(1);*/

  /* Deselect the RAM chip */
  PORTB = OPERATION_DESELECT;
}

/* Get the value in RAM at the specified address */
unsigned char peek(unsigned int address) {
  unsigned char value;

  /* Every pin of port A is set to output mode */
  DDRA = B00000000;

  /* Set the address to read */
  PORTC = (unsigned char)(address & 0x00ff);
  PORTL = (unsigned char)(address >> 8);

  /* Set the RAM chip in read mode */
  PORTB = OPERATION_READ;

  /* Give some time to the RAM chip */
  delayMicroseconds(2);
  /* These 2 read are finer delay than delayMicroseconds(); */
  /* value = PINA; */
  /* value = PINA; */

  /* Read the value */
  value = PINA;

  /* Deselect the RAM chip */
  PORTB = OPERATION_DESELECT;

  return value;
}

/*
 * The following memory test functions are adapted from this article:
 * https://barrgroup.com/Embedded-Systems/How-To/Memory-Test-Suite-C
 */
unsigned char test_data_bus() {
  unsigned char pattern;

  /* Perform a walking 1's test at the given address. */
  for(pattern = 1; pattern != 0; pattern <<= 1) {
    /* Write the test pattern. */
    poke(0, pattern);

    /* Read it back (immediately is okay for this test). */
    if(peek(0) != pattern) return pattern;
  }

  return 0;
}

unsigned int test_address_bus(unsigned int count) {
  unsigned long mask = count - 1;
  unsigned long offset;
  unsigned long test_offset;

  unsigned char pattern = 0xAA;
  unsigned char antipattern = 0x55;

  /* Write the default pattern at each of the power-of-two offsets. */
  for(offset = 1; (offset & mask) != 0; offset <<= 1) {
    poke(offset, pattern);
  }

  /* Check for address bits stuck high. */
  test_offset = 0;
  poke(test_offset, antipattern);

  for(offset = 1; (offset & mask) != 0; offset <<= 1) {
    if(peek(offset) != pattern) return offset;
  }

  poke(test_offset, pattern);

  /* Check for address bits stuck low or shorted. */
  for(test_offset = 1; (test_offset & mask) != 0; test_offset <<= 1) {
    poke(test_offset, antipattern);

    if(peek(0) != pattern) return test_offset;

    for(offset = 1; (offset & mask) != 0; offset <<= 1) {
      if((peek(offset) != pattern) && (offset != test_offset)) {
        return test_offset;
      }
    }

    poke(test_offset, pattern);
  }

  return -1;
}

unsigned int test_device(unsigned int count) {
  unsigned int offset;

  unsigned char pattern;
  unsigned char antipattern;

  /* Fill memory with a known pattern. */
  for(pattern = 1, offset = 0; offset < count; pattern++, offset++) {
    poke(offset, pattern);
  }

  /* Check each location and invert it for the second pass. */
  for(pattern = 1, offset = 0; offset < count; pattern++, offset++) {
    if(peek(offset) != pattern) return offset;

    antipattern = ~pattern;
    poke(offset, antipattern);
  }

  /* Check each location for the inverted pattern and zero it. */
  for(pattern = 1, offset = 0; offset < count; pattern++, offset++) {
    antipattern = ~pattern;

    if(peek(offset) != antipattern) return offset;
  }

  return -1;
}

/* Run a memory test set. */
void test_memory() {
  unsigned int result;

  result = test_data_bus();
  if(result != 0) {
    Serial.println("");
    Serial.print("Data bus test, error with value 0x");
    Serial.println(result, HEX);
    delay(5000);
    return;
  }

  result = test_address_bus(8192);
  if(result != -1) {
    Serial.println("");
    Serial.print("Address bus test, error at address 0x");
    Serial.println(result, HEX);
    delay(5000);
    return;
  }

  result = test_device(8192);
  if(result != -1) {
    Serial.println("");
    Serial.print("Device test, error at address 0x");
    Serial.println(result, HEX);
    delay(5000);
    return;
  }

  Serial.print(".");
}

/* Evaluate memory bandwidth */
#define TEST_SAMPLES 1000000
void memory_bandwidth() {
  double time_start;
  double time_stop;
  double bandwidth;

  unsigned long i;

  /* Write bandwidth */
  Serial.print("Write speed: ");

  time_start = micros();
  for(i = 0; i < TEST_SAMPLES; i++) poke(i, i);
  time_stop = micros();

  bandwidth = TEST_SAMPLES / ((time_stop - time_start) / 1000000);
  Serial.print((time_stop - time_start) / TEST_SAMPLES);
  Serial.print(" µs / ");
  Serial.print(bandwidth);
  Serial.println(" bytes/second");

  /* Read bandwidth */
  Serial.print("Read speed: ");

  time_start = micros();
  for(i = 0; i < TEST_SAMPLES; i++) peek(i);
  time_stop = micros();

  bandwidth = TEST_SAMPLES / ((time_stop - time_start) / 1000000);
  Serial.print((time_stop - time_start) / TEST_SAMPLES);
  Serial.print(" µs / ");
  Serial.print(bandwidth);
  Serial.println(" bytes/second");
}

unsigned int count = 0;
void loop() {
  /* Each line has no more than 64 dots. */
  if(count++ % 64 == 0) Serial.println("");

  /* Run a test set. */
  test_memory();
}
