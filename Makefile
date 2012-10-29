CFLAGS=		-W -Wall -std=c99 -pedantic $(COPT)
CXXFLAGS=       -Wall $(COPT) -D_FILE_OFFSET_BITS=64
LINFLAGS=	-ldl -pthread

all: CFLAGS += -O2 -DNDEBUG
all: CXXFLAGS += -O2 -DNDEBUG
all: targets

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: targets

targets:
	$(CC) -c deps/mongoose/mongoose.c -I deps/mongoose $(CFLAGS)
	$(CXX) -o lib-face src/main.cpp mongoose.o -I . -I deps/mongoose $(CXXFLAGS) $(LINFLAGS)

test:
	$(CXX) -o tests/containers tests/containers.cpp -I . $(CXXFLAGS)
	tests/containers

perf:
	$(CXX) -o tests/rmq_perf tests/rmq_perf.cpp -I . $(CXXFLAGS)
	tests/rmq_perf

clean:
	rm mongoose.o lib-face tests/containers
