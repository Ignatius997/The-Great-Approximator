#!/bin/bash
find . -maxdepth 1 -type f -exec cat {} + | wc -l