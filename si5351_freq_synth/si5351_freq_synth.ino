/*
 * Author: Ruchir Chauhan (KI7HFB)
 * Date: March 23rd 2020
 * Functionality: Signal generator
 *
 *
 * Hardware connections:
 * ENCODER -- ARDUINO:
 *      s1 -- pin d4
 *      s2 -- pin d5
 *     key -- pin d3
 *     vcc -- +5v
 *    
 *    OLED -- ARDUINO:
 *     vcc -- +5v
 *     scl -- a5
 *     sda -- a4
 *  
 *  S15351 -- ARDUINO:
 *     vin -- +5v
 *     scl -- a5
 *     sda -- a4 
 *
 * Notes:
 * 1. At a time only one of Ch1 or ch2 can be active.
 *    When one is active, library automatically disables the other.
 *    However, this code also does it, but only for our bookeeping and display.
 * 2. SI5351 Library used: https://github.com/pavelmc/Si5351mcu.git
 */

#include <Wire.h>
#include <Rotary.h>
#include <si5351mcu.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* Rotary encoder related */
#define ROT_ENC_PIN_1     (4)           // D4
#define ROT_ENC_PIN_2     (5)           // D5
#define ROT_ENC_PIN_PRESS (3)           // D3

/* OLED related */
#define SCREEN_WIDTH      (128)         // OLED display width, in pixels
#define SCREEN_HEIGHT     (64)          // OLED display height, in pixels
#define OLED_ADDR         (0x3C)
#define OLED_RESET        (-1)           // Reset pin # (or -1 if sharing Arduino reset pin)

/* Si5351 related */
#define XTAL_FREQ         (25000000L)
#define CH_OFF            (0)           // This is meant only for display and my book keeping, si5351 library does it automatically
#define MIN_FREQ          (8000UL)      // 8 KHz
#define MAX_FREQ          (160000000UL) // 160 MHz

/* State Machine, event misc enums */
typedef enum eChannelModifyState{
  Ch0_Select_State ,
  Ch1_Select_State ,
  Ch2_Select_State ,
  Ch_Modify_State  ,
  Freq_Select_State,
  Unit_Select_State,
  Freq_Modify_State,
  Unit_Modify_State,
  CMS_Default
}eCMS;

typedef enum {
  Rot_CW_Event ,
  Rot_CCW_Event,
  Rot_KEY_Event,
  State_Entry  ,
  Rot_Default
}eRotEncEvent;

typedef enum {
  Ch_None=-1,
  Ch0,
  Ch1,
  Ch2
}eChannel;

typedef enum {
  Hz ,   // 0 000 001
  KHz,  // 0 001 000
  MHz,   // 1 000 000
  Off
}eFreqUnit;
unsigned long unit_multiplier[3] = {1,1000,1000000};
/* State Machine and event enums End */

/* Globals */
Si5351mcu Si;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Rotary r = Rotary(ROT_ENC_PIN_1, ROT_ENC_PIN_2);

eChannel selected_channel        = Ch_None;
eFreqUnit ch_unit[3]             = {KHz, Off, KHz};
unsigned long ch_freq[3]         = {MIN_FREQ,CH_OFF,MIN_FREQ}; // 0 008 000 Hz
eCMS cms_current_state           = Ch0_Select_State;
bool key_event_detected          = false;
int ch_sel_cursor                = 0;
int ch_mod_cursor                = -1;
/* Globals End */


void setup() {
  setup_lcd();
  setup_rotary_encoder();
  setup_si5351();
  update_display();
}

void loop() {

  unsigned char rot_event = r.process();
  eRotEncEvent new_event = Rot_Default;

  if (rot_event) {
    new_event = (rot_event == DIR_CW ? Rot_CW_Event : Rot_CCW_Event);
  } else if (key_event_detected) {
    new_event = Rot_KEY_Event;
    key_event_detected = false;
  }

  if (new_event != Rot_Default) {
    sm_handler(new_event);
    new_event = Rot_Default;
  }

}

/* Setup Functions */
void setup_computer_serial_port() {
  Serial.begin(9600);
//  Serial.println("Freq Synthesizer");
}

void setup_si5351() {
  Si.init(XTAL_FREQ); // Default xtal freq in this library is 27Mhz, so change it to 25Mhz
  // Si.correction(); //Experiment and see xtal correction factor
  update_si5351_all_ch();
}

void setup_rotary_encoder() {
  pinMode(ROT_ENC_PIN_PRESS, INPUT);
  attachInterrupt(digitalPinToInterrupt(ROT_ENC_PIN_PRESS),
                  rot_enc_key_press_event, FALLING);
  // Serial.println("Rotary Encoder up");
}

