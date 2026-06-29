import { useState, useEffect, useRef } from 'react';
import mqtt from 'mqtt';
import LineChart from './components/LineChart';


const MQTT_OPTIONS = {
  host: "192.168.0.106",
  port: "9001",
  username: 'admin',
  password: 'admin',
  clientId: 'ReactDash_' + Math.random().toString(16).substr(2, 8),
  
};
const PREFIX = 'uffs/dev/';

const MAX_POINTS_AMBIENT = 30; // Aprox 2.5 minutos de histórico
const MAX_POINTS_RSSI = 15;    // Aprox 75 segundos de histórico (cobre a exigência de 60s)

function App() {
  const clientRef = useRef(null);

  const [brokerStatus, setBrokerStatus] = useState('Desconectado');
  const [espStatus, setEspStatus] = useState('Aguardando...');
  const [msgTimestamps, setMsgTimestamps] = useState([]);
  
  const [telemetry, setTelemetry] = useState({ temp: '--', hum: '--', rssi: '--' });
  
  const [isLocked, setIsLocked] = useState(false);
  const [leds, setLeds] = useState({ led1: false, led2: false });
  const [rgbColor, setRgbColor] = useState('#000000');

  // --- Estados dos Gráficos (Histórico) ---
  const [history, setHistory] = useState({
    timeLabels: [],
    tempData: [],
    humData: [],
  });
  const [rssiHistory, setRssiHistory] = useState({
    timeLabels: [],
    rssiData: []
  });


  useEffect(() => {
    // Este temporizador roda a cada 1 segundo para varrer a memória
    const timer = setInterval(() => {
      const timeLimit = Date.now() - 60000; // Define a "linha de corte" (60s atrás)
      
      // Mantém apenas os timestamps que são mais recentes que a linha de corte
      setMsgTimestamps(prev => prev.filter(timestamp => timestamp > timeLimit));
    }, 1000);
    
    return () => clearInterval(timer);
  }, []);

  // ================= EFEITO DE LIGAÇÃO MQTT =================
  useEffect(() => {
    if (clientRef.current) return; // Evita múltiplas ligações no Strict Mode do React

    const client = mqtt.connect(MQTT_OPTIONS);
    clientRef.current = client;

    client.on('connect', () => {
      setBrokerStatus('Conectado');
      client.subscribe(`${PREFIX}#`); // Ouve todos os tópicos do projeto
    });

    client.on('message', (topic, message) => {
      
      setMsgTimestamps(prev => [...prev, Date.now()]);
      
      const payload = message.toString();
      const now = new Date().toLocaleTimeString('pt-BR', { hour12: false });
      
      switch (topic) {
        case `${PREFIX}temperatura/celsius`:
          setTelemetry(prev => ({ ...prev, temp: payload }));
          setHistory(prev => {
            const newLabels = [...prev.timeLabels, now].slice(-MAX_POINTS_AMBIENT);
            const newTemp = [...prev.tempData, parseFloat(payload)].slice(-MAX_POINTS_AMBIENT);
            return { ...prev, timeLabels: newLabels, tempData: newTemp };
          });
          break;
          
        case `${PREFIX}umidade`:
          setTelemetry(prev => ({ ...prev, hum: payload }));
          setHistory(prev => {
            const newHum = [...prev.humData, parseFloat(payload)].slice(-MAX_POINTS_AMBIENT);
            return { ...prev, humData: newHum };
          });
          break;
          
        case `${PREFIX}rssi`:
          setTelemetry(prev => ({ ...prev, rssi: payload }));
          setRssiHistory(prev => {
            const newLabels = [...prev.timeLabels, now].slice(-MAX_POINTS_RSSI);
            const newRssi = [...prev.rssiData, parseInt(payload)].slice(-MAX_POINTS_RSSI);
            return { timeLabels: newLabels, rssiData: newRssi };
          });
          break;

        case `${PREFIX}status`:
          setEspStatus(payload.toUpperCase());
          break;

        case `${PREFIX}controle/bloqueio`:
          setIsLocked(payload === '1');
          break;

        case `${PREFIX}leds/estado`:
          const [l1, l2] = payload.split(',');
          setLeds({ led1: l1 === '1', led2: l2 === '1' });
          break;
        case `${PREFIX}notificacoes`:
          setNotificationCount(payload);
          break;
        default:
          break;
      }
    });

    client.on('offline', () => setBrokerStatus('Desconectado'));
    client.on('error', (err) => console.error('MQTT Erro:', err));

    return () => {
      if (clientRef.current) {
        clientRef.current.end();
        clientRef.current = null;
      }
    };
  }, []);

  // ================= FUNÇÕES DE COMANDO =================
  const toggleLed = (ledNumber) => {
    if (isLocked) {
      alert('Controle físico bloqueado pela chave SW1 no ESP32.');
      return;
    }
    
    const newLeds = { ...leds };
    if (ledNumber === 1) newLeds.led1 = !newLeds.led1;
    if (ledNumber === 2) newLeds.led2 = !newLeds.led2;

    const command = `${newLeds.led1 ? 1 : 0},${newLeds.led2 ? 1 : 0}`;
    clientRef.current?.publish(`${PREFIX}leds/comando`, command);
    setLeds(newLeds);
  };

  const handleRgbChange = (e) => {
    const hex = e.target.value;
    setRgbColor(hex);
    if (isLocked) return;

    const r = parseInt(hex.substr(1, 2), 16);
    const g = parseInt(hex.substr(3, 2), 16);
    const b = parseInt(hex.substr(5, 2), 16);

    clientRef.current?.publish(`${PREFIX}rgb/comando`, `${r},${g},${b}`);
  };

  const sendReset = () => {
    clientRef.current?.publish(`${PREFIX}controle/reset`, '1');
    alert('Comando de reset dos Mínimos e Máximos enviado ao ESP32!');
  };

  return (
    <div style={{ padding: '20px', fontFamily: 'Segoe UI, Tahoma, Geneva, Verdana, sans-serif', backgroundColor: '#f0f2f5', minHeight: '100vh' }}>
      <h1 style={{ textAlign: 'center', color: '#333' }}>Monitoramento Ambiental UFFS</h1>

      <div style={{ display: 'flex', gap: '20px', flexWrap: 'wrap', justifyContent: 'center', marginBottom: '30px' }}>
        
        <div style={cardStyle}>
          <h3 style={cardHeaderStyle}>⚙️ Status do Sistema</h3>
          <p>Broker MQTT: <strong style={{ color: brokerStatus === 'Conectado' ? '#2ecc71' : '#e74c3c' }}>{brokerStatus}</strong></p>
          <p>ESP32 (LWT): <strong style={{ color: espStatus === 'ONLINE' ? '#2ecc71' : '#e74c3c' }}>{espStatus}</strong></p>
          <p>Modo de Controle: <strong>{isLocked ? '🔒 Bloqueado (Local)' : '🔓 Liberado'}</strong></p>
          <p>Notificações (últimos 60s): <strong style={{ color: '#3498db', fontSize: '1.2em' }}>{msgTimestamps.length}</strong></p>
        </div>

        <div style={cardStyle}>
          <h3 style={cardHeaderStyle}>📊 Sensores (Live)</h3>
          <p>🌡️ Temperatura: <strong>{telemetry.temp} °C</strong></p>
          <p>💧 Umidade: <strong>{telemetry.hum} %</strong></p>
          <p>📡 Sinal Wi-Fi: <strong>{telemetry.rssi} dBm</strong></p>
        </div>

        <div style={cardStyle}>
          <h3 style={cardHeaderStyle}>🕹️ Painel de Controle</h3>
          
          <div style={{ display: 'flex', gap: '10px', marginBottom: '15px' }}>
            <button disabled={isLocked} onClick={() => toggleLed(1)} style={btnStyle(leds.led1)}>
              LED 1: {leds.led1 ? 'ON' : 'OFF'}
            </button>
            <button disabled={isLocked} onClick={() => toggleLed(2)} style={btnStyle(leds.led2)}>
              LED 2: {leds.led2 ? 'ON' : 'OFF'}
            </button>
          </div>
          
          <div style={{ marginBottom: '15px' }}>
            <label style={{ fontWeight: 'bold' }}>Cor do LED RGB: </label>
            <input type="color" value={rgbColor} onChange={handleRgbChange} disabled={isLocked} style={{ cursor: isLocked ? 'not-allowed' : 'pointer' }} />
          </div>

          <button onClick={sendReset} style={{ ...btnStyle(false), backgroundColor: '#e74c3c', color: 'white', width: '100%' }}>
            🔄 Resetar Histórico LCD
          </button>
        </div>

      </div>

      <h2 style={{ textAlign: 'center', color: '#333', marginTop: '40px' }}>Histórico de Telemetria</h2>
      
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '20px', justifyContent: 'center' }}>
        
        <div style={chartContainerStyle}>
          <LineChart 
            title="Temperatura (°C)" 
            label="Graus Celsius" 
            color="#e74c3c" 
            labels={history.timeLabels} 
            data={history.tempData} 
          />
        </div>

        <div style={chartContainerStyle}>
          <LineChart 
            title="Umidade Relativa (%)" 
            label="Umidade %" 
            color="#3498db" 
            labels={history.timeLabels} 
            data={history.humData} 
          />
        </div>

        <div style={chartContainerStyle}>
          <LineChart 
            title="Qualidade do Sinal (RSSI)" 
            label="dBm" 
            color="#f1c40f" 
            labels={rssiHistory.timeLabels} 
            data={rssiHistory.rssiData} 
          />
        </div>

      </div>
    </div>
  );
}

const cardStyle = { 
  backgroundColor: '#fff', 
  border: '1px solid #ddd', 
  borderRadius: '10px', 
  padding: '20px', 
  minWidth: '280px',
  boxShadow: '0 4px 6px rgba(0,0,0,0.05)',
  flex: '1 1 300px',
  maxWidth: '400px'
};

const cardHeaderStyle = { 
  marginTop: 0, 
  borderBottom: '2px solid #eee', 
  paddingBottom: '10px',
  color: '#2c3e50'
};

const chartContainerStyle = { 
  width: '100%', 
  minWidth: '300px',
  maxWidth: '500px',
  height: '300px',
  position: 'relative',
  border: '1px solid #ddd', 
  borderRadius: '10px', 
  padding: '15px', 
  backgroundColor: '#fff',
  boxShadow: '0 4px 6px rgba(0,0,0,0.05)'
};

const btnStyle = (isOn) => ({
  flex: 1,
  padding: '10px', 
  cursor: 'pointer',
  backgroundColor: isOn ? '#2ecc71' : '#ecf0f1',
  color: isOn ? '#fff' : '#333',
  border: '1px solid #bdc3c7', 
  borderRadius: '5px',
  fontWeight: 'bold',
  transition: 'all 0.3s'
});

export default App;