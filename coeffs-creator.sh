#!/bin/bash

while IFS= read -r line; do
    printf "COEFF %s\r\n" "$line" >> coeffs.txt
done