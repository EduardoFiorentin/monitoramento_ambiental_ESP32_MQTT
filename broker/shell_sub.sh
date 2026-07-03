#!/bin/bash

# Inscrever e monitorar todos os tópicos do broker 
docker exec -it mqtt5 mosquitto_sub \
    -h localhost -t "#" -v