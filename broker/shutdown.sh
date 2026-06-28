#!/bin/bash
echo "Parando e removendo o container mqtt5"

docker stop mqtt5
docker rm mqtt5

echo "Broker MQTT encerrado com sucesso"