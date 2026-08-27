# PCB Led dispaly wifi clock

# Introduction: 
A led display taking avantage of the transparency of PCBs FR4 board charliplexing a couple of leds with a ESP32 C3, it display time  and temperature, auto time sync based on IP.
<img width="2236" height="1326" alt="20260814_115938" src="https://github.com/user-attachments/assets/0c8ef493-8c9e-4351-bac5-b09b48d5e743" />

Leds are mounted sideways in the bottom of the board.

<img width="3000" height="4000" alt="20260728_200018" src="https://github.com/user-attachments/assets/e89deed8-6aca-40ce-afee-97134708f9e2" />

<img width="1676" height="1535" alt="20260813_190813" src="https://github.com/user-attachments/assets/c8df0108-78f4-4db1-98c9-d2e753f6749f" />

Time display, in person the display is much more define and vivid thanks to human eyes persistence of vision, because of charlyPlexing of the leds the camera washout the result.

<img width="1658" height="1441" alt="20260813_190823" src="https://github.com/user-attachments/assets/840e3d11-ebf7-4f96-81f5-093efb4d8e2f" />

Temp display, it uses temperature from nearest whether station available in auto IP setting.

<img width="1834" height="1539" alt="20260813_190843" src="https://github.com/user-attachments/assets/ba181ecf-2dc7-4f63-a56b-2b8156dfffd5" />

When the board is power up or reseted, AP is enable for 1 minute, with any wifi capable device you can connect to ClockSetup then enter in a browser IP:192.168.4.1
and password:12345678
Then a portal is opened in the browser to config the clock and update the firmware, the clock can also be used offline if desired.
Enter your network name and password to use auto sync feature, only 2.4ghz networks are compatible.

<img width="1677" height="1580" alt="20260813_190858" src="https://github.com/user-attachments/assets/c79468b9-df62-4c2b-862e-3c892521c4bd" />

If auto sync is enabled in portal, the clock auto syncs every time.

<img width="1820" height="1152" alt="20260814_111958" src="https://github.com/user-attachments/assets/e5218064-cee5-4569-b5e5-731c7501173a" />

To have better definition of the digits a 3d printed diffusor/case is recommended, white color gives the best results, if you have a multicolor printer the first 4 layers can be printed in black to prevent led bleeding in the back, painting the back also works, be carful of some paints may contain metal that can decrease wifi performance.

<img width="2263" height="1356" alt="20260814_112311" src="https://github.com/user-attachments/assets/a3d10305-ad62-412f-80c6-2a91aec4f0dc" />

# ESP32 C3 board:
You need to manually solder the esp32 C3 super mini to the board it is beginners friendly since the pads are pretty big, you only need to be careful to buy the same pin placement board in Aliexpress or local supplier.

<img width="702" height="527" alt="Sin título" src="https://github.com/user-attachments/assets/a2b4d4ee-3948-4200-8df7-c21813804198" />

To join the difussor to the board you can use some 1.75mm filament preferably PLA since it melts very easily and with a lighter burn the point and pressed with you fingers, cut with some plyers and repat form the other side, alternatively you can use the filaments to align the boards glue it, and then cut the excess, or use some 2mm screws and nuts.

<img width="2744" height="1512" alt="20260814_113749" src="https://github.com/user-attachments/assets/baa21291-d857-48c5-8fe1-7e0899a7fff9" />

# Flashing the firmware:
You need to flash the ESP32 C3 in arduino or using other ESP32 flashing solution with the compiled binary.
If you are using the arduino ide you need to install ESP32 by Espressif in board manager then burn bootloader to the boards and then send the program with this board settings.

<img width="589" height="631" alt="Captura de pantalla 2026-08-14 151308" src="https://github.com/user-attachments/assets/90be87ea-62ce-4b1c-9e00-87717a713805" />

Once flashed you can use the clock portal to config and update the firmware, using the compiled binary in releases.

# Portal Interface
<img width="535" height="945" alt="Screenshot_20260817_114501_Chrome" src="https://github.com/user-attachments/assets/667816ad-a1a0-4d6b-9199-6a6ecc898a90" />


