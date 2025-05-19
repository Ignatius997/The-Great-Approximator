CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2
LDFLAGS :=

SRC_DIR := .
OBJ_DIR := .

SERVER_TGT := approx-server
CLIENT_TGT := approx-client

SERVER_SRC := $(SRC_DIR)/approx-server.cpp \
              $(SRC_DIR)/args.cpp \
              $(SRC_DIR)/log-server.cpp \
              $(SRC_DIR)/log.cpp \
              $(SRC_DIR)/err.cpp \
              $(SRC_DIR)/Rational.cpp \
              $(SRC_DIR)/io.cpp

CLIENT_SRC := $(SRC_DIR)/approx-client.cpp \
              $(SRC_DIR)/args.cpp \
              $(SRC_DIR)/log-client.cpp \
              $(SRC_DIR)/log.cpp \
              $(SRC_DIR)/err.cpp \
              $(SRC_DIR)/Rational.cpp \
              $(SRC_DIR)/io.cpp

SERVER_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/server_%.o,$(SERVER_SRC))
CLIENT_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/client_%.o,$(CLIENT_SRC))

.PHONY: all clean

all: $(SERVER_TGT) $(CLIENT_TGT)

# Kompilacja plików serwera z -DSERVER
$(OBJ_DIR)/server_%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DSERVER -c $< -o $@

# Kompilacja plików klienta z -DCLIENT
$(OBJ_DIR)/client_%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DCLIENT -c $< -o $@

$(SERVER_TGT): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT_TGT): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(SERVER_TGT) $(CLIENT_TGT)