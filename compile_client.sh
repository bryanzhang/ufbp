#! /bin/bash

rm -rf ufbp_client
g++ ufbp_client.cc socket_util.cc -o ufbp_client
if [ -x ufbp_client ]; then
  ./ufbp_client 0.0.0.0 abc abc
else
  echo "Comile failed!"
  return 1
fi
