CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc
LDFLAGS  ?= -lpthread -lstdc++fs
TARGET    = hetrix
PREFIX   ?= /usr/local

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): src/main.cpp src/*.hpp
	@echo "  >> compiling hetrix..."
	$(CXX) $(CXXFLAGS) src/main.cpp -o $(TARGET) $(LDFLAGS)
	@echo "  ✔  built: ./$(TARGET)"

install: $(TARGET)
	@echo "  >> installing to $(PREFIX)/bin/$(TARGET)"
	install -Dm755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	@#im gonna put my seed in you
	@HDIR="$${HETRIX_DIR:-$$HOME/hetrix}"; \
	 mkdir -p "$$HDIR/bin" "$$HDIR/workspace" "$$HDIR/repos"; \
	 if [ ! -f "$$HDIR/index.json" ] && [ -f index.json ]; then \
	     cp index.json "$$HDIR/index.json"; \
	     echo "  ✔  seeded: $$HDIR/index.json"; \
	 fi
	@echo "  ✔  done. run: hetrix help"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	@echo "  ✔  removed $(PREFIX)/bin/$(TARGET)"

clean:
	rm -f $(TARGET)
