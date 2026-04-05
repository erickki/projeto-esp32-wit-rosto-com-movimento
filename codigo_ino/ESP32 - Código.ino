#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================
//  CONFIGURAÇÕES DO DISPLAY
// ============================================================
#define LARGURA 128
#define ALTURA   64

Adafruit_SSD1306 display(LARGURA, ALTURA, &Wire, -1);
Adafruit_MPU6050 mpu;

// ============================================================
//  CONFIGURAÇÕES DO ROSTO
//  Mexa aqui para mudar tamanho e posição do rosto na tela
// ============================================================
#define ROSTO_CX   64    // centro X (0=esquerda, 128=direita)
#define ROSTO_CY   32    // centro Y do rosto
#define ROSTO_R    30    // raio do rosto (maior = rosto maior)

// ============================================================
//  CONFIGURAÇÕES DOS OLHOS
//  Mexa aqui para ajustar tamanho, posição e movimento
// ============================================================
#define OLHO_OFFSET_Y   -9   // quanto os olhos sobem do centro do rosto
#define OLHO_DIST_X     11   // distância horizontal de cada olho do centro
#define ORBITA_R         7   // raio da área branca (órbita) do olho
#define PUPILA_R         3   // raio da pupila preta
#define MOVIMENTO_MAX    4   // máximo de pixels que a pupila se desloca

// ============================================================
//  CONFIGURAÇÕES DA BOCA
// ============================================================
#define BOCA_OFFSET_Y   14   // quanto a boca desce do centro do rosto
#define BOCA_LARGURA    13   // metade da largura da boca
#define BOCA_ALTURA      5   // profundidade do sorriso

// ============================================================
//  CONFIGURAÇÕES DE SENSIBILIDADE
// ============================================================
#define SENSIBILIDADE  1.2   // 1.0 = normal | 2.0 = muito sensível

// ============================================================
//  CONFIGURAÇÕES DA PISCADA AUTOMÁTICA
//  Mexa aqui para mudar o tempo e duração da piscada
// ============================================================
#define INTERVALO_PISCAR  5000   // tempo entre piscadas em milissegundos
                                  // 5000 = 5 segundos | 3000 = 3 segundos
#define TEMPO_PISCAR       150   // duração da piscada em milissegundos
                                  // 150 = rápida | 400 = bem devagar

// ============================================================
//  VARIÁVEIS DE CONTROLE DA PISCADA (não precisa mexer)
// ============================================================
bool piscando             = false;  // true enquanto os olhos estão fechados
unsigned long ultimaPisca = 0;      // momento da última piscada
unsigned long inicioPisca = 0;      // momento em que a piscada atual começou

// ============================================================
//  SETUP — roda uma vez ao ligar
// ============================================================
void setup() {
  Serial.begin(115200);

  // Inicia o display OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED não encontrado!");
    while (true);
  }

  // Inicia o sensor MPU-6050
  if (!mpu.begin()) {
    Serial.println("MPU-6050 não encontrado!");
    while (true);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  ultimaPisca = millis(); // começa a contar do momento que ligou
  Serial.println("Pronto!");
}

// ============================================================
//  FUNÇÃO AUXILIAR: limita valor entre mínimo e máximo
// ============================================================
float limitar(float valor, float minVal, float maxVal) {
  if (valor < minVal) return minVal;
  if (valor > maxVal) return maxVal;
  return valor;
}

// ============================================================
//  FUNÇÃO: desenha um olho aberto ou fechado
//
//  cx, cy         = centro do olho na tela
//  piscando       = true = olho fechado (linha), false = aberto
//  pxOlho, pyOlho = posição da pupila quando aberto
// ============================================================
void desenharOlho(int cx, int cy, bool estaFechado, int pxOlho, int pyOlho) {
  if (estaFechado) {
    // Olho fechado: linha horizontal simples (pálpebra)
    display.drawFastHLine(cx - ORBITA_R, cy, ORBITA_R * 2, SSD1306_WHITE);
  } else {
    // Olho aberto: círculo branco + pupila preta por cima
    display.fillCircle(cx, cy, ORBITA_R, SSD1306_WHITE);
    display.fillCircle(pxOlho, pyOlho, PUPILA_R, SSD1306_BLACK);
  }
}

