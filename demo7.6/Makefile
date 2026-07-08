CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LDLIBS ?=
GUI_LDLIBS ?= -mwindows -lcomctl32 -lcomdlg32 -lshell32 -lole32

TARGET := bin/sdb.exe
GUI_TARGET := bin/sdb_gui.exe
SOURCES := src/main.cpp src/backup.cpp
GUI_SOURCES := src/gui.cpp src/backup.cpp

.PHONY: all clean test

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(SOURCES)
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force bin | Out-Null"
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDLIBS)

$(GUI_TARGET): $(GUI_SOURCES)
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force bin | Out-Null"
	$(CXX) $(CXXFLAGS) -o $(GUI_TARGET) $(GUI_SOURCES) $(GUI_LDLIBS)

test: all
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_tests.ps1

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force bin,tests/tmp -ErrorAction SilentlyContinue"
