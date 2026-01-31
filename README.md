# Waddotrons fflag dumper ( physics1514_ discord )
### Simplified MacOS Roblox FFlag dumper, for usage with Intel roblox builds.
>> supports both intel and arm64 macs <<

### Prerequisits:
```
brew install capstone curl
```
If you dont have homebrew, or compiler tools use the following before anything else:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
```
xcode-select --install
```

### Build arm64 / silicon:
```
arch -x86_64 clang++ -O3 -std=c++17 -target x86_64-apple-darwin \main.cpp -o dumpfflags -L/usr/local/lib -lcapstone -lcurl
```

### Build Intel:
```
clang++ -O3 -std=c++17 -target x86_64-apple-darwin \main.cpp -o dumpfflags -L/usr/local/lib -lcapstone -lcurl
```
