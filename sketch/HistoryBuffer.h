#ifndef HISTORY_BUFFER_H
#define HISTORY_BUFFER_H

#include <Arduino.h>

#define HISTORY_SIZE 60 // 60 pontos = 1 hora (1 ponto por minuto)

struct EnvDataPoint {
  float temperature;
  float humidity;
};

class HistoryBuffer {
private:
  EnvDataPoint  _buffer[HISTORY_SIZE];
  int           _head;   // Onde o próximo dado será inserido
  int           _tail;   // Onde está o dado mais antigo
  int           _count;  // Quantidade atual de dados no buffer

public:
  HistoryBuffer();

  bool isFull();
  bool isEmpty();
  
  // Adiciona uma nova leitura. Se estiver cheio, sobrescreve a mais antiga.
  void enqueue(float temp, float hum);
  
  // Gera um JSON com as matrizes de temperatura e umidade para enviar via MQTT
  String getHistoryJSON();
};

#endif // HISTORY_BUFFER_H