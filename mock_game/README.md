# Build and Run for Windows

## Build AngelScript

```
cd C:\dev\lang\asdbg-vscode\mock_game\angelscript\angelscript\projects\gnuc
make
```

## Build mock_engine

`ctrl + shift + B` in VSCode

or

```bat
g++ mock_engine.cpp -static-libstdc++ -static-libgcc -Langelscript\angelscript\lib -langelscript -lws2_32 -g -o mock_engine.exe
./mock_engine.exe
```

