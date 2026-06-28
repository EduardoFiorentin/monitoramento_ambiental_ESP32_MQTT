## Comandos úteis 


```sh
docker exec -it mqtt5 mosquitto_sub -h localhost -t "#" -v

docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/temperatura/celsius" -m "25.4"
docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/umidade" -m "60.2"
docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/rssi" -m "-55"

# Comando para acender o LED 1
docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/leds/comando" -m "1"

# Comando para alterar a cor do LED RGB para vermelho
docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/rgb/comando" -m "255,0,0"

# Comando para resetar os limites Min/Max
docker exec -it mqtt5 mosquitto_pub -h localhost -t "uffs/dev/controle/reset" -m "1"

```