# Waddotrons FFlag dumper
### Simplified Static MacOS Roblox FFlag dumper, for usage with Intel roblox builds.
`>> supports both intel and arm64 macs <<`

## Prerequisites

**1. Install Command Line Tools:**
```bash
xcode-select --install
```

**2. Set up x86_64 Homebrew:**
*If you are on **Apple Silicon (M1/M2/M3)**, you likely only have the arm64 Homebrew. You must install the x86_64 version separately to compile this dumper.*

**ARM64 / Silicon:**
Install Rosetta if you dont have it:
```bash
softwareupdate --install-rosetta
```
Then install x86 Homebrew:
```bash
arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**Intel:**
*(Skip this if you already have Homebrew)*
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**3. Install Dependencies:**
**ARM64 / Silicon:**
```bash
arch -x86_64 /usr/local/bin/brew install capstone curl
```
**Intel:**
```bash
brew install capstone curl
```

---

# Building
**ARM64 / Silicon:**
```bash
arch -x86_64 clang++ -O3 -std=c++17 -target x86_64-apple-darwin main.cpp -o dumpfflags -I/usr/local/include -L/usr/local/lib -lcapstone -lcurl
```

**Intel:**
```bash
clang++ -O3 -std=c++17 -target x86_64-apple-darwin main.cpp -o dumpfflags -I/usr/local/include -L/usr/local/lib -lcapstone -lcurl
```

# Usage
**Universal run:**
*Args: roblox_binary_path > output_file*
```bash
./dumpfflags /Applications/Roblox.app/Contents/MacOS/RobloxPlayer > fflags.hpp
```
*The output file is created automatically.*

Any problems / errors lmk on discord **physics1514_**
