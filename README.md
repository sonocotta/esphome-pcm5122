# ESPHome Component for PCM5122 DAC

The PCM5122 is a high-performance audio DAC (Digital-to-Analog Converter) designed for portable and automotive audio applications. This ESPHome external component provides easy integration of the PCM5122 DAC with ESP32-based systems for professional audio output.

- [ESPHome Component for PCM5122 DAC](#esphome-component-for-pcm5122-dac)
- [Why](#why)
  - [Usage: PCM5122 Component from GitHub](#usage-pcm5122-component-from-github)
  - [Overview](#overview)
    - [Key Specifications](#key-specifications)
  - [Component Features](#component-features)
  - [Hardware Requirements](#hardware-requirements)
    - [I2C Interface](#i2c-interface)
    - [Optional GPIO Enable Pin](#optional-gpio-enable-pin)
  - [YAML Configuration](#yaml-configuration)
    - [Basic Audio DAC Configuration](#basic-audio-dac-configuration)
    - [Complete Configuration Example](#complete-configuration-example)
    - [Configuration Variables](#configuration-variables)
      - [audio\_dac](#audio_dac)
  - [Switches](#switches)
  - [DSP Process Flow Select](#dsp-process-flow-select)
    - [Available Process Flows](#available-process-flows)
  - [6-Band Parametric EQ (Biquad Filter Controls)](#6-band-parametric-eq-biquad-filter-controls)
    - [Example EQ Configuration](#example-eq-configuration)
    - [EQ Band Configuration Variables](#eq-band-configuration-variables)
    - [Known issues](#known-issues)
  - [Volume Control](#volume-control)
    - [Setting Volume in YAML](#setting-volume-in-yaml)
    - [Volume Control via Automations](#volume-control-via-automations)
  - [Mute Control](#mute-control)
    - [I2C Register-based Mute](#i2c-register-based-mute)
    - [Optional GPIO Mute Pin](#optional-gpio-mute-pin)
    - [Mute Control in YAML](#mute-control-in-yaml)
  - [Integration with I2S Audio](#integration-with-i2s-audio)
  - [Power Modes](#power-modes)
    - [Play Mode (Enabled)](#play-mode-enabled)
    - [Sleep Mode (Deep Sleep)](#sleep-mode-deep-sleep)
  - [Complete Example YAML](#complete-example-yaml)
  - [Troubleshooting](#troubleshooting)
    - [DAC Not Initializing](#dac-not-initializing)
    - [No Audio Output](#no-audio-output)
    - [EQ Controls Not Working](#eq-controls-not-working)
    - [I2C Communication Errors](#i2c-communication-errors)
  - [Component Files](#component-files)
  - [License](#license)
  - [References](#references)


# Why

I've created a [HiFi-ESP32-Plus](https://sonocotta.com/hifi-esp32-and-hifi-esp32s3/) and [Amped-ESP32-Plus](https://sonocotta.com/amped-esp32/) boards, that uses this DAC, and I wanted to integrate this DAC into the Home Assistant. Even though it is a fairly old DAC, and TI doesn't update they tooling to support it properly, it is still great chip for the price 

## Usage: PCM5122 Component from GitHub

This component requires ESPHome version 2025.7.0 or later.

The following YAML can be used so ESPHome accesses the component files:

```yaml
external_components:
  - source: github://sonocotta/esphome-pcm5122@main
```

## Overview

The PCM5122 is a versatile audio DAC that communicates with the ESP32 via I2C interface (control) and I2S (audio). This component allows basic control of the DAC's features including digital volume adjustment, mute control, analog gain, basic mixer controls and power management through Home Assistant or direct ESPHome automation.

### Key Specifications

- **Interface**: I2C (Two-wire interface) and I2S (Digital Audio)
- **Digital Volume Range**: -103 dB to +24 dB in 0.5 dB steps
- **Volume Control**: 0-100% via ESPHome (internally remapped to dB range)
- **Analog Gain**: Configurable (-6 dB or 0 dB) for 1V or 2V RMS output levels
- **Mute Control**: Register-based (always available) and optional GPIO-based mute
- **Default I2C Address**: 0x4D (7-bit)
- **Power Modes**: Play mode (active audio) and Sleep mode (low power)
- **DSP Process Flow Selection**: Multiple interpolation filter options including parametric EQ
- **6-Band Parametric EQ**: Configurable biquad filters with adjustable gain, frequency, Q-factor, and filter type

## Component Features

The PCM5122 ESPHome component provides the following capabilities:

- **Initialize PCM5122 DAC** - Configure and initialize the audio codec on startup
- **Enable/Disable DAC** - Place the DAC into Play mode or low-power Sleep mode via optional switch
- **Volume Control** - Set and get volume level from 0-100% (remapped to -103 dB to +24 dB register range)
- **Analog Gain Control** - Set desired output voltage level
- **Mute Control** - Mute and unmute the DAC via I2C register and optional GPIO pin
- **Power Management** - Enable Deep Sleep mode for low-power operation
- **Volume Range Configuration** - Define maximum and minimum volume limits in dB
- **Basic Mixer control** - Stereo (default), Stereo (inverted channels), Left or Right channel only 
- **DSP Process Flow Select** - Choose between 5 built-in DSP processing paths (including Parametric EQ) via Home Assistant select entity
- **6-Band Parametric EQ** - Full biquad filter control with per-band gain adjustment, configurable frequency, Q-factor, and filter type

## Hardware Requirements

### I2C Interface

The PCM5122 requires I2C communication:
- **SDA Pin**: Data line (configurable)
- **SCL Pin**: Clock line (configurable)

### Optional GPIO Enable Pin

For hardware-based mute control via GPIO:
- **Mute Pin**: GPIO pin for direct mute control (optional, when XSMT pin is wired to it)
  - `HIGH` = Unmute (audio enabled)
  - `LOW` = Mute (audio disabled)
  - Can also be controlled programmatically via DAC Enable Switch

## YAML Configuration

### Basic Audio DAC Configuration

```yaml
audio_dac:
  - platform: pcm5122
```

### Complete Configuration Example

```yaml
i2c:
  sda: GPIO21
  scl: GPIO27
  id: i2c_bus

audio_dac:
  - platform: pcm5122
    i2c_id: i2c_bus
    enable_pin: GPIO13
    volume_max: 0
    volume_min: -103
    analog_gain: 0dB
    mixer_mode: STEREO
```

### Configuration Variables

#### audio_dac

- **platform** (*Required*): `pcm5122`
- **i2c_id** (*Optional*): ID of the I2C bus. Leave unset if only one I2C bus is defined.
- **address** (*Optional*): I2C address of the DAC. Defaults to `0x4D` (7-bit address).
- **enable_pin** (*Optional*): GPIO pin used to control DAC mute pin (if connected to XSMT pin).
- **analog_gain** (*Optional*): Analog gain in dB. Valid values: `0dB` (default) or `-6dB`.
  - `0dB`: Use for 2V RMS output signal
  - `-6dB`: Use for 1V RMS output signal
- **volume_max** (*Optional*): Maximum volume level in dB. Valid range: -103 to 24. Default: `0` dB, to guarantee no clipping
- **volume_min** (*Optional*): Minimum volume level in dB. Valid range: -103 to 24. Default: `-103` dB, but maybe raised to limit dynamic range. Reasonable value is -60dB.
- **mixer_mode** (*Optional*): Channel mixer mode. Valid values: `STEREO` (default), `STEREO_INVERSE`, `LEFT`, `RIGHT`.

## Switches

An optional switch can be configured to control the DAC enable/disable state in Home Assistant.

```yaml
switch:
  - platform: pcm5122
    enable_dac:
      name: "Enable DAC"
      id: enable_dac
      restore_mode: ALWAYS_OFF
```

## DSP Process Flow Select

The PCM5122 supports multiple DSP processing paths via its process flow register. This can be controlled through a Home Assistant select entity, allowing runtime switching between different filter and interpolation modes.

```yaml
select:
  - platform: pcm5122
    process_flow:
      name: "PCM5122 Process Flow"
      id: pcm5122_process_flow
```

### Available Process Flows

| Flow | Name | Description |
|------|------|-------------|
| Flow 1 | FIR Interpolation | FIR interpolation filter with de-emphasis |
| Flow 2 | Low-latency IIR | Low latency IIR interpolation filter |
| Flow 3 | High-attenuation FIR | High attenuation FIR interpolation filter |
| Flow 5 | Parametric EQ | Fixed process flow with configurable biquad parameters (enables EQ) |
| Flow 7 | Ringing-less FIR | Ringing-less low latency FIR interpolation filter |

**Note:** Flow 5 (Parametric EQ) enables the 6-band biquad filter engine. This flow is required for the EQ band controls to have an audible effect. The selection is persisted across reboots.

## 6-Band Parametric EQ (Biquad Filter Controls)

When using Process Flow 5 (Parametric EQ), the PCM5122's internal DSP provides 6 configurable biquad filter bands. Each band supports the following parameters:

- **Gain** (adjustable in Home Assistant, -24 dB to +24 dB, 0.5 dB steps)
- **Filter type** (configured in YAML)
- **Center frequency** (configured in YAML, 1-20000 Hz)
- **Q-factor** (configured in YAML)

### Example EQ Configuration

```yaml
number:
  - platform: pcm5122
    band_1:
      filter_type: "Low Shelf"        # Filter type: "Low Shelf", "High Shelf", "Equalizer", "Low Pass", "High Pass"
      frequency: 50                    # Center frequency (Hz), range 1-20000
      q_factor: 1.5                    # Q-factor, range 0.1-10.0
      min_gain: -12dB                  # Minimum slider value
      max_gain: 12dB                   # Maximum slider value
      gain:
        name: "63 Hz"

    band_2:
      filter_type: "Equalizer"
      frequency: 250
      q_factor: 1.0
      min_gain: -12dB
      max_gain: 12dB
      gain:
        name: "250 Hz"

    band_3:
      filter_type: "Equalizer"
      frequency: 1000
      q_factor: 0.9
      min_gain: -12dB
      max_gain: 12dB
      gain:
        name: "1000 Hz"

    band_4:
      filter_type: "Equalizer"
      frequency: 4000
      q_factor: 0.8
      min_gain: -12dB
      max_gain: 12dB
      gain:
        name: "4000 Hz"

    band_5:
      filter_type: "Equalizer"
      frequency: 8000
      q_factor: 0.7
      min_gain: -12dB
      max_gain: 12dB
      gain:
        name: "8000 Hz"

    band_6:
      filter_type: "High Shelf"
      frequency: 16000
      q_factor: 0.6
      min_gain: -12dB
      max_gain: 12dB
      gain:
        name: "16000 Hz"
```

### EQ Band Configuration Variables

Each `band_N` block supports:

- **filter_type** (*Optional*, default: `"Equalizer"`): Biquad filter type. One of:
  - `"Equalizer"` - Peaking EQ (bell curve)
  - `"Low Shelf"` - Low shelving filter
  - `"High Shelf"` - High shelving filter
  - `"Low Pass"` - Low pass filter
  - `"High Pass"` - High pass filter
- **frequency** (*Optional*, default: `1000`): Center/corner frequency in Hz (1-20000).
- **q_factor** (*Optional*, default: `1.0`): Quality factor (0.1-10.0).
- **min_gain** (*Optional*, default: `-12dB`): Minimum gain value for the Home Assistant slider (-24 to 24 dB).
- **max_gain** (*Optional*, default: `12dB`): Maximum gain value for the Home Assistant slider (-24 to 24 dB).
- **gain** (*Required*): ESPHome number entity schema for the adjustable gain control. Must have a `name:` property.

**Note:** The filter type, frequency, and Q-factor are static configuration values set in YAML and cannot be changed at runtime. Only the gain per band is adjustable via Home Assistant. The gain is stored in the number entity's preferences and restored on reboot.

### Known issues

Since I don't have PCM5122 EVM (and could not fine a second-hand one), and PPC app would not allow exporting BQ parameters without device connected, I had to do a fir bit of reverse engineering to determine the data format and calculation formulas for each filter type. The only helpful part of the datasheet is the information on the correct register addresses for the biquad filter configuration, but even there lots of koefficients exist that lacks documentation and they are required to be set for audio processing. 

While I have tested that changes to the gain parameter do have an effect on the audio output, I have not been able to fully verify that the frequency and Q-factor settings are applied correctly, due to lack of test equipment. If you notice any issues with the EQ controls or if the frequency/Q settings don't seem to have any effect, please open an issue with details about your hardware setup and observations. At this point I'm pretty sure it doesn't work correctly at high frequencies and at high gain values, but I'm not able to determine if that is expected behavior of the filters or if there is a bug in the implementation.

## Volume Control

Volume is controlled as a percentage (0-100%) which is internally remapped to the DAC's digital volume register range (-103 dB to 0 dB, unless remaped in yaml to custom values).

### Setting Volume in YAML

```yaml
media_player:
  - platform: i2s_audio
    # ... other configuration
    volume: 0.8  # 80% volume
```

### Volume Control via Automations

```yaml
automation:
  - trigger:
      platform: state
      entity_id: input_number.audio_volume
    action:
      service: media_player.volume_set
      data:
        entity_id: media_player.my_speaker
        volume_level: "{{ trigger.to_state.state | float / 100 }}"
```

## Mute Control

The PCM5122 supports two types of mute control:

### I2C Register-based Mute

Mute is controlled via the PCM5122 register (Register 0x03):
- Setting bits enable/disable mute on both channels
- Works regardless of GPIO configuration

### Optional GPIO Mute Pin

If a GPIO mute pin connected to the XSMT pin:
- Provides hardware-level mute control
- Works in parallel with register-based mute
- `HIGH` = Unmute (enabled)
- `LOW` = Mute (disabled)

### Mute Control in YAML

```yaml
automation:
  - trigger:
      platform: homeassistant
      event: call_service
      service: media_player.mute_volume
    action:
      - service: media_player.mute_volume
        data:
          entity_id: media_player.my_speaker
          is_volume_muted: true
```

## Integration with I2S Audio

The PCM5122 DAC is typically used with the ESPHome I2S Audio component:

```yaml
i2s_audio:
  - i2s_lrclk_pin: GPIO25
    i2s_bclk_pin: GPIO26
    i2s_dout_pin: GPIO27

media_player:
  - platform: i2s_audio
    name: "My Audio Player"
    dac: pcm5122_dac
    speaker: my_speaker

speaker:
  - platform: i2s_audio
    name: "Speaker"
    dac_type: external
    i2s_audio_id: i2s_audio
```

## Power Modes

### Play Mode (Enabled)

When the DAC is enabled:
- Normal audio operation
- Audio can be produced at configured volume
- Current consumption: ~100-150 mA typical

### Sleep Mode (Deep Sleep)

When the DAC is disabled:
- Reduced power consumption
- Audio output disabled
- Current consumption: < 50 µA typical

## Complete Example YAML

Can be found in the example for [ESP32](/components/pcm5122/yaml/hifi-esp32-plus-media-player.yaml) and [ESP32-S3](/components/pcm5122/yaml/hifi-esp32s3-plus-media-player.yaml), including full EQ configuration in [pcm5122-dac.yaml](/components/pcm5122/yaml/pcm5122-dac.yaml).

## Troubleshooting

### DAC Not Initializing

1. **Check I2C Connection**
   ```bash
   esphome logs components/pcm5122/yaml/hifi-esp32s3-plus-media-player.yaml
   ```
   Look for I2C initialization messages and any I2C errors.

2. **Verify I2C Address**
   - Default address is `0x4D` (7-bit)
   - Check hardware configuration if using a different address

3. **Check GPIO Pins**
   - Enable pin is now optional - DAC will work without it
   - Verify pull-up resistors on SDA/SCL lines (typically 4.7k ohms)
   - Ensure I2C pins are not used for other purposes

### No Audio Output

1. **Check Mute Status**
   - Ensure DAC is not muted via register or GPIO
   - Verify speaker is properly connected

2. **Verify I2S Configuration**
   - Ensure I2S audio pins are correctly configured
   - Check I2S clock and data line connections

3. **Volume Level**
   - Confirm volume is not set to 0%
   - Check that volume_min/volume_max are within valid range

### EQ Controls Not Working

1. **Select Process Flow 5**
   - The 6-band parametric EQ only functions when Process Flow 5 (Parametric EQ) is selected via the process flow select entity
   - Other process flows bypass the biquad filter coefficients entirely

2. **Verify EQ Band Gain**
   - Each EQ band has a `min_gain` and `max_gain` setting - ensure the range is appropriate for your use case
   - If gain values don't seem to have any effect, verify the DAC is not muted

### I2C Communication Errors

1. **Enable Debug Logging**
   ```yaml
   logger:
     level: DEBUG
   ```

2. **Check Voltage Levels**
   - I2C bus should be pulled to 3.3V (for ESP32)
   - Add 4.7k ohm pull-up resistors if missing (pins have no PULLUP support)

## Component Files

The PCM5122 component consists of:

- `pcm5122.h` - Component header file with class definitions
- `pcm5122.cpp` - Component implementation
- `pcm5122_cfg.h` - Configuration and register definitions
- `audio_dac.py` - ESPHome integration for YAML configuration
- `switch/__init__.py` - Optional switches for DAC enable/disable control
- `switch/enable_dac_switch.cpp` / `.h` - DAC enable switch implementation
- `select/__init__.py` - Optional process flow select entity
- `select/process_flow_select.cpp` / `.h` - Process flow selection implementation
- `number/__init__.py` - Optional 6-band parametric EQ number entities
- `number/eq_band_gain.cpp` / `.h` - EQ band gain control implementation

## License

This component is licensed under GPLv3

## References

- [PCM5122 Datasheet](https://www.ti.com/product/PCM5122)
- [ESPHome Audio DAC Component](https://esphome.io/components/audio_dac/index.html)
- [ESPHome I2S Audio](https://esphome.io/components/i2s_audio/index.html)