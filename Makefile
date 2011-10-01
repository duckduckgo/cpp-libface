CFLAGS=		-W -Wall -std=c99 -pedantic $(COPT)
CXXFLAGS=       -Wall $(COPT)
LINFLAGS=	-ldl -pthread

all:
	$(CC) -c deps/mongoose/mongoose.c -I deps/mongoose $(CFLAGS) -O2 -DNDEBUG
	$(CXX) -o lib-face src/main.cpp mongoose.o -I . -I deps/mongoose $(CXXFLAGS) $(LINFLAGS) -O2 -DNDEBUG

debug:
	$(CC) -c deps/mongoose/mongoose.c -I deps/mongoose $(CFLAGS) -g
	$(CXX) -o lib-face src/main.cpp mongoose.o -I . -I deps/mongoose $(CXXFLAGS) $(LINFLAGS) -g

test:
	$(CXX) -o tests/containers tests/containers.cpp -I . $(CXXFLAGS)
	tests/containers

clean:
	rm mongoose.o lib-face tests/containers