// ============================================================
//  LOOP PRINCIPAL — repete continuamente
// ============================================================
void loop() {
  unsigned long agora = millis();

  // ── Controle da piscada automática ────────────────────────
  // Verifica se já passou o intervalo desde a última piscada
  if (!piscando && (agora - ultimaPisca >= INTERVALO_PISCAR)) {
    piscando    = true;       // fecha os olhos
    inicioPisca = agora;      // registra quando fechou
  }

  // Verifica se o tempo de piscada já passou → reabre os olhos
  if (piscando && (agora - inicioPisca >= TEMPO_PISCAR)) {
    piscando    = false;      // abre os olhos
    ultimaPisca = agora;      // reinicia a contagem para a próxima piscada
  }

  // ── Leitura do sensor ──────────────────────────────────────
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Eixos mapeados conforme posição da protoboard
  // Se algum lado estiver invertido, troque o sinal de ax ou ay
  float ax = -(a.acceleration.y / 9.8) * SENSIBILIDADE; // horizontal
  float ay = -(a.acceleration.x / 9.8) * SENSIBILIDADE; // vertical

  ax = limitar(ax, -1.0, 1.0);
  ay = limitar(ay, -1.0, 1.0);

  // ── Deslocamento da pupila em pixels ──────────────────────
  float dx = ax * MOVIMENTO_MAX;
  float dy = ay * MOVIMENTO_MAX;

  // Mantém a pupila dentro do círculo da órbita
  float dist = sqrt(dx * dx + dy * dy);
  if (dist > MOVIMENTO_MAX) {
    dx = dx / dist * MOVIMENTO_MAX;
    dy = dy / dist * MOVIMENTO_MAX;
  }

  // ── Posições base dos olhos ───────────────────────────────
  int olhoEsqX = ROSTO_CX - OLHO_DIST_X;
  int olhoDirX = ROSTO_CX + OLHO_DIST_X;
  int olhoY    = ROSTO_CY + OLHO_OFFSET_Y;

  // ── Limite inferior da pupila (não passa da boca) ─────────
  int bocaCY         = ROSTO_CY + BOCA_OFFSET_Y;
  int limiteInferior = (bocaCY - PUPILA_R - 1) - olhoY;
  float dyFinal      = (dy > limiteInferior) ? limiteInferior : dy;

  // ── Posições finais das pupilas ───────────────────────────
  int pEsqX = olhoEsqX + (int)dx;
  int pEsqY = olhoY    + (int)dyFinal;
  int pDirX = olhoDirX + (int)dx;
  int pDirY = olhoY    + (int)dyFinal;

  // ── Desenho ───────────────────────────────────────────────
  display.clearDisplay();

  // 1. Rosto
  display.drawCircle(ROSTO_CX, ROSTO_CY, ROSTO_R, SSD1306_WHITE);

  // 2. Olho esquerdo
  desenharOlho(olhoEsqX, olhoY, piscando, pEsqX, pEsqY);

  // 3. Olho direito
  desenharOlho(olhoDirX, olhoY, piscando, pDirX, pDirY);

  // 4. Boca sorrindo
  // Para tristeza: troque "bocaCY + BOCA_ALTURA - curva" por "bocaCY - BOCA_ALTURA + curva"
  for (int i = -BOCA_LARGURA; i <= BOCA_LARGURA; i++) {
    int curva = (i * i * BOCA_ALTURA) / (BOCA_LARGURA * BOCA_LARGURA);
    int xb    = ROSTO_CX + i;
    int yb    = bocaCY + BOCA_ALTURA - curva;
    display.drawPixel(xb, yb, SSD1306_WHITE);
  }

  display.display();
  delay(20); // ~50 quadros por segundo
}