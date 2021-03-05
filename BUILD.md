# Analogcolor, how to build

Linux, BSD:

 1. Install git, cmake, gcc (clang).
 2. Clone the repository: git clone http://code.mastervirt.ru/analogcolor && cd analogcolor
 3. Get submodules: git submodule update --init --recursive --remote
 4. Create directory to build: mkdir build && cd build
 5. Build it: cmake ../src/ &&  make
