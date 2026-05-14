Basic idea: get jokes from internet and show them on screen.

This is a demo of a few things:

- mostly off (deep sleep) device that wakes on button and connects to wifi fast
- captive portal with custom UI: since it only has 2 buttons, you can config over wifi, which is easier. you can hold B button to enable "config mode"
- get remote jokes from https://sv443.net/jokeapi/v2/ and parse JSON
- word-wrap display
- including HTML from a seperate file for easier editing

I think it shows off an operational-mode that is pretty good for these devices, since they run on batteries, and should feel like when you press the button they just activate, not do a bunch of boot stuff, so it's ideal for a pocket AI helper, etc.


```sh
# build & upload
pio run -e pocketjoke -t upload
```
