#! /bin/bash

echo "#include \"$1\"" > x.cc
g++ x.cc
