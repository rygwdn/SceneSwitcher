# MIDI Device Tracking and Management

## How MIDI Devices Are Tracked

MIDI devices are tracked by **name** (not by unique ID). The device name is constructed from the port information:

```cpp
std::string name = "[" + manufacturer + " " + device_name + " " + port_name + "] " + display_name;
```

The name is stored in a map with the key: `(MidiDeviceType, std::string name)` where:
- `MidiDeviceType` is either `INPUT` or `OUTPUT`
- `name` is the constructed string from port information

**Important**: This is a **name-based** identification system, not ID-based. This means:
- If a device's name changes (e.g., manufacturer/port name changes), it will be treated as a different device
- Devices are matched by exact string comparison
- The same physical device with different port names will be treated as separate devices

## Device Lifecycle Scenarios

### 1. New Device Added After Startup

**What happens:**
- The `libremidi::observer` detects the new device via `input_added` or `output_added` callback
- The callback now:
  1. Constructs the device name from port information
  2. Checks if the device is enabled in settings via `IsMidiEndpointEnabled()`
  3. If enabled, calls `GetDeviceAndOpen()` which:
     - Creates a new `MidiDeviceInstance` if it doesn't exist
     - Adds it to the `devices` map
     - Attempts to open the port (which will succeed if the device is available)
  4. If disabled, logs a message and does nothing

**Result**: ✅ New devices that are enabled in settings are automatically opened when they become available.

### 2. Device Removed After Startup

**What happens:**
- The observer detects removal via `input_removed` or `output_removed` callback
- The callback finds the device in the map and logs the removal
- The device instance remains in the map but the port is closed
- When the device is reconnected, the `*_added` callback will handle reopening it

**Result**: ✅ Devices are properly tracked when removed, and will reopen when reconnected (if enabled).

### 3. Device Not Available on Load, Becomes Available Later

**What happens:**
- During `LoadGeneralSettings()`, all enabled MIDI endpoints are loaded into `midiEndpointSettings`
- After loading, `OpenEnabledMidiEndpoints()` is called, which:
  1. Gets all currently available devices
  2. For each enabled device in settings:
     - Checks if the device is currently available
     - If available, opens it
     - If not available, logs that it's enabled but not available
- When the device becomes available later:
  - The observer's `*_added` callback fires
  - It checks if the device is enabled in settings
  - If enabled, it opens the device

**Result**: ✅ Devices that should be opened but aren't available on load will be automatically opened when they become available.

### 4. Device Name Changes

**What happens:**
- If a device's name changes (e.g., port name changes), it will be treated as a completely new device
- The old device entry in settings will not match the new name
- The new device will use default behavior (allowed if not in settings, or disabled if explicitly set to NONE)

**Result**: ⚠️ Device name changes can cause devices to be treated as new devices. Users may need to update their settings.

## Settings Behavior

### Backward Compatibility

- If no MIDI endpoint settings exist, **all devices are allowed** (backward compatible)
- If a device is not in the settings list, it's **allowed by default**
- Only devices explicitly configured in settings are restricted

### Settings Storage

Settings are stored in `SwitcherData::midiEndpointSettings` as a vector of `MidiEndpointSettings`, each containing:
- `_name`: The device name (string)
- `_mode`: The endpoint mode (NONE, INPUT, OUTPUT, or BOTH)

Settings are saved/loaded with the scene collection, so they persist across OBS restarts.

## Summary

| Scenario | Behavior |
|----------|----------|
| New device added (enabled) | ✅ Automatically opened |
| New device added (disabled) | ✅ Ignored, not opened |
| Device removed | ✅ Tracked, will reopen when reconnected (if enabled) |
| Device unavailable on load, becomes available | ✅ Automatically opened when available |
| Device name changes | ⚠️ Treated as new device |
| No settings configured | ✅ All devices allowed (backward compatible) |
