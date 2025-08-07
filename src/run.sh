#! /bin/sh
make
mkdir -p ./out
./build/cgull "$@"
./thirdparty/jasm/bin/jasm -i out -o out Main.jasm
java -cp out Main
