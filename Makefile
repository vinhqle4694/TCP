# Simple Makefile for TCP Library
# For more advanced builds, use CMake

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -I.
LDFLAGS = 

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lpthread
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lpthread
endif

# SSL support (uncomment if OpenSSL is available)
# CXXFLAGS += -DTCP_SSL_SUPPORT
# LDFLAGS += -lssl -lcrypto

# Source files
SOURCES = tcp_socket.cpp tcp_client.cpp tcp_server.cpp tcp_utils.cpp
OBJECTS = $(SOURCES:.cpp=.o)
LIBRARY = libtcp.a

# Example sources
EXAMPLE_SOURCES = $(wildcard examples/*.cpp)
EXAMPLE_TARGETS = $(EXAMPLE_SOURCES:examples/%.cpp=examples/%)

# Default target
all: $(LIBRARY) examples

# Build library
$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

# Build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build examples
examples: $(EXAMPLE_TARGETS)

examples/%: examples/%.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L. -ltcp $(LDFLAGS) -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(LIBRARY) $(EXAMPLE_TARGETS)

# Install (simple version)
install: $(LIBRARY)
	mkdir -p /usr/local/include/tcp
	cp *.h /usr/local/include/tcp/
	cp $(LIBRARY) /usr/local/lib/

# Uninstall
uninstall:
	rm -rf /usr/local/include/tcp
	rm -f /usr/local/lib/$(LIBRARY)

# Test compilation
test-compile: $(LIBRARY)
	@echo "Testing compilation of all examples..."
	@for example in $(EXAMPLE_TARGETS); do \
		echo "Building $$example..."; \
		$(CXX) $(CXXFLAGS) $(INCLUDES) $$example.cpp -L. -ltcp $(LDFLAGS) -o $$example || exit 1; \
	done
	@echo "All examples compiled successfully!"

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build library and examples"
	@echo "  $(LIBRARY)   - Build library only"
	@echo "  examples     - Build examples only"
	@echo "  clean        - Clean build files"
	@echo "  install      - Install library (requires sudo)"
	@echo "  uninstall    - Uninstall library (requires sudo)"
	@echo "  test-compile - Test compilation of all examples"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Example usage:"
	@echo "  make                    # Build everything"
	@echo "  make $(LIBRARY)         # Build library only"
	@echo "  make examples           # Build examples only"
	@echo "  make clean              # Clean build files"
	@echo "  sudo make install       # Install library system-wide"

.PHONY: all examples clean install uninstall test-compile help
