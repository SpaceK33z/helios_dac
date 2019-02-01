# JavaScript example

Make sure you have Node v8+ and run `npm install`. If it fails, checkout https://github.com/cmake-js/fastcall#requirements.

Then run `node index.js` in this directory.

Note: this was only tested on macOS, the binary was compiled with:

```bash
g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDacAPI.cpp
g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDac.cpp
g++ -shared -o libHeliosDacAPI.dylib HeliosDacAPI.o HeliosDac.o libusb-1.0.0.dylib
```