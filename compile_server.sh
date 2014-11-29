#! /bin/bash

rm -rf ufbp_server
g++ ufbp_server.cc socket_util.cc -o ufbp_server
if [ -x ufbp_server ]; then
  ./ufbp_server
else
  echo "Comile failed!"
  return 1
fi
