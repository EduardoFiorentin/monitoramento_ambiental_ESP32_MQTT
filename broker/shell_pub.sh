#!/bin/bash

USER="admin"
PASS="admin"
TOPIC="uffs/EduardoFiorentin/dev/rgb/comando"
MESSAGE="0,150,0"


# Publicar mensagem 
docker exec -it mqtt5 mosquitto_pub -h localhost \
    -t $TOPIC \
    -m $MESSAGE \
    -u $USER -P $PASS