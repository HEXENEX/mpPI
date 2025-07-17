CXX = g++ -I/usr/include/nlohmann
CXXFLAGS = -std=c++17 -Wall -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lasound

all: mpPI

mpPI: main.cpp
	$(CXX) $(CXXFLAGS) -o mpPI main.cpp $(LDFLAGS)

clean:
	rm -f mpPI
