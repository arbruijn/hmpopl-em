CFLAGS = -Wall -g -O2
CXXFLAGS = -Wall -g -O2
CC = gcc
CXX = g++
#CC = clang
#CXX = clang++
CC = /home/arne/src/emscripten/emcc
CXX = /home/arne/src/emscripten/emcc
LDLIBS = -lSDL
#FILES = $(wildcard *.hmp) $(wildcard *.hmq) $(wildcard *.bnk) descent.sng
FILES = descent.hog
LDFLAGS = $(foreach name,$(FILES),--preload-file $(name)) --js-library myaudio.js
LDFLAGS += -O2 -s ASM_JS=1
LDFLAGS += -s EXPORTED_FUNCTIONS="['_play','_main','_getsng']"
#-s ASM_JS=1

COBJ = hmpfile.o hmpopl.o loaddata.o main.o dbopl.o

hmpopl.js: $(COBJ) myaudio.js
	$(CXX) $(LDFLAGS) -o $@ $(COBJ) $(LDLIBS)

hmpoplo: hmpfile.o hmpopl.o loaddata.o maino.o dbopl.o oplmus.o oplsnd.o
	$(CXX) -o $@ $^ $(LDLIBS) -lSDL_mixer

clean:
	rm -f *.o hmpopl.html hmpopl.js

install:
	scp -Cp hmpopl.htm hmpopl.js hmpopl.data $(cat scpdest)

