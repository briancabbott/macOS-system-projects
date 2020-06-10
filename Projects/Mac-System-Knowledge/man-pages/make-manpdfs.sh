#!/bin/bash

# Set space as the delimiter
IFS=':'

#Read the split words into an array based on space delimiter
read -a strarr <<< "$MANPATH"

# Print each value of the array by using the loop
for val in "${strarr[@]}";
do
  printf "\n\n\n"
  printf "MANPAGE-DIR: $val\n"
  tree $val
done
