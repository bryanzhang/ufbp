#! /bin/bash

rm -rf ufbp_server
g++ ufbp_server.cc socket_util.cc server_states.cc respack.cc -o ufbp_server
if [ -x ufbp_server ]; then
  echo "Comile successfully!"
  ./ufbp_server
else
  echo "Comile failed!"
  exit 1
fi
