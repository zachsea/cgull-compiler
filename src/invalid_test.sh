#! /bin/bash
for file in ../examples/invalid/*; do
  if [[ -f "$file" ]]; then
    echo "Running $file"
    ./run.sh "$file" $@
    if [ $? -eq 0 ]; then
      echo "No error when running $file"
      exit 1
    fi
  fi
done
echo "All tests completed."
