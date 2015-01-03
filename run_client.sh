#! /bin/bash

rm -rf ufbp_client
rm -rf core.*
g++ -g ufbp_client.cc socket_util.cc client_states.cc -o ufbp_client
if [ -x ufbp_client ]; then
  echo "Comile successfully!"
  rm -rf out_shop_searcher.tar*
  ulimit -c unlimited
  time ./ufbp_client 0.0.0.0 shop_searcher.tar out_shop_searcher.tar
else
  echo "Comile failed!"
  exit 1
fi
