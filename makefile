CXX = i686-w64-mingw32-g++
TARGET = Syringe.exe
SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp, %.o, $(SRC))

CXXFLAGS = -c -Wall -O2

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ -lcomctl32

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGET)
