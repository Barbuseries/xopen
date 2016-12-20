CC = g++
DEFINES = -DEF_DEBUG=1
CFLAGS = -W -Wall -Wno-pointer-arith -Wno-write-strings -Wno-unused -g $(DEFINES)
LDFLAGS = 

BUILD_DIR=../build/
AOUT_DIR=../
SRC = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,$(BUILD_DIR)%.o,$(SRC))
AOUT = $(AOUT_DIR)xopen

all: $(AOUT)

$(AOUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)%.o: %.cpp %.h ef_utils.h xopen_common.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD_DIR)%.o: %.cpp ef_utils.h xopen_common.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@rm $(BUILD_DIR)*

cleanf: clean
	@rm $(AOUT)

run:
	./$(AOUT)

runv:
	valgrind ./$(AOUT)
