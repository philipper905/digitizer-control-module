#include <LiquidCrystal.h>
#define SERIAL_SPEED 9600

// Open Questions
// Is 16 characters of text enough to describe the action the arduino should take?
// Once the communication with the Raspberry pi is implemented 

// Issues & Thoughts
// We are trying to stay very conscious of memory usage. If we use fixed 
// strings, they can be stored in program memory and not use up the extremely
// limited stack space on the arduino.
// This means that the COMMAND-SET that the Control module uses to communicate
// with the raspberry pi is static and cannot be defined at runtime by communicating 
// with the raspberry pi as I had initially envisioned.

// On the other hand this may make the communication protocol simpler

// IDEAS
// In the strings for setting values, the first character stored 
// could be the suffix used after the number is incrementing.
// For now though there are no suffixes
// For example having a voltage (V) unit, or seconds (s) unit could be nice 
//to be able to display after the number

//CONTROL language
// client is the AVR microcontroller and server is the RPi
// A command starts with : and ends with a newline
// for both the arduino and the raspberry pi, this means we can continue sending debug garbage over serial which is nice.
// looks something :GET ID [NAME] <extra info>
// and the server would respond :TRIGGER STATUS=OK|FAIL
// examples of commands :TRIGGER id [NAME] <extra info>
// and server would respond :RESPOND TRIGGER id OK|FAIL <extra info>

// :GET id [name] <extra info>
// :RESPOND 

// Server can :RESPOND, :UPDATE, and :PUSH
// respond will be the first one implemented, 

// Button Pins: 
#define BUTTON_1 A0
#define BUTTON_2 A1
#define BUTTON_3 A6
#define BUTTON_4 A7

//LCD Pins
#define VSS_PIN 12
#define VDD_PIN 11
#define RS_PIN 10
#define RW_PIN 9
#define ENABLE_PIN 8
#define D4_PIN 6
#define D5_PIN 5
#define D6_PIN 4
#define D7_PIN 3
#define ANODE_PIN A7
#define CATHODE_PIN 2

//initializing the LCD, using the Arduino builtin LiquidCrystal library
LiquidCrystal lcd(RS_PIN, RW_PIN, ENABLE_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);


//Once we start running out of memory offload these to PROGMEM
const char string_0[16] = "Get IP Addr"; // "String 0" etc are strings to store - change to suit.
const char string_1[16] = "Start New Run";
const char string_2[16] = "Save wavefm #";
const char string_3[16] = "Get Thresh 1";
const char string_4[16] = "Get Thresh 2";
const char string_5[16] = "Get PSCL";
const char string_6[16] = "Get # events";
const char string_7[17] = "Get Bias 1";
const char string_8[18] = "Get Bias 2";

const byte NUM_ENTRIES = 9;

  

// these variables remember the current state of the buttons, in order to compare 
// to see if a button press has occured (change of state)
// true = button pressed
// fale = button not pressed
bool button_state_1=0;
bool button_state_2=0;
bool button_state_3=0;
bool button_state_4=0;

void do_nothing() {}

// These function pointers are run whenever the button is pressed (only runs once per press), see the logic in the loop function.
// Depending on the state of the program, these function pointers are assigned to different function
// Eg does something different when right pressing when viewing the main menu compared to when incrementing a value which will be set later
// on
void (*f_button_ok) ()    = do_nothing;
void (*f_button_back) ()  = do_nothing;
void (*f_button_left) ()  = do_nothing;
void (*f_button_right) () = do_nothing;
void menu_ok();
void menu_back();
void menu_left();
void menu_right();
void trigger_ok();
void trigger_back();
void trigger_left();
void trigger_right();

void get_ok();
void get_back();
void get_left();
void get_right();


void send_trigger_command();
void send_get_command();
void send_set_command();

void wait_server_response();
void process_trigger_response();
void process_get_response();
void process_set_response();

// what this state is depends on the context
// in main menu is useless
// in selected mode it counts the index of which one we're actually at
// in get or trigger it tracks whether currently on yes or no option (0 or 1)
// in set mode it is set to the value the user has selected
// and then yes or no for later on. 
// basically the entire state of the system should be used here
byte state = 0;



// Trigger tells the the RPI to start a certain action, no values involved
// Get asks the the RPI for some value and displays it on the screen
// Set tells the RPI to set the value to some parameter, user can control number onscreen
// BUILTIN is some action that does not involve the Raspberry Pi
enum ENTRY_TYPE {
  TRIGGER=0,
  GET=1,
  SET=3,
  BUILTIN=2
};



