#include "HistoryBuffer.h"

HistoryBuffer::HistoryBuffer() {
  _head = 0;
  _tail = 0;
  _count = 0;
  
  // zera o buffer por segurança
  for (int i = 0; i < HISTORY_SIZE; i++) {
    _buffer[i].temperature = 0.0;
    _buffer[i].humidity = 0.0;
  }
}

bool HistoryBuffer::isFull() {
  return _count == HISTORY_SIZE;
}

bool HistoryBuffer::isEmpty() {
  return _count == 0;
}

void HistoryBuffer::enqueue(float temp, float hum) {
  _buffer[_head].temperature = temp;
  _buffer[_head].humidity = hum;

  // avança head do vetor
  _head = (_head + 1) % HISTORY_SIZE;

  if (!isFull()) {
    _count++; // Apenas cresce até atingir os 60
  } else {
    // se está cheio, o head chegou no tail.
    // empurra o head para frente para acompanhar (descarta dado mais antigo).
    _tail = (_tail + 1) % HISTORY_SIZE;
  }
}

String HistoryBuffer::getHistoryJSON() {
  if (isEmpty()) return "{\"temp\":[],\"hum\":[]}";

  String json = "{\"temp\":[";
  
  // iteração de tail para head
  int currentIndex = _tail;
  for (int i = 0; i < _count; i++) {
    json += String(_buffer[currentIndex].temperature, 1);
    if (i < _count - 1) json += ",";
    
    currentIndex = (currentIndex + 1) % HISTORY_SIZE;
  }
  
  json += "],\"hum\":[";
  
  currentIndex = _tail;
  for (int i = 0; i < _count; i++) {
    json += String(_buffer[currentIndex].humidity, 1);
    if (i < _count - 1) json += ",";
    
    currentIndex = (currentIndex + 1) % HISTORY_SIZE;
  }
  
  json += "]}";
  
  return json;
}