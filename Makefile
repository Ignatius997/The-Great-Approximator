CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2 -DNDEBUG

SRC_DIR := .
OBJ_DIR := .

SERVER_TGT := approx-server
CLIENT_TGT := approx-client

SERVER_SRC := $(SRC_DIR)/approx-server.cpp \
              $(SRC_DIR)/args.cpp \
              $(SRC_DIR)/io.cpp \
			  $(SRC_DIR)/netutils.cpp \
			  $(SRC_DIR)/communication.cpp \
			  $(SRC_DIR)/MessageCombinators.cpp

CLIENT_SRC := $(SRC_DIR)/approx-client.cpp \
              $(SRC_DIR)/args.cpp \
              $(SRC_DIR)/io.cpp \
			  $(SRC_DIR)/netutils.cpp \
			  $(SRC_DIR)/communication.cpp \
			  $(SRC_DIR)/MessageCombinators.cpp

SERVER_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/server-%.o,$(SERVER_SRC))
CLIENT_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/client-%.o,$(CLIENT_SRC))

.PHONY: all clean

all: $(SERVER_TGT) $(CLIENT_TGT)

# Compile server files z TGA_SERVER macro.
$(OBJ_DIR)/server-%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DTGA_SERVER -c $< -o $@

# Compile client files with TGA_CLIENT macro.
$(OBJ_DIR)/client-%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DTGA_CLIENT -c $< -o $@

$(SERVER_TGT): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT_TGT): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f *.o $(SERVER_TGT) $(CLIENT_TGT)
