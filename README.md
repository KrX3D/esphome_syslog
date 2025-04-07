# ESPHome Syslog Component

A powerful and configurable Syslog component for ESPHome that forwards logs to a Syslog server. The component can automatically attach itself to the ESPHome logger core module (similar to how the MQTT component works with `log_topic`) while providing advanced filtering and configuration options.

## Features

- Forward ESPHome logs to a Syslog server
- Direct logging through automations
- Advanced filtering (include/exclude specific components)
- Configurable log levels
- Color code stripping
- Runtime reconfigurable (server IP, port, filters)
- Support for both IPv4 and IPv6 addresses
- Component-specific log prefixing
- Integration with Text components for filter management

## Installation

### Using External Components (Recommended)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/TheStaticTurtle/esphome_syslog
      ref: main  # or a specific commit SHA
    refresh: always
```

### Manual Installation

To install manually, locate your `esphome` folder, create a folder named `custom_components` (if it doesn't exist), navigate into it, and execute:

```shell
git clone https://github.com/TheStaticTurtle/esphome_syslog.git
mv esphome_syslog/components/syslog .
rm -rf esphome_syslog
```

## Basic Configuration

The simplest configuration only requires adding the component to your ESPHome configuration:

```yaml
syslog:
```

With this minimal configuration, the component will **broadcast logs to everyone on the network** using the default UDP port 514.

## Configuration Options

| Option                | Type      | Default           | Description                                                       |
|-----------------------|-----------|-------------------|-------------------------------------------------------------------|
| `ip_address`          | string    | "255.255.255.255" | IP address of the Syslog server (IPv4 or IPv6)                   |
| `port`                | integer   | 514               | UDP port of the Syslog server                                     |
| `client_id`           | string    | Device name       | Client identifier in Syslog messages                             |
| `strip_colors`        | boolean   | true              | Remove ESPHome color codes from log messages                      |
| `enable_logger`       | boolean   | true              | Enable forwarding of ESPHome logger messages                      |
| `enable_direct_logs`  | boolean   | true              | Enable direct logging through automations                         |
| `globally_enabled`    | boolean   | true              | Global switch to enable/disable the component                     |
| `min_level`           | string    | "DEBUG"           | Minimum log level to forward (ERROR, WARN, INFO, DEBUG, etc.)     |
| `filter_mode`         | string/bool | "exclude"       | Filter mode: "include" or "exclude" (or true/false)               |
| `filters`             | list      | []                | List of component tags to include/exclude                         |
| `filter_string`       | string    | ""                | Comma-separated list of component tags to include/exclude         |
| `filter_text`         | id        | -                 | Text component ID to display/update filter string                 |
| `direct_log_prefix`   | string    | ""                | Prefix added to direct log messages                               |
| `logger_log_prefix`   | string    | ""                | Prefix added to logger messages                                   |

## Advanced Configuration Examples

### Specific Server with Tag Filtering

```yaml
syslog:
  ip_address: "192.168.1.53"
  port: 514
  min_level: INFO
  filter_mode: exclude
  filters:
    - wifi
    - mqtt
    - api
  direct_log_prefix: "direct:"
  logger_log_prefix: "logger:"
```

This configuration:
- Sends logs to a specific server at 192.168.1.53:514
- Only forwards logs of level INFO or higher
- Excludes logs from the wifi, mqtt, and api components
- Prefixes direct logs with "direct:" and logger logs with "logger:"

### Using a Text Component for Filter Management

```yaml
text:
  - platform: template
    id: syslog_filter
    name: "Syslog Filter"
    
syslog:
  filter_mode: include
  filter_text: syslog_filter
```

This configuration allows updating filters at runtime via the text component in Home Assistant or other frontends.

## Automation Actions

The component provides several automation actions:

### Sending Custom Logs

```yaml
button:
  - platform: template
    name: "Test Syslog"
    on_press:
      - syslog.log:
          level: 6  # INFO level
          tag: "custom_action"
          payload: "Button pressed!"
