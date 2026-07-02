#ifndef HISTORY_BUFFER_H
#define HISTORY_BUFFER_H

#include <Arduino.h>

#define HISTORY_SIZE 60 // 60 pontos -> 1 hora -> 1 ponto por minuto

struct EnvDataPoint {
  float temperature;
  float humidity;
};

class HistoryBuffer {
private:
  EnvDataPoint  _buffer[HISTORY_SIZE];
  int           _head;   // onde proximo dado entra 
  int           _tail;   // aponta para o dado mais antigo
  int           _count;  // quantidade de dados em buffer

public:
  HistoryBuffer();

  bool isFull();
  bool isEmpty();
  
  void enqueue(float temp, float hum);  
  String getHistoryJSON();
};

#endif // HISTORY_BUFFER_H