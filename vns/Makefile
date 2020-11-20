CXX = g++
CXXFLAGS = -std=c++11 -Wpedantic -Wall -Wextra -Weffc++ -Werror -fno-default-inline -O0 -fPIC -ggdb3
TARGETS = sender #receiver

LIBS := -ljemalloc -lm -pthread -lpthread -lboost_program_options
INCLUDES := -I .

SRCS := $(wildcard *.cc)
OBJS := $(SRCS:.cc=.o)

SENDER_OBJS := $(filter-out receiver.o, $(OBJS))
RECEIVER_OBJS := $(filter-out sender.o, $(OBJS))

all: $(TARGETS)

sender: $(SENDER_OBJS)
	@echo Linking $@
	@$(CXX) $^ -o $@ $(LIBS)
	@rm -f $^

receiver: $(RECEIVER_OBJS)
	@echo Linking $@
	@$(CXX) $^ -o $@ $(LIBS)
	@rm -f $^

%.o: %.cc
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $< $(LIBS)

.PHONY: clean

clean:
	@echo Cleaning files
	@rm -rf $(OBJS) $(TARGETS)
