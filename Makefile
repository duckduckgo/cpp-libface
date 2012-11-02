CFLAGS=		-W -Wall -std=c99 -pedantic $(COPT)
CXXFLAGS=       -Wall $(COPT) -D_FILE_OFFSET_BITS=64
LINFLAGS=	-ldl -pthread
INCDEPS=        include/segtree.hpp include/sparsetable.hpp include/benderrmq.hpp \
                include/phrase_map.hpp include/suggest.hpp include/types.hpp \
                include/utils.hpp

all: CFLAGS += -O2 -DNDEBUG
all: CXXFLAGS += -O2 -DNDEBUG
all: targets

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: targets

targets: lib-face

lib-face: mongoose.o src/main.cpp $(INCDEPS)
	$(CXX) -o lib-face src/main.cpp mongoose.o -I . -I deps/mongoose $(CXXFLAGS) $(LINFLAGS)

mongoose.o: deps/mongoose/mongoose.h deps/mongoose/mongoose.c
	$(CC) -o mongoose.o -c deps/mongoose/mongoose.c -I deps/mongoose $(CFLAGS)

test:
	$(CXX) -o tests/containers tests/containers.cpp -I . $(CXXFLAGS)
	tests/containers

perf:
	$(CXX) -o tests/rmq_perf tests/rmq_perf.cpp -I . $(CXXFLAGS)
	tests/rmq_perf

clean:
	rm -f mongoose.o lib-face tests/containers tests/rmq_perf
