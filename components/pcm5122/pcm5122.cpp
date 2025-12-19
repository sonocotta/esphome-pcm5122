#include "pcm5122.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome
{
  namespace pcm5122
  {

    static const char *const TAG = "pcm5122";
    static const char *const ERROR = "Error ";
    static const char *const MIXER_MODE = "Mixer Mode";

    static const char* const MIXER_MODE_TEXT[] = {"STEREO", "STEREO_INVERSE", "RIGHT", "LEFT"};

    // maximum delay allowed in "pcm5122_minimal.h" used in configure_registers()
    static const uint8_t ESPHOME_MAXIMUM_DELAY = 5; // milliseconds

    void Pcm5122Component::setup()
    {
      ESP_LOGCONFIG(TAG, "Running setup");
      if (this->enable_pin_ != nullptr)
      {
        this->enable_pin_->setup();
        this->enable_pin_->digital_write(false);
        delay(10);
        this->enable_pin_->digital_write(true);
      }

      if (!configure_registers_())
      {
        this->error_code_ = CONFIGURATION_FAILED;
        this->mark_failed();
      }

      // rescale -103db to 24db digital volume range to register digital volume range 254 to 0
      this->pcm5122_state_.raw_volume_max = (uint8_t)((this->pcm5122_state_.volume_max - 24) * -2);
      this->pcm5122_state_.raw_volume_min = (uint8_t)((this->pcm5122_state_.volume_min - 24) * -2);
    }

    bool Pcm5122Component::configure_registers_()
    {
      uint16_t i = 0;
      uint16_t counter = 0;
      uint16_t number_configurations = sizeof(pcm51xx_init_seq) / sizeof(pcm51xx_init_seq[0]);

      while (i < number_configurations)
      {
        switch (pcm51xx_init_seq[i].offset)
        {
        case PCM5122_CFG_META_DELAY:
          if (pcm51xx_init_seq[i].value > ESPHOME_MAXIMUM_DELAY)
            return false;
          delay(pcm51xx_init_seq[i].value);
          break;
        default:
          if (!this->pcm5122_write_byte_(pcm51xx_init_seq[i].offset, pcm51xx_init_seq[i].value))
            return false;
          counter++;
          break;
        }
        i++;
      }
      this->number_registers_configured_ = counter;

      if (!this->set_deep_sleep_off_())
        return false;

      if (!this->set_mixer_mode_(this->pcm5122_state_.mixer_mode))
        return false;

      if (!this->set_analog_gain_(this->pcm5122_state_.analog_gain))
        return false;

      if (!this->set_state_(CTRL_PLAY))
        return false;

      // initialise to now
      this->start_time_ = millis();
      return true;
    }

    void Pcm5122Component::loop()
    {
    }

    void Pcm5122Component::update()
    {
    }

    void Pcm5122Component::dump_config()
    {
      ESP_LOGCONFIG(TAG, "PCM5122 Audio Dac:");

      switch (this->error_code_)
      {
      case CONFIGURATION_FAILED:
        ESP_LOGE(TAG, "  %s setting up PCM5122: %i", ERROR, this->i2c_error_);
        break;
      case NONE:
        LOG_PIN("  Enable Pin: ", this->enable_pin_);
        LOG_I2C_DEVICE(this);
        ESP_LOGCONFIG(TAG,
                      "  Registers configured: %i\n"
                      "  Analog Gain: %idB\n"
                      "  Maximum Volume: %idB\n"
                      "  Minimum Volume: %idB\n"
                      "  Mixer Mode: %s\n",
  
                      this->number_registers_configured_,
                      this->pcm5122_state_.analog_gain,
                      this->pcm5122_state_.volume_max,
                      this->pcm5122_state_.volume_min,
                      MIXER_MODE_TEXT[this->pcm5122_state_.mixer_mode]
        );
        LOG_UPDATE_INTERVAL(this);
        break;
      }
    }

    // public
    void Pcm5122Component::enable_dac(bool enable)
    {
      enable ? this->set_deep_sleep_off_() : this->set_deep_sleep_on_();
    }

    bool Pcm5122Component::set_mute_off()
    {
      ESP_LOGD(TAG, "Mute OFF entered");
      if (!this->is_muted_)
        return true;

      if (!this->pcm5122_write_byte_(PCM51XX_REG_MUTE, PCM51XX_REG_VALUE_UNMUTE)) {
        ESP_LOGD(TAG, "%s unmute", ERROR);
        return false;
      }

      this->is_muted_ = false;
      ESP_LOGD(TAG, "Mute OFF done");
      return true;
    }

    bool Pcm5122Component::set_mute_on()
    {
      ESP_LOGD(TAG, "Mute ON entered");
      if (this->is_muted_)
        return true;

      if (!this->pcm5122_write_byte_(PCM51XX_REG_MUTE, PCM51XX_REG_VALUE_MUTE)) {
        ESP_LOGD(TAG, "%s mute", ERROR);
        return false;
      }

      this->is_muted_ = true;
      ESP_LOGD(TAG, "Mute ON done");
      return true;
    }

    float Pcm5122Component::volume()
    {
      uint8_t raw_volume;
      get_digital_volume_(&raw_volume);

      return remap<float, uint8_t>(raw_volume, this->pcm5122_state_.raw_volume_min,
                                   this->pcm5122_state_.raw_volume_max,
                                   0.0f, 1.0f);
    }

    bool Pcm5122Component::set_volume(float volume)
    {
      float new_volume = clamp(volume, 0.0f, 1.0f);
      uint8_t raw_volume = remap<uint8_t, float>(new_volume, 0.0f, 1.0f,
                                                 this->pcm5122_state_.raw_volume_min,
                                                 this->pcm5122_state_.raw_volume_max);
      if (!this->set_digital_volume_(raw_volume))
        return false;

      int8_t dB = -(raw_volume / 2) + 24;
      ESP_LOGD(TAG, "Volume: %idB", dB);
      return true;
    }

    // protected
    bool Pcm5122Component::get_analog_gain_(int8_t *gain_db)
    {
      *gain_db = this->pcm5122_state_.analog_gain;
      return true;
    }

    // Analog Gain Control -6 dB or 0 dB
    // OdB corresponds to 2V RMS output
    // -6dB corresponds to 1V RMS output
    bool Pcm5122Component::set_analog_gain_(int8_t gain_db)
    {
      if ((gain_db != 0) && (gain_db != -6))
        return false;

      if (!this->set_page_(PCM51XX_PAGE_ONE))
      {
        ESP_LOGE(TAG, "%s begin get %s", ERROR, "Analog Gain");
        return false;
      }
      uint8_t current = 0;
      if (!this->pcm5122_read_byte_(PCM51XX_REG_AGAIN, &current))
      {
        ESP_LOGE(TAG, "%s read %s", ERROR, "Analog Gain");
        return false;
      }

      if (gain_db == 0)
        // set 0-th and 5-th bit to 0 for 0dB
        current = current & 0xEE;
      else
        // set 0-th and 5-th bit to 1 for -6dB
        current = current | PCM51XX_REG_AGAIN_MINUS6DB;

      if (!this->pcm5122_write_byte_(PCM51XX_REG_AGAIN, current))
      {
        ESP_LOGE(TAG, "%s write %s", ERROR, "Analog Gain");
        return false;
      }
      if (!this->set_page_(PCM51XX_PAGE_ZERO))
      {
        ESP_LOGE(TAG, "%s finish get %s", ERROR, "Analog Gain");
        return false;
      }

      this->pcm5122_state_.analog_gain = gain_db;
      ESP_LOGD(TAG, "Analog Gain: %fdB", gain_db);
      return true;
    }

    bool Pcm5122Component::set_deep_sleep_off_()
    {
      if (this->pcm5122_state_.control_state != CTRL_PWDN)
        return true; // already not in POWER DOWN

      if (!this->pcm5122_write_byte_(PCM51XX_REG_STATE, CTRL_PLAY))
        return false;

      this->pcm5122_state_.control_state = CTRL_PLAY; // set Control State to PLAY
      ESP_LOGD(TAG, "POWER DOWN OFF");
      if (this->is_muted_)
        ESP_LOGD(TAG, "Mute ON");
      return true;
    }

    bool Pcm5122Component::set_deep_sleep_on_()
    {
      if (this->pcm5122_state_.control_state == CTRL_PWDN)
        return true; // already in POWER DOWN

      if (!this->pcm5122_write_byte_(PCM51XX_REG_STATE, CTRL_PWDN))
        return false;

      this->pcm5122_state_.control_state = CTRL_PWDN; // set Control State to POWER DOWN
      ESP_LOGD(TAG, "POWER DOWN ON");
      if (this->is_muted_)
        ESP_LOGD(TAG, "Mute ON");
      return true;
    }

    bool Pcm5122Component::get_digital_volume_(uint8_t *raw_volume)
    {
      uint8_t current = 254; // lowest raw volume
      if (!this->pcm5122_read_byte_(PCM51XX_REG_VOL_L, &current))
        return false;
      *raw_volume = current;
      return true;
    }

    // controls both left and right channel digital volume
    // digital volume is 24 dB to -103 dB in -0.5 dB step
    // 00000000: +24.0 dB
    // 00000001: +23.5 dB
    // 00101111: +0.5 dB
    // 00110000: 0.0 dB
    // 00110001: -0.5 dB
    // 11111110: -103 dB
    // 11111111: Mute
    bool Pcm5122Component::set_digital_volume_(uint8_t raw_volume)
    {
      if (!this->pcm5122_write_byte_(PCM51XX_REG_VOL_L, raw_volume))
        return false;
      if (!this->pcm5122_write_byte_(PCM51XX_REG_VOL_R, raw_volume))
        return false;
      return true;
    }

    bool Pcm5122Component::get_mixer_mode_(MixerMode *mode)
    {
      *mode = this->pcm5122_state_.mixer_mode;
      return true;
    }

    bool Pcm5122Component::set_mixer_mode_(MixerMode mode)
    {
      uint8_t mixer_value = 0;
      switch (mode)
      {
      case STEREO:
        mixer_value = PCM51XX_REG_MIXER_VAL_STEREO;
        break;

      case STEREO_INVERSE:
        mixer_value = PCM51XX_REG_MIXER_VAL_STEREO_INV;
        break;

      case LEFT:
        mixer_value = PCM51XX_REG_MIXER_VAL_LEFT;
        break;

      case RIGHT:
        mixer_value = PCM51XX_REG_MIXER_VAL_RIGHT;
        break;

      default:
        ESP_LOGD(TAG, "Invalid %s", MIXER_MODE);
        return false;
      }

      if (!this->pcm5122_write_byte_(PCM51XX_REG_MIXER, mixer_value))
      {
        ESP_LOGE(TAG, "%s Mixer L-L Gain", ERROR);
        return false;
      }

      this->pcm5122_state_.mixer_mode = mode;
      ESP_LOGD(TAG, "%s: %s", MIXER_MODE, MIXER_MODE_TEXT[this->pcm5122_state_.mixer_mode]);
      return true;
    }

    bool Pcm5122Component::get_state_(ControlState *state)
    {
      *state = this->pcm5122_state_.control_state;
      return true;
    }

    bool Pcm5122Component::set_state_(ControlState state)
    {
      if (this->pcm5122_state_.control_state == state)
        return true;
      if (!this->pcm5122_write_byte_(PCM51XX_REG_STATE, state))
        return false;
      this->pcm5122_state_.control_state = state;
      return true;
    }

    // low level functions
    bool Pcm5122Component::set_page_(uint8_t page)
    {
      if (!this->pcm5122_write_byte_(PCM5122_REG_PAGE_SET, page))
      {
        ESP_LOGE(TAG, "%s page %d", ERROR, page);
        return false;
      }
      return true;
    }

    bool Pcm5122Component::pcm5122_read_byte_(uint8_t a_register, uint8_t *data)
    {
      return pcm5122_read_bytes_(a_register, data, 1);
    }

    bool Pcm5122Component::pcm5122_read_bytes_(uint8_t a_register, uint8_t *data, uint8_t number_bytes)
    {
      i2c::ErrorCode error_code;
      error_code = this->write(&a_register, 1);
      if (error_code != i2c::ERROR_OK)
      {
        ESP_LOGE(TAG, "Write error:: %i", error_code);
        this->i2c_error_ = (uint8_t)error_code;
        return false;
      }
      error_code = this->read_register(a_register, data, number_bytes);
      if (error_code != i2c::ERROR_OK)
      {
        ESP_LOGE(TAG, "Read error: %i", error_code);
        this->i2c_error_ = (uint8_t)error_code;
        return false;
      }
      return true;
    }

    bool Pcm5122Component::pcm5122_write_byte_(uint8_t a_register, uint8_t data)
    {
      return this->pcm5122_write_bytes_(a_register, &data, 1);
    }

    bool Pcm5122Component::pcm5122_write_bytes_(uint8_t a_register, uint8_t *data, uint8_t len)
    {
      i2c::ErrorCode error_code = this->write_register(a_register, data, len);
      if (error_code != i2c::ERROR_OK)
      {
        ESP_LOGE(TAG, "Write error: %i", error_code);
        this->i2c_error_ = (uint8_t)error_code;
        return false;
      }
      return true;
    }

  } // namespace pcm5122
} // namespace esphome