void setup_lcd() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { // Address 0x3C for 128x64
    // Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();
}

void update_display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(18,0);
  display.print("FREQ SYNTHESIZER");

  const int blk_width = 128;
  const int blk_height = 15;
  const int blk_x = 0;
  const int blk_y[3] = {16,30,44};
  const int txt_row_x = 4;
  int txt_row_y;
  unsigned long whole_part;
  unsigned long decimal_part;

  for(int i=Ch0; i<=Ch2; i++) {
    display.setTextColor(WHITE);
    txt_row_y = blk_y[i]+4;
    whole_part = ch_freq[i] / unit_multiplier[ch_unit[i]];
    decimal_part = ch_freq[i] % unit_multiplier[ch_unit[i]];

    // Column-1 (Channel number)
    if (selected_channel == i && ch_mod_cursor == 0) {
      display.setTextColor(BLACK, WHITE);
    }
    display.setCursor(txt_row_x, txt_row_y);
    display.print("Ch" + (String)i);
    display.setTextColor(WHITE);

    // Column-2 (Channel Freq)
    if (selected_channel == i && ch_mod_cursor == 1) {
      display.setTextColor(BLACK, WHITE);
    }
    display.setCursor(txt_row_x+25, txt_row_y);
    if (ch_unit[i] == Off){
      display.print("DISABLED");
    } else{
      display.print(String(whole_part) + "." + String(decimal_part));
    }
    display.setTextColor(WHITE);

    // Column-3 (Channel Unit)
    if (selected_channel == i && ch_mod_cursor == 2) {
      display.setTextColor(BLACK, WHITE);
    }
    display.setCursor(txt_row_x+100, txt_row_y);
    display.print(String(get_frq_unit_name(ch_unit[i])));
  }

  display.setTextColor(WHITE);
  display.drawRoundRect(blk_x, blk_y[ch_sel_cursor], blk_width, blk_height, 4, WHITE);

  // update display with all of the above graphics
  display.display();
}
/* Setup Functions End */

/* Interrupt Handlers */
void rot_enc_key_press_event() {
  key_event_detected = true;
}
/* Interrupt Handlers End */

/* StateMachine and State Handlers */
void sm_handler(eRotEncEvent event) {
  eCMS last_state = cms_current_state;

  static eCMS (*st_handler[])(eRotEncEvent) = {
    ch0_select_handler,
    ch1_select_handler,
    ch2_select_handler,
    ch_modify_handler,
    freq_select_handler,
    unit_select_handler,
    freq_modify_handler,
    unit_modify_handler
  };
  if (cms_current_state < sizeof(st_handler)/sizeof(*st_handler)) {
    cms_current_state = st_handler[cms_current_state](event);
  }
  /*
  Serial.println("Received ["+(String)get_rot_event_name(event)+"] in ["+
                 (String)get_cms_state_name(last_state)+"] -> ["+
                 (String)get_cms_state_name(cms_current_state)+"]");
  */
  (void)st_handler[cms_current_state](State_Entry);
  update_display();

}

eCMS ch0_select_handler(eRotEncEvent event) {
  eCMS next_state = Ch0_Select_State;

  switch (event) {
  case State_Entry:
    ch_sel_cursor = 0;
    break;
  case Rot_CW_Event:
    next_state = Ch1_Select_State;
    break;
  case Rot_CCW_Event:
    next_state = Ch2_Select_State;
    break;
  case Rot_KEY_Event:
    next_state = Ch_Modify_State;
    select_channel(Ch0);
    break;
  default:
    break;
  }

  return next_state;
}

eCMS ch1_select_handler(eRotEncEvent event) {
  eCMS next_state = Ch1_Select_State;

  switch (event) {
  case State_Entry:
    ch_sel_cursor = 1;
    break;
  case Rot_CW_Event:
    next_state = Ch2_Select_State;
    break;
  case Rot_CCW_Event:
    next_state = Ch0_Select_State;
    break;
  case Rot_KEY_Event:
    next_state = Ch_Modify_State;
    select_channel(Ch1);
    break;
  default:
    break;
  }

  return next_state;
}

eCMS ch2_select_handler(eRotEncEvent event) {
  eCMS next_state = Ch2_Select_State;

  switch (event) {
  case State_Entry:
    ch_sel_cursor = 2;
    break;
  case Rot_CW_Event:
    next_state = Ch0_Select_State;
    break;
  case Rot_CCW_Event:
    next_state = Ch1_Select_State;
    break;
  case Rot_KEY_Event:
    next_state = Ch_Modify_State;
    select_channel(Ch2);
    break;
  default:
    break;
  }

  return next_state;
}

