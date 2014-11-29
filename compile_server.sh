#! /bin/bash

rm -rf ufbp_server
g++ ufbp_server.cc socket_util.cc -o ufbp_server
