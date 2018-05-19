
cp game "release/Par 99.app/Contents/MacOS/"
install_name_tool -change /usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib @executable_path/libSDL2-2.0.0.dylib "release/Par 99.app/Contents/MacOS/game"
rm -rf "release/Par 99.app/Contents/MacOS/res/"
cp -R res/ "release/Par 99.app/Contents/MacOS/res/"
