# mon_ambiental_ESP32_WIFI_MQTT

# Documentação dos Tópicos MQTT - Projeto de Monitorização Ambiental

Esta secção detalha a estrutura de dados e os tópicos MQTT utilizados para a comunicação bidirecional entre o microcontrolador ESP32 (Atuando como Cliente Wi-Fi) e o Dashboard na Nuvem. 

> **Nota:** O prefixo base configurado para todos os tópicos deste dispositivo é `uffs/dev/`.

## Tabela de Estrutura de Dados (Tópicos MQTT)

| Tópico MQTT | Direção | Tipo de Dado | Descrição / Uso |
| :--- | :--- | :--- | :--- |
| `uffs/dev/temperatura/celsius` | ESP32 &rarr; Nuvem | `String / Float` | Envia a temperatura atual em graus Celsius (ex: "25.40"). |
| `uffs/dev/temperatura/fahrenheit` | ESP32 &rarr; Nuvem | `String / Float` | Envia a temperatura atual convertida em Fahrenheit (ex: "77.72"). |
| `uffs/dev/umidade` | ESP32 &rarr; Nuvem | `String / Float` | Envia a humidade relativa do ar (ex: "60.50"). |
| `uffs/dev/rssi` | ESP32 &rarr; Nuvem | `String / Integer` | Envia a força do sinal Wi-Fi em dBm (ex: "-55"). Alimenta o gráfico de qualidade da rede. |
| `uffs/dev/status` | ESP32 &rarr; Nuvem | `String` | Status de ligação ("online" ou "offline"). Utiliza o recurso LWT (*Last Will and Testament*) do Broker para detetar quedas de energia no hardware. |
| `uffs/dev/leds/estado` | ESP32 &rarr; Nuvem | `String` | Feedback do estado físico dos LEDs. Formato `<LED1>,<LED2>` (ex: "1,0"). Sincroniza o Dashboard caso os botões físicos locais sejam premidos. |
| `uffs/dev/controle/bloqueio` | ESP32 &rarr; Nuvem | `String / Boolean`| Indica se o controlo remoto está bloqueado fisicamente pela chave SW1 ("1" = bloqueado, "0" = livre). |
| `uffs/dev/leds/comando` | Nuvem &rarr; ESP32 | `String` | Comando para ligar/desligar LEDs pelo Dashboard. Formato `<LED1>,<LED2>` (ex: "1,1"). Ignorado ativamente pelo firmware se o SW1 estiver ativo. |
| `uffs/dev/rgb/comando` | Nuvem &rarr; ESP32 | `String` | Comando para alterar a cor do LED RGB. Formato `<R>,<G>,<B>` (ex: "255,0,0"). |
| `uffs/dev/controle/reset` | Nuvem &rarr; ESP32 | `String / Integer` | Comando para efetuar o reset aos valores históricos de Máximas e Mínimas locais. Acionado ao receber o valor "1". |

---

## 💡 Notas Técnicas para a Integração do Dashboard (HTML/JS)

Se estiver a desenvolver a interface Web utilizando uma biblioteca MQTT para Javascript:

1. **Porta de Ligação (WebSockets):** O cliente JS no navegador não consegue utilizar ligações TCP puras. Certifique-se de que o seu cliente se liga ao Broker utilizando a porta configurada para WebSockets (neste projeto, a porta `9001`) e não a porta TCP padrão do microcontrolador (1883).
2. **Retenção de Mensagens (*Retain Flag*):** * Ao publicar **comandos** a partir do Dashboard (ex: `uffs/dev/leds/comando`), **NÃO** utilize a flag de retenção (`retain = true`). Isto evita que o ESP32 execute comandos antigos de forma involuntária (efeito "fantasma") após um eventual reinício ou quebra de rede.
   * A flag de retenção só é utilizada de forma ativa pelo ESP32 no tópico `uffs/dev/status` para garantir a funcionalidade correta da mensagem de testamento (LWT).