// This is the main entry data structure
// Each entry is has a name which is displayed in the ui
// the type determines what kind of action the Entry performs
// currently we have have trigger, get, set, builtin
// trigger just tells the digitizer to launch something
// get asks the digitizer for some value and displays it
// set gives the digitizer a new parameter value which the user chooses
// builtin is stuff internal to the control module that doesn't involve interacting with the raspberry pi
// maybe including stuff like shudown/sleep, display stats (though this might need to interact with raspberry pi)
// change display brightness/contrast.
struct Entry_s {
  char* name;
  char id;
  char type;
};

typedef struct Entry_s Entry;


const Entry entries[] =
  {
    {.name = string_0, .id=25, .type=GET},
    {.name = string_1, .id=1  , .type=TRIGGER},
    {.name = string_2, .id=24, .type=GET},
    {.name = string_3, .id=30, .type=GET},
    {.name = string_4, .id=31, .type=GET},
    {.name = string_5, .id=32, .type=GET},
    {.name = string_6, .id=33, .type=GET},
    {.name = string_7, .id=19, .type=GET},
    {.name = string_8, .id=21, .type=GET}
  };

//This is the text buffer that the RPi writes serial into
char buffer[32];

// Serial communications stuff added here
void codify_name(char* og, char* buf) {
  strcpy(buf, og);
  char* p = buf;
  strlwr(buf);
  for( ; *p; ++p) {
    if (*p == ' ') {
      *p='-';
    }
  }
}

void display_freeram() {
  Serial.print(F("- SRAM left: "));
  Serial.println(freeRam());
}


int freeRam() {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}


void send_get_command(char* name, byte id) {
  Serial.print(":GET <");
  Serial.print(id, DEC);
  Serial.print("> [");
  Serial.print(name);
  Serial.println("]");
}

void send_trigger_command(char* name, byte id) {
  Serial.print(":TRIGGER <");
  Serial.print(id, DEC);
  Serial.print("> [");
  Serial.print(name);
  Serial.println("]");
}

void send_set_command(char* name, byte val, byte id) {
  Serial.print(":SET <");
  Serial.print(id, DEC);
  Serial.print("> [");
  Serial.print(name);
  Serial.print("] ");
  Serial.println(val, DEC);
}


void wait_server_response() {
  memset(buffer, 0, sizeof buffer);
  Serial.readBytesUntil('\n', buffer, 64);
}


//if true returns length of the string match
int starts_with(char* main, char* stub) {
  int i=0;
  while(1) {
    if (stub[i] == 0) {
      return i;
    }
    if (stub[i] != main[i] || main[i] == 0) {
      return false;
    }

    i+=1;
  }
}



bool contains(char* main, char* stub) {
  for(int i = 0; main[i] != 0; i++) {
    if (starts_with(main+i, stub)) {
      return true;
    }
  }
  return false;
}

bool between(char* main, char left, char right, byte* start, byte* end) {
//  Serial.println("Heere");
//  Serial.println(main);
  bool started = false;
  
  for(int i=0; main[i]; i++) {
//    Serial.println(main+i);
    if (!started && main[i]==left) {
      started=true;
      *start=i+1;
    } else{
      if(started && main[i]==right) {
         *end=i;
          return true;
      }
    }
  }

  return false;
}


// 2-bits
enum SERVER_MESSAGE_TYPE {
  SERVER_RESPOND=1,
  SERVER_UPDATE,
  SERVER_PUSH
};

// 2-bits
enum SERVER_RESPONSE_TYPE {
  RESPOND_TRIGGER=1,
  RESPOND_GET,
  RESPOND_SET
};


//only matches positive numbers
short process_id(char *buf) {
  byte id; 
  byte r = sscanf(buf, "<%d>", &id);
  if (r == 1) {
    return id;
  } else {
    return -1;
  }
}


int process_server_get_response(byte* left, byte* right) {
    byte id;
    byte a, b, c;
    unsigned int result;

      a = starts_with(buffer, ":RESPOND GET ");
  if (a) {
    result = SERVER_RESPOND << 14;
    result += RESPOND_GET << 12;
    id = process_id(buffer+a);
    
    if(id<1) {
      return -1;
    }
    result+=id;
//    Serial.println(&buffer[a]);
//    S/erial.println(id);
//    Se/rial.println(strlen(buffer+a));
    if(contains(buffer+a, "FAIL")) {
      result |= (1<<8);
      return result;
    }

//    Serial.println(F("Made it past fail"));

//    Serial.println(&buffer[a]);
//    Serial.println(&buffer[a]);
    if(between(buffer+a, '"', '"', &b, &c)) {
      result &= ~(1<<8);
//      Serial.println(a+b);
//      Serial.println(a+c);
      *left=a+b;
      *right=a+c;
      return result;
    }
  }
  

  return -1;
}

