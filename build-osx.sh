echo Building \'hblhost-txt-osx-`uname -m`\' ...
clang -Wall main.cpp -D HBLHOST_TEXTRESPONSEONLY -o hblhost-txt-osx-`uname -m`
echo Building \'hblhost-mp4-osx-`uname -m`\' ...
clang -Wall main.cpp -o hblhost-mp4-osx-`uname -m`
echo Done!