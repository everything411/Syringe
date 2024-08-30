CXX = i686-w64-mingw32-g++
TARGET = Syringe.exe
SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp, %.o, $(SRC))
STRIP = i686-w64-mingw32-strip

CXXFLAGS = -c -Wall -O2

$(TARGET): $(OBJ)
	$(CXX) -static -o $@ $^ -lcomctl32
	$(STRIP) $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGET)
