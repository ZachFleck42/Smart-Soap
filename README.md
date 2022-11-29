# Smart-Soap
Arduino code for a protype "smart" soap dispenser, which was a liquid-level sensor immersed in a soap dispenser hooked up to a WiFi-equipped Arduino.

Sensor samples the liquid-level every second and watches for changes. If a significant and stable change in the liquid level is detected, the Arduino will send an HTTP post to an API endpoint.

Would probably require a little bit of tinkering to get it working for another project, but not that much. Tried to make the code as general/portable as possible.
