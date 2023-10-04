# ESP32-EvilTwin-Deauther
An ESP32 based evil twin AP and deauther, with support for Heltec ESP32(V3), based off of y0xhz's code found here: https://github.com/y0xhz/ESP32-EvilTwin



# FAQ/COMMON ISSUES:
The setup/configuration AP will be renamed to something like "ESP_...." or the last AP you cloned if the udefPW (user defined password) is less than 8 characters.
 - To avoid this, either use no password (#define udefPW "") or use a password longer than 8 characters.