bool is_server_trigger_response(int message) {
  return (message & (0b1111 << 12)) == ((SERVER_RESPOND<<14) + (RESPOND_TRIGGER << 12));
}

bool is_server_set_response(int message) {
  return (message & (0b1111 << 12)) == ((SERVER_RESPOND<<14) + (RESPOND_SET << 12));
}

bool is_server_get_response(int message) {
  return (message & (0b1111 << 12)) == ((SERVER_RESPOND<<14) + (RESPOND_GET << 12));
}

void display_get_result_message(int message, byte start, byte end) {
  if (is_server_get_response) {
///

    buffer[end]=0;
    Serial.println(buffer+start);
    buffer[end]='"';
  }
}

void lcd_display_get_result_message(int message, byte start, byte end) {
  if (is_server_get_response) {
///
//    Serial.print(start); Serial.print(" "); Serial.println(end);
    buffer[end]=0;
    lcd.print(buffer+start);
//    Serial.println(buffer+start);
    buffer[end]='"';
  }
}







































// Serial communication stuff ends here.

// these are the entries which are displayed by the control module
// currently these are hard coded into Arduino so microcontroller needs to be 
// reflashed when trying to change the actions which the arduino can perform
// this is due to SRAM memory constraints, keeping it static allows us to use program flash.
// we'll see if in the future we can make it dynamic whilst staying in the SRAM budget.
// TODO: want to use PROGMEM flash memory to hold the strings in order to use up less data, 
//maybe even each entry could be progmemed, 
//but I'm not sure if that would be beneficial, since it is a lot slower I think and 
//Entry datatype is relatively space efficient now.

// Width of the LCD display 
const char WIDTH = 16;
// HEight of LCD display
const char HEIGHT = 2;


// Display a single entry in the menu in the first line
// On the second line < and > are present when the user is able to click the 
//left button or the right button to reach a new option
void redisplay_menu() {
  lcd.clear();

// if entry is not the first entry in the menu, add '<' indicator 
// to show user that there are options available to the left
  if (state) {
    lcd.setCursor(0, 1);
    lcd.print(F("<")); 
  }
// if entry is not the last entry in the menu, add '>' indicator
// to show user that there are options available to the right.
  if (state < NUM_ENTRIES-1) {
    lcd.setCursor(15, 1);
    lcd.print(F(">"));
  }
//  Printing the name is done last so the cursor is still placed at the end of the word
// Doing this makes it easy to add a question mark when switching from the menu vue to the 
//trigger vue since the cursor is already in the right spot
    lcd.setCursor(0, 0);
    lcd.print(entries[state].name);
}

// Display one of the selected trigger on the screen: 
// On the top line, name of the trigger is displayed
// on the second line yes or no can be selected using the buttons.
void redisplay_trigger() {
  char mask = 0b10000000 & state;
  if (mask) {
    lcd.setCursor(0, 1);
    lcd.print(F(" "));
    lcd.setCursor(5, 1);
    lcd.print(F(">"));
  } else {
    lcd.setCursor(0, 1);
    lcd.print(F(">"));
    lcd.setCursor(5, 1);
    lcd.print(F(" "));
  }
}

void display_trigger() {
  f_button_ok = trigger_ok;
  f_button_back = trigger_back;
  f_button_left = trigger_left;
  f_button_right = trigger_right;
  
  lcd.print(F("?"));
  lcd.setCursor(0, 1);
  lcd.print(F(" Yes  No        "));
  

  redisplay_trigger();
}

void redisplay_get() {
  char mask = 0b10000000 & state;
  if (mask) {
    lcd.setCursor(0, 1);
    lcd.print(F(" "));
    lcd.setCursor(5, 1);
    lcd.print(F(">"));
  } else {
    lcd.setCursor(0, 1);
    lcd.print(F(">"));
    lcd.setCursor(5, 1);
    lcd.print(F(" "));
  }
}

void display_get() {
//  Serial.println("I am here");
  f_button_ok = get_ok;
  f_button_back = get_back;
  f_button_left = get_left;
  f_button_right = get_right;
  lcd.print(F("?"));
  lcd.setCursor(0, 1);
  lcd.print(F(" Yes  No        "));

  redisplay_get();
}

