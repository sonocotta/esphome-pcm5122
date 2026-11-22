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

    // Set to true once the DSP coefficient RAM has been written successfully.
    // Prevents re-writing the coefficients on subsequent set_process_flow()
    // calls at runtime — the PCM5122 coefficient RAM survives standby cycles.
    static bool s_dsp_coeff_written = false;

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

      // Reset DAC using PCM51XX_REG_RESET register
      // Write 0x11 to reset register, then clear reset flags
      if (!this->pcm5122_write_byte_(PCM51XX_REG_RESET, 0x11)) {
        ESP_LOGE(TAG, "%s reset PCM5122", ERROR);
        this->error_code_ = CONFIGURATION_FAILED;
        this->mark_failed();
        return;
      }
      delay(100); // Allow reset to complete

      if (!this->pcm5122_write_byte_(PCM51XX_REG_RESET, 0x00)) {
        ESP_LOGE(TAG, "%s clear reset flags", ERROR);
        this->error_code_ = CONFIGURATION_FAILED;
        this->mark_failed();
        return;
      }
      
      delay(100); // Allow part to stabilise

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
      {
        // --- Write basic codec initialisation sequence ---
        uint16_t reg_count = sizeof(pcm51xx_init_seq) / sizeof(pcm51xx_init_seq[0]);
        for (uint16_t i = 0; i < reg_count; i++)
        {
            if (!this->pcm5122_write_byte_(pcm51xx_init_seq[i].offset, pcm51xx_init_seq[i].value))
              return false;
        }
        ESP_LOGD(TAG, "%s: wrote %d initialisation registers", __func__, reg_count);
      }

      {
        // --- Write DSP CRAM contents ---
        uint16_t reg_count = sizeof(pcm51xx_dsp_coeff_seq) / sizeof(pcm51xx_dsp_coeff_seq[0]);
        for (uint16_t i = 0; i < reg_count; i++)
        {
            if (!this->pcm5122_write_byte_(pcm51xx_dsp_coeff_seq[i].offset, pcm51xx_dsp_coeff_seq[i].value))
              return false;
        }
        s_dsp_coeff_written = true;

        ESP_LOGD(TAG, "%s: wrote %d DSP registers", __func__, reg_count);
      }

      if (!this->set_deep_sleep_off_())
        return false;

      if (!this->set_mixer_mode_(this->pcm5122_state_.mixer_mode))
        return false;

      if (!this->set_analog_gain_(this->pcm5122_state_.analog_gain))
        return false;

      if (!this->set_state_(CTRL_PLAY))
        return false;

      // Read and cache the current process flow from hardware
      uint8_t flow;
      if (this->pcm5122_read_byte_(PCM51XX_REG_PROC_FLOW, &flow)) {
        this->pcm5122_state_.process_flow = flow;
      } else {
        this->pcm5122_state_.process_flow = PCM51XX_PROC_FLOW_1;
      }

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
                      "  Analog Gain: %idB\n"
                      "  Maximum Volume: %idB\n"
                      "  Minimum Volume: %idB\n"
                      "  Mixer Mode: %s\n",

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
      ESP_LOGD(TAG, "Analog Gain: %ddB", gain_db);
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

    bool Pcm5122Component::set_process_flow(uint8_t flow)
    {
      // Validate flow value (only allow documented flows)
      if (flow != PCM51XX_PROC_FLOW_1 &&
          flow != PCM51XX_PROC_FLOW_2 &&
          flow != PCM51XX_PROC_FLOW_3 &&
          flow != PCM51XX_PROC_FLOW_5 &&
          flow != PCM51XX_PROC_FLOW_7) {
        ESP_LOGE(TAG, "%s invalid process flow: %d", ERROR, flow);
        return false;
      }

      // Enter standby before changing process flow to avoid audio glitches
      if (!this->set_state_(CTRL_STBY)) {
        ESP_LOGE(TAG, "%s enter standby for process flow change", ERROR);
        return false;
      }

      if (!this->pcm5122_write_byte_(PCM51XX_REG_PROC_FLOW, flow)) {
        ESP_LOGE(TAG, "%s set process flow %d", ERROR, flow);
        // Exit standby even on failure
        this->set_state_(CTRL_PLAY);
        return false;
      }

      // Exit standby — DSP picks up the new process flow atomically
      if (!this->set_state_(CTRL_PLAY)) {
        ESP_LOGE(TAG, "%s exit standby after process flow change", ERROR);
        return false;
      }

      this->pcm5122_state_.process_flow = flow;
      ESP_LOGD(TAG, "Process flow set to %d", flow);
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

    // ======================== Biquad Coefficient Writing ========================

    /*
     * PCM5122 EQ BQ band address table.
     * Each band: 5 coefficients × 4 bytes = 20 bytes.
     * band 0..3: page 44 (0x2C), band 4..5: page 45 (0x2D).
     */
    static const struct {
      uint8_t page;
      uint8_t base_offset;
    } BQ_BAND_ADDR[6] = {
      {0x2C, 0x30},  /* band 0 (BQ1), c10..c14 */
      {0x2C, 0x44},  /* band 1 (BQ2), c15..c19 */
      {0x2C, 0x58},  /* band 2 (BQ3), c20..c24 */
      {0x2C, 0x6C},  /* band 3 (BQ4), c25..c29 */
      {0x2D, 0x08},  /* band 4 (BQ5), c30..c34 */
      {0x2D, 0x1C},  /* band 5 (BQ6), c35..c39 */
    };

    /*
     * Compute the exact (page, offset) for coefficient ci of a band.
     * Uses the same formula as pcm51xx_bq_coeff_addr() in pcm51xx_bq_addr.h.
     */
    static void bq_coeff_addr_(uint8_t band, int ci, uint8_t *page_out, uint8_t *offset_out) {
      uint16_t abs = (uint16_t)BQ_BAND_ADDR[band].base_offset + (uint16_t)(ci * 4);
      if (abs >= 0x80u) {
        *page_out   = (uint8_t)(BQ_BAND_ADDR[band].page + 1u);
        *offset_out = (uint8_t)(abs - 0x78u);
      } else {
        *page_out   = BQ_BAND_ADDR[band].page;
        *offset_out = (uint8_t)abs;
      }
    }

    /*
     * Convert float in range (-1, +1) to PCM5122 Q3.23 register format.
     * Returns a 32-bit value where bits[31:8] contain the 24-bit coefficient,
     * bits[7:0] are always 0x00.
     */
    static uint32_t float_to_q323_(float value) {
      if (value > 0.9999999f) value = 0.9999999f;
      if (value < -1.0f) value = -1.0f;
      int32_t q = (int32_t)(value * (float)(1u << 23));
      return (uint32_t)q << 8;
    }

    bool Pcm5122Component::write_bq_coefficients(uint8_t band, const float coeff_f[5]) {
      if (band >= 6) {
        ESP_LOGE(TAG, "Invalid BQ band: %d", band);
        return false;
      }

      // Enter standby
      if (!this->pcm5122_write_byte_(PCM51XX_REG_STATE, CTRL_STBY)) {
        ESP_LOGE(TAG, "Failed to enter standby for BQ write");
        return false;
      }

      bool success = true;
      uint8_t current_page = 0xFF;  // sentinel — force first page select

      for (int ci = 0; ci < 5 && success; ci++) {
        uint8_t pg, off;
        bq_coeff_addr_(band, ci, &pg, &off);

        // Select page if needed
        if (pg != current_page) {
          if (!this->set_page_(pg)) {
            ESP_LOGE(TAG, "Failed to select page 0x%02x for BQ band %d ci %d", pg, band, ci);
            success = false;
            break;
          }
          current_page = pg;
        }

        // Convert coefficient to Q3.23 big-endian register format
        uint32_t value = float_to_q323_(coeff_f[ci]);
        uint8_t be[4] = {
          (uint8_t)(value >> 24),
          (uint8_t)(value >> 16),
          (uint8_t)(value >>  8),
          (uint8_t)(value),
        };

        // Write 4 bytes (MSByte first) to consecutive register addresses
        for (int bi = 0; bi < 4 && success; bi++) {
          if (!this->pcm5122_write_byte_(off + (uint8_t)bi, be[bi])) {
            ESP_LOGE(TAG, "Failed to write BQ band %d ci %d byte %d", band, ci, bi);
            success = false;
          }
        }

        ESP_LOGD(TAG, "BQ band %d ci %d pg=0x%02x off=0x%02x wrote [0x%02x 0x%02x 0x%02x 0x%02x] (%.7f)",
                 band, ci, pg, off, be[0], be[1], be[2], be[3], coeff_f[ci]);
      }

      // Restore page 0
      if (!this->set_page_(PCM51XX_PAGE_ZERO)) {
        ESP_LOGW(TAG, "Failed to restore page 0 after BQ write");
      }

      // Exit standby
      if (!this->pcm5122_write_byte_(PCM51XX_REG_STATE, CTRL_PLAY)) {
        ESP_LOGE(TAG, "Failed to exit standby after BQ write");
        return false;
      }

      if (!success) {
        ESP_LOGE(TAG, "BQ coefficient write failed for band %d", band);
        return false;
      }

      ESP_LOGD(TAG, "BQ band %d coefficients written successfully", band);
      return true;
    }

  } // namespace pcm5122
} // namespace esphome
