#! /bin/sh
make
mkdir -p ./out
./build/cgull "$@"
# only run assembler if there's no arguments besides the file name
if [ $# -eq 1 ]; then
  ./thirdparty/jasm/bin/jasm -i out -o out Main.jasm
  java -cp out Main
fi
