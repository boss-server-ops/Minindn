#!/bin/bash

# Terminate processes with pkill
sudo pkill -9 aggregator
sudo pkill -9 consumer
sudo pkill -9 producer

echo "Processes 'aggregator', 'consumer', and 'producer' have been terminated."