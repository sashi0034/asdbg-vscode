CXX = g++
SRC = mock_engine.cpp \
      $(wildcard angelscript/add_on/scriptarray/*.cpp) \
	  $(wildcard angelscript/add_on/scriptbuilder/*.cpp) \
	  $(wildcard angelscript/add_on/scriptdictionary/*.cpp) \
	  $(wildcard angelscript/add_on/scriptstdstring/*.cpp) \

OUT = mock_engine.exe
INCLUDE = -Iangelscript/angelscript/include
LIBS = -Langelscript/angelscript/lib -langelscript -lws2_32
FLAGS = -static-libstdc++ -static-libgcc -g

all:
	$(CXX) $(SRC) $(INCLUDE) $(LIBS) $(FLAGS) -o $(OUT)

run: all
	./$(OUT)

clean:
	del /Q $(OUT) 2>nul || true