eCMS ch_modify_handler(eRotEncEvent event) {
  eChannel ch = selected_channel;
  eCMS next_state = Ch_Modify_State;

  switch (event) {
  case State_Entry:
    ch_mod_cursor = 0;
    break;
  case Rot_CW_Event:
    next_state = Freq_Select_State;
    break;
  case Rot_CCW_Event:
    next_state = Unit_Select_State;
    break;
  case Rot_KEY_Event:
    if (ch==Ch0) {
      next_state = Ch0_Select_State;
    } else if (ch==Ch1) {
      next_state = Ch1_Select_State;
    } else if (ch==Ch2) {
      next_state = Ch2_Select_State;
    }
    update_si5351_sel_ch();
    selected_channel = Ch_None;
    ch_mod_cursor = -1;
    break;
  default:
    break;
  }

  return next_state;
}

eCMS freq_select_handler (eRotEncEvent event) {
  eCMS next_state = Freq_Select_State;

  switch (event) {
  case State_Entry:
    ch_mod_cursor = 1;
    break;
  case Rot_CW_Event:
    next_state = Unit_Select_State;
    break;
  case Rot_CCW_Event:
    next_state = Ch_Modify_State;
    break;
  case Rot_KEY_Event:
    next_state = Freq_Modify_State;
    break;
  default:
    break;
  }

  return next_state;
}

eCMS unit_select_handler (eRotEncEvent event) {
  eCMS next_state = Unit_Select_State;

  switch (event) {
  case State_Entry:
    ch_mod_cursor = 2;
    break;
  case Rot_CW_Event:
    next_state = Ch_Modify_State;
    break;
  case Rot_CCW_Event:
    next_state = Freq_Select_State;
    break;
  case Rot_KEY_Event:
    next_state = Unit_Modify_State;
    break;
  default:
    break;
  }

  return next_state;
}

eCMS freq_modify_handler (eRotEncEvent event) {
  eCMS next_state = Freq_Modify_State;
  eChannel tmp_ch = selected_channel;
  unsigned long unit_mul = unit_multiplier[ch_unit[tmp_ch]];
  unsigned long tmp_freq = ch_freq[tmp_ch];

  switch (event) {
  case State_Entry:
    ch_mod_cursor = 1;
    break;
  case Rot_CW_Event:
    // Increment freq based on unit
    tmp_freq += unit_mul;
    break;
  case Rot_CCW_Event:
    // Decrement freq based on unit
    tmp_freq -= unit_mul;
    break;
  case Rot_KEY_Event:
    next_state = Freq_Select_State; // Set freq and return to freq select state
    break;
  default:
    break;
  }

  if (tmp_freq < (unsigned long)MIN_FREQ) {
    set_min_freq(tmp_ch);
  } else if (tmp_freq > (unsigned long)MAX_FREQ) {
    set_max_freq(tmp_ch);
  } else {
    ch_freq[tmp_ch] = tmp_freq;
  }

  update_si5351_sel_ch();

  return next_state;
}

eCMS unit_modify_handler (eRotEncEvent event) {
  eCMS next_state = Unit_Modify_State;

  switch (event) {
  case State_Entry:
    ch_mod_cursor = 2;
    break;
  case Rot_CW_Event:
    change_ch_unit(1);
    break;
  case Rot_CCW_Event:
    change_ch_unit(-1);
    break;
  case Rot_KEY_Event:
    next_state = Unit_Select_State;
    break;
  default:
    break;
  }

  return next_state;
}
/* State Handlers End */

/* Helper Functions */
void change_ch_unit(int direction) {
  int selected_unit = ch_unit[selected_channel] + direction;
  if (selected_unit < Hz) {
    selected_unit = Off;
  } else {
    selected_unit %= (Off+1);
  }
  ch_unit[selected_channel] = (eFreqUnit)selected_unit;
}

void select_channel(eChannel channel) {
  selected_channel = channel;
  if (ch_freq[selected_channel] == CH_OFF) {
    ch_freq[selected_channel] = MIN_FREQ;
    ch_unit[selected_channel] = KHz;
  }
  if (channel == Ch1) {
    ch_freq[Ch2] = CH_OFF;
    ch_unit[Ch2] = Off;
  } else if (channel == Ch2) {
    ch_freq[Ch1] = CH_OFF;
    ch_unit[Ch1] = Off;
  }
}

void set_min_freq(eChannel channel) {
  ch_freq[channel] = (unsigned long)MIN_FREQ;
  ch_unit[channel] = KHz;
}

void set_max_freq(eChannel channel) {
  ch_freq[channel] = (unsigned long)MAX_FREQ;
  ch_unit[channel] = MHz;
}
/* Helper Functions End */

