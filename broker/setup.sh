#!/bin/bash
echo "Subindo o container do Broker mqtt"

chmod 0644 mqtt/config/passwd mqtt/config/acl

docker run -d --name mqtt5 \
  -p 1883:1883 \
  -p 9001:9001 \
  -v "$(pwd)/mqtt/config:/mosquitto/config:ro" \
  -v "$(pwd)/mqtt/data:/mosquitto/data:rw" \
  -v "$(pwd)/mqtt/log:/mosquitto/log:rw" \
  --restart unless-stopped \
  eclipse-mosquitto

echo "Broker MQTT rodando em background"
echo "Porta TCP: 1883"
echo "WebSocket: 9001"