```

### Managing Filters

```yaml
button:
  - platform: template
    name: "Add Filter"
    on_press:
      - syslog.add_filter:
          tag: "wifi"
          
  - platform: template
    name: "Remove Filter"
    on_press:
      - syslog.remove_filter:
          tag: "wifi"
          
  - platform: template
    name: "Clear Filters"
    on_press:
      - syslog.clear_filters:
      
  - platform: template
    name: "Set Filter String"
    on_press:
      - syslog.set_filter_string:
          filter_string: "wifi,mqtt,api"
```

## Log Levels Mapping

Due to differences between ESPHome and Syslog log levels, the component maps them as follows:

| ESPHome Level              | Syslog Priority |
|----------------------------|-----------------|
| NONE                       | EMERG (0)       |
| ERROR                      | ERR (3)         |
| WARN                       | WARNING (4)     |
| INFO                       | INFO (6)        |
| CONFIG                     | NOTICE (5)      |
| DEBUG                      | DEBUG (7)       |
| VERBOSE                    | DEBUG (7)       |
| VERY_VERBOSE               | DEBUG (7)       |

## Example Log Output

When properly configured, you'll see log messages in your Syslog server like these:

```
Apr  7 12:34:56 esp32_living_room wifi: WiFi connected to 'MyNetwork'
Apr  7 12:34:57 esp32_living_room direct:button: Button pressed!
Apr  7 12:34:58 esp32_living_room logger:sensor: Temperature: 22.5°C
Apr  7 12:35:00 esp32_living_room syslog: Filter string updated: 'wifi,mqtt'
```

## Complete Example YAML

Here's a complete example configuration for an ESP32 device with temperature monitoring and Syslog integration:

```yaml
esphome:
  name: esp32_syslog_demo
  friendly_name: ESP32 Syslog Demo

esp32:
  board: esp32dev

# Enable logging
logger:
  level: DEBUG

# Basic network setup
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  
  # Optional fallback hotspot
  ap:
    ssid: "Syslog Demo Fallback"
    password: "12345678"

# API for ESPHome dashboard
api:

# OTA updates
ota:

# Web server for status page
web_server:
  port: 80

# Temperature sensor (example component)
sensor:
  - platform: dht
    pin: GPIO23
    temperature:
      name: "Room Temperature"
    humidity:
      name: "Room Humidity"
    update_interval: 60s

# Syslog configuration
external_components:
  - source:
      type: git
      url: https://github.com/TheStaticTurtle/esphome_syslog
      ref: main  # or a specific commit SHA
    refresh: always

# Text sensor for filter management
text:
  - platform: template
    id: syslog_filter
    name: "Syslog Filter"
    
syslog:
  ip_address: "192.168.1.100"  # Your Syslog server IP
  port: 514
  client_id: esp32_living_room
  min_level: INFO
  filter_mode: exclude
  filters:
    - wifi  # Exclude wifi logs
  filter_text: syslog_filter
  direct_log_prefix: "direct:"
  
# Button to test direct logging
button:
  - platform: template
    name: "Test Syslog Message"
    on_press:
      - syslog.log:
          level: 6  # INFO level
          tag: "button"
          payload: "Test button was pressed!"
          
# Timer to log temperature periodically
interval:
  - interval: 5min
    then:
      - lambda: |-
          id(syslog_filter).publish_state(id(syslog_filter).state + ",temp");
          
          // Get current temperature and log it
          float temp = id(room_temperature).state;
          char message[64];
          sprintf(message, "Current temperature is %.1f°C", temp);
          
      - syslog.log:
          level: 6  # INFO level
          tag: "temp_monitor"
          payload: !lambda 'return message;'
```

## Compatibility

This component has been tested with:
- ESP8266
- ESP32

Note: The ESP32 may experience issues when sending many log messages in rapid succession, such as during boot when configuration is printed. See [issue #7 comment](https://github.com/TheStaticTurtle/esphome_syslog/issues/7#issuecomment-1236194816) for more details.

## Troubleshooting

If you're not seeing logs on your Syslog server:

1. Check that your server IP and port are correct
2. Ensure your network allows UDP traffic on the configured port
3. Verify that log levels and filters aren't blocking messages
4. Check if the component is globally enabled
5. For ESP32, try reducing the logging verbosity to prevent buffer overflow

## Contributing

Contributions are welcome! If you find a bug or want to add a feature, please open an issue or submit a pull request.

## License

This project is licensed under the ISC License - see the LICENSE file for details.
