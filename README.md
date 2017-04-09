# HBLHost
HBLHost is a tiny webserver which hosts the payload to launch the [WiiU Homebrew Launcher](https://github.com/dimok789/homebrew_launcher) quickly over LAN with zero user inputs.

## Explanation
For a short overview see https://www.youtube.com/watch?v=0S9BQ8A7s2Y

## Setup
This requires to have the Homebrew Launcher ready on your WiiU (follow other guides how to setup the SD card etc.)
  1. Launch HBLHost on your PC in TXT mode (on Windows right click the systray icon and select 'Serve plain text file', on other platforms launch the hblhost-txt binary)
  2. Notice the URL for your PC in your network which also connects your WiiU (on Windows right click the systray icon and see the http-URLs listed in the menu, on other platforms check the http-URLs listed with 'Serving on ...' in the console output)
  3. On WiiU open the Internet Browser, close all tabs (if any) and open the http-URL from step 2 - You should see a message "HELLO WORLD"
  4. Go back to the home menu and launch the settings app and quit it (this will store your open tabs in the Internet Browser).
  5. On the PC, switch to MP4 mode (on Windows right click the systray icon and select 'Serve HBL MP4 file', on other platforms quit HBLHost and launch the hblhost-mp4 binary)
  6. On WiiU launch the Internet Browser from the home menu, it should immediately load the Homebrew Launcher due to the host serving the MP4 and the previous default tabs

## Advanced settings
Further options are available with these command line switches.
- Option -b <ip-address> limits the web server to a single network interface instead of serving on all interfaces
- Option -p <port-number> overrides the default port (which is 5000)
- Option -e <elf-path> overrides the default elf-executable path (the default path is shown in the Windows systray menu or the console output)

## License
HBLHost is available under the [GPL-3.0 license](https://choosealicense.com/licenses/gpl-3.0/).
