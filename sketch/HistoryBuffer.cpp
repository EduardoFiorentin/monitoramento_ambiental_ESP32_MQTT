#include "HistoryBuffer.h"

// Construtor: INICIALIZAÇÃO OBRIGATÓRIA dos ponteiros de memória!
HistoryBuffer::HistoryBuffer() {
  _head = 0;
  _tail = 0;
  _count = 0;
  
  // Zera o buffer por segurança
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
  // Salva o dado na posição atual do Head
  _buffer[_head].temperature = temp;
  _buffer[_head].humidity = hum;

  // Avança o Head de forma circular
  _head = (_head + 1) % HISTORY_SIZE;

  if (!isFull()) {
    _count++; // Apenas cresce até atingir os 60
  } else {
    // Se está cheio, o Head "atropelou" o Tail.
    // Precisamos empurrar o Tail para a frente para esquecer o dado mais antigo.
    _tail = (_tail + 1) % HISTORY_SIZE;
  }
}

// O pulo do gato para o MQTT: Gera um JSON sem destruir os dados da memória
String HistoryBuffer::getHistoryJSON() {
  if (isEmpty()) return "{\"temp\":[],\"hum\":[]}";

  String json = "{\"temp\":[";
  
  // Iteração do mais antigo (Tail) até ao mais recente (Head)
  int currentIndex = _tail;
  for (int i = 0; i < _count; i++) {
    json += String(_buffer[currentIndex].temperature, 1); // 1 casa decimal para poupar bytes na string
    if (i < _count - 1) json += ",";
    
    currentIndex = (currentIndex + 1) % HISTORY_SIZE;
  }
  
  json += "],\"hum\":[";
  
  currentIndex = _tail; // Reinicia a iteração para extrair a humidade
  for (int i = 0; i < _count; i++) {
    json += String(_buffer[currentIndex].humidity, 1);
    if (i < _count - 1) json += ",";
    
    currentIndex = (currentIndex + 1) % HISTORY_SIZE;
  }
  
  json += "]}";
  
  return json;
}