/* SI5351 Update Functions */
void update_si5351_all_ch() {
  for(int ch=Ch0; ch<=Ch2; ch++) {
    if (ch_unit[ch] != Off) {
      Si.setPower(ch, SIOUT_8mA);
      Si.setFreq(ch, ch_freq[ch]);
      Si.enable(ch);
      // Si.reset();
    }
  }
}

void update_si5351_sel_ch() {
  if (ch_unit[selected_channel] != Off) {
    Si.setPower(selected_channel, SIOUT_8mA);
    Si.setFreq(selected_channel, ch_freq[selected_channel]);
    Si.enable(selected_channel);
  } else {
    ch_freq[selected_channel] = 0;
    Si.disable(selected_channel);
  }
  Si.reset(); //reset PLLs: not sure if we really need to do this, good explanation here: https://groups.io/g/BITX20/topic/si5351a_facts_and_myths/5430607
}

void update_si5351_ch(eChannel channel) {
  if (ch_unit[selected_channel] != Off) {
    Si.setPower(channel, SIOUT_8mA);
    Si.setFreq(channel, ch_freq[channel]);
    Si.enable(channel);
  } else {
    ch_freq[channel] = 0;
    Si.disable(channel);
  }
}
/* SI5351 Update Functions End*/


/* Enum translators */
#define ENUM_PAIR(x)  { (x), #x }
#define ARRAY_ELEMENT_COUNT(x) (sizeof(x) / sizeof(x[0]))
const char* const p_empty_string = "";

char const* get_cms_state_name(eCMS e_state_id) {
  char const* p_ret_name = p_empty_string;
  static struct {
    int enum_id;
    const char* enum_txt;
  } st[] = {
    ENUM_PAIR(Ch0_Select_State ),
    ENUM_PAIR(Ch1_Select_State ),
    ENUM_PAIR(Ch2_Select_State ),
    ENUM_PAIR(Ch_Modify_State  ),
    ENUM_PAIR(Freq_Select_State),
    ENUM_PAIR(Unit_Select_State),
    ENUM_PAIR(Freq_Modify_State),
    ENUM_PAIR(Unit_Modify_State)
  };
  int const n_count = ARRAY_ELEMENT_COUNT(st);
  if (n_count != CMS_Default) {
//    Serial.println("cms enum translator failed");
    return p_ret_name;
  }
  for (int i=0; i<n_count; i++) {
    if (st[i].enum_id == e_state_id) {
      p_ret_name = st[i].enum_txt;
      break;
    }
  }
//  if (p_ret_name == p_empty_string) {
//    Serial.println("Missing entry for state ");
//    Serial.print(e_state_id);
//    Serial.println();
//  }
  return p_ret_name;
}
char const* get_rot_event_name(eRotEncEvent event_id) {
  char const* p_ret_name = p_empty_string;
  static struct {
    int enum_id;
    const char* enum_txt;
  } st[] = {
    ENUM_PAIR(Rot_CW_Event ),
    ENUM_PAIR(Rot_CCW_Event),
    ENUM_PAIR(Rot_KEY_Event),
    ENUM_PAIR(State_Entry)
  };
  int const n_count = ARRAY_ELEMENT_COUNT(st);
  if (n_count != Rot_Default) {
//    Serial.println("rot enc enum translator failed");
    return p_ret_name;
  }
  for (int i=0; i<n_count; i++) {
    if (st[i].enum_id == event_id) {
      p_ret_name = st[i].enum_txt;
      break;
    }
  }
//  if (p_ret_name == p_empty_string) {
//    Serial.print("Missing entry for state ");
//    Serial.print(event_id);
//    Serial.println();
//  }
  return p_ret_name;
}
char const* get_frq_unit_name(eFreqUnit eval) {
  char const* p_ret_name = p_empty_string;
  static struct {
    int enum_id;
    const char* enum_txt;
  } st[] = {
    ENUM_PAIR(Hz ),
    ENUM_PAIR(KHz),
    ENUM_PAIR(MHz),
    ENUM_PAIR(Off)
  };
  int const n_count = ARRAY_ELEMENT_COUNT(st);
  if (n_count != 4) {
//    Serial.println("freq unit enum translator failed");
    return p_ret_name;
  }
  for (int i=0; i<n_count; i++) {
    if (st[i].enum_id == eval) {
      p_ret_name = st[i].enum_txt;
      break;
    }
  }
//  if (p_ret_name == p_empty_string) {
//    Serial.print("Missing entry for enum ");
//    Serial.print(eval);
//    Serial.println();
//  }
  return p_ret_name;
}
/* Enum translators End */

/* Misc */
/* Misc End */
