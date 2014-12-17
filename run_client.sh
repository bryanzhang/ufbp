#! /bin/bash

rm -rf ufbp_client
g++ ufbp_client.cc socket_util.cc -o ufbp_client
if [ -x ufbp_client ]; then
  echo "Comile successfully!"
  ./ufbp_client 0.0.0.0 sample_file sample_file
else
  echo "Comile failed!"
  exit 1
fi
