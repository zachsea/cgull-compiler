#! /bin/sh
make
./build/cgull "$@"
# only run assembler if there's no arguments besides the file name and no error
RESULT=$?
if [ $# -eq 1 ] && [ $RESULT -eq 0 ]; then
  # for each .jasm file in out, run jasm on it
  for file in out/*.jasm; do
    ./thirdparty/jasm/bin/jasm -i out -o out $(basename $file)
    if [ $? -ne 0 ]; then
      echo "\nError: jasm failed on $file"
      exit 1
    fi
    echo "\nSuccessfully compiled $file"
  done
  java -cp out Main
  RESULT=$?
fi
exit $RESULT