void display_entry() {
  int type = entries[state].type;
//  Serial.println(type);
  if (type==TRIGGER) {
    display_trigger();
  } else {
    if(type==GET) {
      display_get();
    } else{
      //    TODO implement
//    Serial.println(F("other types of entries not implemented yet"));
//    Serial.println(type);
    }

  }
}

void display_menu() {
  state = 0;
  f_button_ok = menu_ok;
  f_button_back = menu_back;
  f_button_left = menu_left;
  f_button_right = menu_right;
  redisplay_menu();
}

void menu_ok() {
  display_entry();
}

void menu_back() {
  Serial.println(F("Menu back doesn't do anything"));
}

void menu_right() {
  if (state < NUM_ENTRIES-1) {
    state +=1;
    redisplay_menu();
  }
//  state = (state + 1) % NUM_ENTRIES;
}

void menu_left() {
  if (state > 0) {
    state -= 1;
    redisplay_menu();
  }
}

void trigger_ok() {
//  Serial.println(F("Trigger ok not implemented"));
  lcd.clear();
  byte left, right;
  Entry* e = entries+state;
  send_trigger_command(e->name, e->id);
  wait_server_response(); 
  display_menu();
}

void trigger_back() {
  f_button_ok    = menu_ok;
  f_button_back  = menu_back;
  f_button_left  = menu_left;
  f_button_right = menu_right;
  redisplay_menu();
}

void trigger_left() {
  if (1<<7 & state) {
    Serial.println(state, 2);
    state-=1<<7;
    Serial.println(state, 2);
    redisplay_trigger();
  }
}

void trigger_right() {
  if (!(1<<7 & state)) {
    Serial.println(state, 2);
    state+=1<<7;
    Serial.println(state, 2);
    redisplay_trigger();
  }
}

void get_left() {
  if (1<<7 & state) {
//    Serial.println(state, 2);
    state-=1<<7;
    redisplay_get();
  }
}

void get_right() {
  if(!(1<<7 & state)) {
    state+=1<<7;
    redisplay_trigger();
  }
}

void get_ok() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  byte left, right;
  Entry* e = entries+state;
  send_get_command(e->name, e->id);
  wait_server_response(); 
  int message = process_server_get_response(&left, &right);
//  lcd.clear();
//  lcd.print(buffer);
  Serial.println(buffer);
  Serial.println(message, BIN);
  if(is_server_get_response(message)) {
    Serial.println("In get_ok is get_response");
    lcd.print("> ");
    lcd_display_get_result_message(message, left, right);
//    Serial.print(buffer+10);
  }
//  lcd.print("Hooray!");
}

void get_back() {
  f_button_ok    = menu_ok;
  f_button_back  = menu_back;
  f_button_left  = menu_left;
  f_button_right = menu_right;
  redisplay_menu();
}

bool just_pressed(int button_pin, bool *previous_state) {
  bool new_state=!digitalRead(button_pin);
  if(new_state != *previous_state) {
    *previous_state = new_state;
    return new_state;
  }
  return false;
}

void lcd_init() {
//enable the supplies
  pinMode(VSS_PIN, OUTPUT);
  pinMode(VDD_PIN, OUTPUT);
  pinMode(ANODE_PIN, OUTPUT);
  pinMode(CATHODE_PIN, OUTPUT);
  digitalWrite(VSS_PIN, LOW);
  digitalWrite(VDD_PIN, HIGH);
  digitalWrite(ANODE_PIN, HIGH);
  digitalWrite(CATHODE_PIN, LOW);

//  initialize the lcd class with the dimensions.
  lcd.begin(WIDTH, HEIGHT);
}

void setup() {
  Serial.begin(9600); // initialize the Serial Port
  Serial.setTimeout(100*1000);
      
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);
  pinMode(BUTTON_4, INPUT);
  button_state_1=0;
  button_state_2=0;
  button_state_3=0;
  button_state_4=0;
  
  while(!Serial) {} // Wait for the USB FTDI serial connection to start
  lcd_init(); // Initizalize communications to LCD screen

//  Serial.println(sizeof(f_button_ok));
//  Serial.println(sizeof(Entry));
//  Serial.println(sizeof(char));
  display_menu();
}

void loop() {
  bool new_state;
  if (just_pressed(BUTTON_1, &button_state_1)) {
    f_button_left();
  }
  if (just_pressed(BUTTON_2, &button_state_2)) {
    f_button_right();
  }
  if (just_pressed(BUTTON_3, &button_state_3)) {
    f_button_ok();
  }
  if (just_pressed(BUTTON_4, &button_state_4)) {
    f_button_back();
  }
}
