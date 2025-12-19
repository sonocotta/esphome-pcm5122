#pragma once

namespace esphome
{
  namespace pcm5122
  {

    enum ControlState : uint8_t
    {
      CTRL_PWDN = 0x01,       // Power down
      CTRL_STBY = 0x10,       // Stand by
      CTRL_PLAY = 0x00,       // Play
    };

    struct Pcm5122Configuration
    {
      uint8_t offset;
      uint8_t value;
    } __attribute__((packed));

    #define PCM51XX_REG_00 0x00
    #define PCM51XX_REG_STATE 0x02
    #define PCM51XX_REG_MUTE 0x03
    #define PCM51XX_REG_VALUE_MUTE 0x11
    #define PCM51XX_REG_VALUE_UNMUTE 0x00
    
    #define PCM51XX_REG_24 0x24
    #define PCM51XX_REG_25 0x25
    #define PCM51XX_REG_26 0x26
    #define PCM51XX_REG_27 0x27
    #define PCM51XX_REG_28 0x28
    #define PCM51XX_REG_29 0x29
    #define PCM51XX_REG_2A 0x2a
    #define PCM51XX_REG_2B 0x2b
    #define PCM51XX_REG_35 0x35
    #define PCM51XX_REG_7E 0x7e
    #define PCM51XX_REG_7F 0x7f

    #define PCM51XX_PAGE_00 0x00
    #define PCM51XX_PAGE_2A 0x2a

    #define PCM51XX_BOOK_00 0x00
    #define PCM51XX_BOOK_8C 0x8c

    #define PCM51XX_REG_VOL_L 0X3D
    #define PCM51XX_REG_VOL_R 0X3E
   
    #define PCM5122_CFG_META_DELAY 0xFF

    typedef struct
    {
      uint8_t offset;
      uint8_t value;
    } pcm51xx_cfg_reg_t;

    static const pcm51xx_cfg_reg_t pcm51xx_init_seq[] = {

        // EXIT SHUTDOWN STATE
        {0x00, 0x00}, // SELECT PAGE 0
        {0x03, 0x00}, // UNMUTE           TODO: replace with mute control
        {0x2a, 0x11}, // DAC DATA PATH L->ch1, R->ch2
        {0x02, 0x00}, // DISABLE STBY.    TODO: replace with power mode
        {0x0d, 0x10}, // BCK as SRC for PLL
        {0x25, 0x08}, // IGNORE MISSING MCLK
        {0x3d, 0x30}, // DIGITAL VOLUME L TODO: verify if needed
        {0x3e, 0x30}, // DIGITAL VOLUME R TODO: verify if needed
    };

  } // namespace pcm5122
} // namespace esphome