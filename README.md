# ESP8266 OLED TOTP Generator: My First ESP Adventure! 🚀

Hey there! Welcome to my very first ESP8266 project. This little gadget is my solution to a real-world problem: I got tired of constantly pulling out my phone, unlocking it, and squinting at my OKTA 2FA code every time I needed to log in. So, I decided to make my life easier (and hopefully yours too)!

## Demo

Here's a quick demo of the board in action:

![Demo GIF](assets/demo.gif)

This shows the board turning on, connecting to WiFi, and generating a TOTP code.

## What's This All About?

This project turns an ESP8266 microcontroller with a built-in OLED display into a Time-based One-Time Password (TOTP) generator. It's like having a dedicated 2FA device, but cooler because you made it yourself!

## Cool Features 😎

- Connects to Wi-Fi (and remembers multiple networks, just in case)
- Syncs time using NTP (because accuracy is key in the world of TOTP)
- Generates TOTP codes (the whole point of this project)
- Shows everything on a tiny, yet mighty OLED screen
- Updates the TOTP code automagically when it changes

## The Hardware

I'm using this nifty ESP8266 board that comes with an OLED display already attached:

- [ESP8266 NodeMCU with 0.96 inch OLED Display](https://www.aliexpress.com/item/1005006579253452.html)

I chose this because, well, less soldering = less chance of me messing things up on my first project!

## Software Stuff You'll Need

- PlatformIO (because it makes life easier)
- Arduino framework for ESP8266 (PlatformIO will handle this for you)

## Libraries That Do The Heavy Lifting

- U8g2lib: For making the OLED display do its thing
- ezTime: For keeping our ESP8266 in sync with the world
- TOTP library: The star of the show, generating our TOTP codes

## Getting This Up and Running

1. Clone this repo (you're already halfway there!)
2. Fire up PlatformIO and open this project
3. Find the `src/config.h` file and put in your Wi-Fi details and TOTP secret key
4. Hit that upload button and watch the magic happen!

## Customizing Your Experience

Pop open `src/config.h` and you'll see something like this:

```cpp
const WiFiCredentials WIFI_CREDS[] = {
    {"MyHomeWiFi", "SuperSecretPassword"},
    {"WorkWiFi", "EvenMoreSecretPassword"},
    // Add as many as you want!
};

const char *TOTP_SECRET = "YOUR_BASE32_ENCODED_SECRET";
```

Just fill in your details and you're good to go!

## A Note on Pins

The beauty of this particular board is that everything's pre-wired. No need to worry about which pin goes where – it's all taken care of!

## Want to Help?

This is my first rodeo with ESP8266 and hardware stuff, so if you spot any rookie mistakes or have ideas to make this even cooler, I'm all ears! Feel free to open an issue or submit a pull request.

## Legal Stuff

This project is open source and available under the [MIT License](LICENSE). Use it, tweak it, share it – just have fun!

## Shoutouts

- Big thanks to the brilliant minds behind U8g2lib, ezTime, and TOTP-library. You folks rock!
- Hats off to the makers of the ESP8266 NodeMCU with integrated OLED display. You saved me from a tangled mess of wires!

Happy TOTPing, folks! 🎉