CFLAGS = -Wall -g -O3 -DNUKED
CXXFLAGS = $(CFLAGS)
CC = gcc
CXX = g++
CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
CC = emcc
# -s USE_SDL=2
#CCm = clang
#CXX = clang++
#CC = emcc
#CXX = emcc
#FILES = $(wildcard *.hmp) $(wildcard *.hmq) $(wildcard *.bnk) descent.sng
FILES = descent.hog
LDFLAGS = $(foreach name,$(FILES),--preload-file $(name))
LDFLAGS += -O2 -s ASM_JS=1 -s TOTAL_MEMORY=33554432
#LDFLAGS += -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1
#LDFLAGS += -s EMTERPRETIFY_WHITELIST='["_main"]'
LDFLAGS += -s EXPORTED_FUNCTIONS="['_playerweb_gen','_playerweb_init','_playerweb_play']"
LDFLAGS += -s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
#-s ASM_JS=1

#COBJ = hmpfile.o hmpopl.o loaddata.o main.o dbopl.o
#COBJ = loaddata.o main2.o playerweb.o
COBJ = playerweb.o

#all: hmpopl.exe hmpopll hmp

EMCC_ONLY_FORCED_STDLIBS = 1

playerweb.wasm: playerweb.o
#	$(CC) -O3 -s EXPORT_ALL -o $@ $^ $(LDLIBS)
	$(CC) -O3 -s EXPORTED_FUNCTIONS="['_get_song_data', '_get_melobnk_data', '_get_drumbnk_data', '_playerweb_gen','_playerweb_init','_playerweb_play']" -o $@ $^ $(LDLIBS)

hmpopl.html: $(COBJ)
	$(CC) $(LDFLAGS) -o $@ $(COBJ) $(LDLIBS)

hmpopl.exe: $(COBJ)
	$(CC) -o $@ $^ $(LDLIBS) -lmingw32 -lSDL2main -lSDL2
#	 -lSDL_mixer

hmpopll: loaddata.lo main2.lo player.lo
	g++ -o $@ $^ $(LDLIBS) -lSDL2main -lSDL2
%.lo: %.c
	gcc $(CFLAGS) -c -o $@ $^
%.lo: %.cpp
	g++ $(CFLAGS) -c -o $@ $^

clean:
	rm -f *.o hmpopl.html hmpopl.js *.lo hmpopl.exe hmpopll *~

install:
	scp -Cp hmpopl.htm hmpopl.js hmpopl.data $(cat scpdest)

