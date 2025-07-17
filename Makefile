CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lasound

all: mpPI

mpPI: app.cpp
	$(CXX) $(CXXFLAGS) -o mpPI app.cpp $(LDFLAGS)

clean:
	rm -f mpPI
