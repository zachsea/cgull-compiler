#! /bin/bash
for file in ../examples/*; do
    echo "Running $file"
    ./run.sh "$file"
    if [ $? -ne 0 ]; then
        echo "Error running $file"
        exit 1
    fi
done
echo "All tests completed."
