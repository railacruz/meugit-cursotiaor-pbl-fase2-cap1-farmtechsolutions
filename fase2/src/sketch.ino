/*
 * FarmTech Solutions - Fase 2
 * Sistema de irrigação inteligente (ESP32 + Wokwi)
 *
 * Hardware (Integrante 1): DHT22 (umidade), LDR (simula pH), 3 botões (N, P, K) e relé (bomba).
 * Cultura: MILHO. Regras agronômicas (Integrante 4) implementadas pelo Integrante 3.
 * ATENÇÃO: os limites são parâmetros DIDÁTICOS do protótipo, não recomendação de manejo.
 *
 * Regras de decisão:
 *   - Bomba: liga se umidade < 50% e desliga se umidade >= 50%.
 *   - pH (LDR) fora de 5,5 a 7,0 e nutrientes N/P/K não marcados geram apenas
 *     ALERTAS no Monitor Serial; eles não impedem a irrigação.
 *   - Condição segura: se a leitura do DHT22 for inválida (NaN), a bomba fica desligada.
 *
 * Comando manual opcional pelo Monitor Serial (previsão de chuva):
 *   C = chuva prevista (suspende a bomba)   |   S = sem chuva prevista
 */

#include <DHT.h>

// ---------- PINOS (iguais à montagem do Integrante 1) ----------
constexpr uint8_t PIN_DHT     = 4;   // DHT22 (umidade do solo, simulada)
constexpr uint8_t PIN_RELE    = 5;   // Relé azul (bomba d'água)
constexpr uint8_t PIN_BOTAO_N = 12;  // Botão Nitrogênio
constexpr uint8_t PIN_BOTAO_P = 13;  // Botão Fósforo
constexpr uint8_t PIN_BOTAO_K = 14;  // Botão Potássio
constexpr uint8_t PIN_LDR     = 34;  // LDR (simula o pH)

#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ---------- REGRAS AGRONÔMICAS - CULTURA DO MILHO (Integrante 4) ----------
constexpr float UMIDADE_MINIMA = 50.0;  // limite didático: abaixo disso liga a bomba (%)
constexpr float PH_MINIMO      = 5.5;   // faixa de referência de pH (Embrapa, aproximada)
constexpr float PH_MAXIMO      = 7.0;

// ---------- ESTADO ----------
bool previsaoChuva = false;             // opcional: atualizado pelo Monitor Serial (C/S)
bool bombaLigada = false;
unsigned long ultimaLeitura = 0;
constexpr unsigned long INTERVALO_MS = 2100;  // o DHT22 precisa de pelo menos 2 s entre leituras

// ---------- LEITURAS (Integrante 2 / hardware) ----------
bool lerBotao(uint8_t pino) {
  return digitalRead(pino) == LOW;      // INPUT_PULLUP: pressionado = LOW
}

float lerPH() {
  int adc = analogRead(PIN_LDR);        // 0 a 4095 (12 bits)
  return adc * 14.0 / 4095.0;           // escala de pH de 0 a 14
}

float lerUmidade() {
  return dht.readHumidity();
}

// ---------- ATUADOR E LÓGICA (Integrante 3) ----------
void acionarBomba(bool ligar) {
  bombaLigada = ligar;
  digitalWrite(PIN_RELE, ligar ? HIGH : LOW);
}

void lerComandoSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'C' || c == 'c') {
      previsaoChuva = true;
      Serial.println("[CLIMA] Chuva prevista");
    } else if (c == 'S' || c == 's') {
      previsaoChuva = false;
      Serial.println("[CLIMA] Sem chuva prevista");
    }
  }
}

void decidirIrrigacao(float umidade, float pH, bool n, bool p, bool k) {
  // 1) Alertas de pH (valor simulado pelo LDR). Não bloqueiam a irrigação.
  if (pH < PH_MINIMO || pH > PH_MAXIMO) {
    Serial.println("ALERTA: pH fora da faixa de referencia adotada para o milho.");
  } else {
    Serial.println("pH dentro da faixa simulada.");
  }

  // 2) Alertas de nutrientes (botões N, P e K). Não bloqueiam a irrigação.
  if (!n) Serial.println("ALERTA: nitrogenio nao marcado como disponivel.");
  if (!p) Serial.println("ALERTA: fosforo nao marcado como disponivel.");
  if (!k) Serial.println("ALERTA: potassio nao marcado como disponivel.");

  // 3) Controle da bomba
  if (isnan(umidade)) {                 // condição segura: leitura inválida desliga a bomba
    acionarBomba(false);
    Serial.println("ERRO: leitura invalida do DHT22. BOMBA DESLIGADA por seguranca.");
    return;
  }

  if (umidade < UMIDADE_MINIMA) {
    if (previsaoChuva) {                // opcional: chuva prevista suspende a irrigação
      acionarBomba(false);
      Serial.println("BOMBA DESLIGADA: indicador abaixo do limite, mas ha previsao de chuva.");
    } else {
      acionarBomba(true);
      Serial.println("BOMBA LIGADA: indicador abaixo do limite simulado.");
    }
  } else {
    acionarBomba(false);
    Serial.println("BOMBA DESLIGADA: indicador no limite ou acima dele.");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);          // bomba desligada no início

  pinMode(PIN_BOTAO_N, INPUT_PULLUP);
  pinMode(PIN_BOTAO_P, INPUT_PULLUP);
  pinMode(PIN_BOTAO_K, INPUT_PULLUP);
  pinMode(PIN_LDR, INPUT);

  analogReadResolution(12);
  dht.begin();

  Serial.println("Sistema de irrigacao inteligente iniciado.");
  Serial.println("Digite C (chuva prevista) ou S (sem chuva) no Monitor Serial.");
}

void loop() {
  lerComandoSerial();

  if (millis() - ultimaLeitura >= INTERVALO_MS) {
    ultimaLeitura = millis();

    bool n = lerBotao(PIN_BOTAO_N);
    bool p = lerBotao(PIN_BOTAO_P);
    bool k = lerBotao(PIN_BOTAO_K);
    float pH = lerPH();
    float umidade = lerUmidade();

    Serial.println("------------------------------------------");
    Serial.print("NPK: ");
    Serial.print(n ? "N " : "- ");
    Serial.print(p ? "P " : "- ");
    Serial.println(k ? "K" : "-");
    Serial.printf("pH (LDR): %.2f\n", pH);
    if (isnan(umidade)) {
      Serial.println("Umidade: ERRO");
    } else {
      Serial.printf("Umidade: %.1f%%\n", umidade);
    }
    Serial.printf("Chuva prevista: %s\n", previsaoChuva ? "SIM" : "NAO");

    decidirIrrigacao(umidade, pH, n, p, k);
  }
}
