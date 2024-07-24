# ESP8266 OLED TOTP Generator

This project implements a Time-based One-Time Password (TOTP) generator using an ESP8266 microcontroller with an integrated OLED display. It connects to Wi-Fi, synchronizes time using NTP, and generates TOTP codes based on a secret key.

## Features

- Connects to Wi-Fi using multiple stored credentials
- Synchronizes time using NTP
- Generates TOTP codes
- Displays information on the integrated OLED screen
- Automatically updates TOTP code when it changes

## Hardware

This project uses a specific ESP8266 development board with an integrated OLED display:

- [ESP8266 NodeMCU with 0.96 inch OLED Display](https://www.aliexpress.com/item/1005006579253452.html)

This board was chosen for its convenience and cost-effectiveness, as it eliminates the need for separate wiring between the ESP8266 and the OLED display.

## Software Requirements

- PlatformIO
- Arduino framework for ESP8266

## Libraries Used

- U8g2lib: For OLED display control
- ezTime: For NTP time synchronization and management
- TOTP library: For generating TOTP codes

## Setup

1. Clone this repository
2. Open the project in PlatformIO
3. Modify the `src/config.h` file to include your Wi-Fi credentials and TOTP secret key
4. Build and upload the project to your ESP8266 board

## Configuration

Edit the `src/config.h` file to set up your Wi-Fi networks and TOTP secret:

```cpp
const WiFiCredentials WIFI_CREDS[] = {
    {"SSID1", "PASSWORD1"},
    {"SSID2", "PASSWORD2"},
    // Add more networks as needed
};

const char *TOTP_SECRET = "YOUR_BASE32_ENCODED_SECRET";
```

## Pin Configuration

Since this project uses a board with an integrated OLED display, no additional wiring is required. The OLED display is pre-connected to the ESP8266.

## Contributing

Contributions to this project are welcome. Please feel free to submit a Pull Request.

## License

This project is open source and available under the [MIT License](LICENSE).

## Acknowledgments

- Thanks to the authors of the U8g2lib, ezTime, and TOTP-library for their excellent work.
- Special thanks to the manufacturers of the ESP8266 NodeMCU with integrated OLED display, which simplifies the hardware setup for this project.
