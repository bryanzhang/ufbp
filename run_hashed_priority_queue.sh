#! /bin/bash

rm -rf hashed_priority_queue_test
g++ hashed_priority_queue_test.cc -o hashed_priority_queue_test
if [ -x hashed_priority_queue_test ]; then
  echo "Compile successfully!"
  ./hashed_priority_queue_test
else
  echo "Compile faield!"
  exit 1
fi
