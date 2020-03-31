#Si5351 Frequency Synthesizer/Signal Generator

This arduino application uses Adafruit Si5351 breakout board to generate frequencies from 8 Khz - 160 Mhz

#Hardware required:
1. Arduino Nano (or any arduino)
2. Adafruit si5351 breakout board (or any other available)
3. 128 x 64 OLED
4. Rotary encoder

#Hardware connections:
ENCODER -- ARDUINO:
     s1 -- pin d4
     s2 -- pin d5
    key -- pin d3
    vcc -- +5v
   
   OLED -- ARDUINO:
    vcc -- +5v
    scl -- a5
    sda -- a4
 
 S15351 -- ARDUINO:
    vin -- +5v
    scl -- a5
    sda -- a4
 
#Other libraries used:
1. Arduino rotary library
2. Adafruit_SSD1306
3. Adafruit_GFX
4. Wire
5. Si5351 - https://github.com/pavelmc/Si5351mcu.git

#How to use the app:
There are only two inputs to arduino:
1. Rotary encoder
2. Push button in rotary encoder

Rotate      : To move to the next option or increase(clockwise) / decrease(counter clockwise) frequency.
Push button : To modify current selection

Unit/Step size : Hz/KHz/MHz/Off

Note: At a time only one of Ch1/Ch2 can be active