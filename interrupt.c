// ======================= BIBLIOTECAS ========================
#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/pio.h>
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#include "animacao_matriz.pio.h"  // Programa PIO compilado para controle dos LEDs

// ======================= CONSTANTES ========================
#define LED_AZUL     12    // Pino do LED azul de status
#define LED_VERDE     11    // Pino do LED azul de status

#define QTD_LEDS            25    // Número de LEDs na matriz 5x5
#define PINO_MATRIZ         7     // Pino de dados da matriz
#define BOTAO_B    6     // Pino do botão para aumentar número
#define BOTAO_A    5     // Pino do botão para diminuir número

// ====================== VARIÁVEIS GLOBAIS ====================
PIO controlador_pio;              // Controlador PIO (0 ou 1)
uint maquina_estado;              // Índice da máquina de estado PIO
volatile uint numero_atual = 0;   // Número exibido (0-9)
volatile uint32_t ultimo_toque = 0; // Temporização para debounce
const float fator_brilho = 0.8;   // 80% do brilho máximo (20% de redução)
bool state_azul = false;
bool state_verde = false;

// ====================== CONFIGURAÇÕES FIXAS ==================
// Padrões dos números (0-9) representados na matriz 5x5
const uint8_t formato_numerico[10][QTD_LEDS] = {
    // 0         1         2         3         4          (Posições na matriz)
    {0,1,1,1,0,
     0,1,0,1,0, 
     0,1,0,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0}, // 0

    {0,0,1,0,0,
     0,0,1,0,0, 
     0,0,1,0,0, 
     0,0,1,0,0, 
     0,0,1,0,0}, // 1

    {0,1,1,1,0,
     0,0,0,1,0, 
     0,1,1,1,0, 
     0,1,0,0,0, 
     0,1,1,1,0}, // 2

    {0,1,1,1,0,
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,1,0,0,0, 
     0,1,1,1,0}, // 3

    {0,1,0,0,0, 
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,0,1,0}, // 4

    {0,1,1,1,0,
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,1,1,1,0}, // 5

    {0,1,1,1,0,
     0,1,0,1,0, 
     0,1,1,1,0, 
     0,0,0,1,0, 
     0,0,0,1,0}, // 6

    {0,1,0,0,0,
     0,1,0,0,0, 
     0,1,0,0,0, 
     0,1,0,0,0, 
     0,1,1,1,0}, // 7

    {0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0}, // 8

    {0,1,0,0,0,
     0,1,0,0,0, 
     0,1,1,1,0, 
     0,1,0,1,0, 
     0,1,1,1,0},  // 9
};

// Mapeamento lógico-físico dos LEDs (corrige disposição da matriz)
const uint8_t ordem_leds[QTD_LEDS] = {
    0,1,2,3,4,9,8,7,6,5,10,11,12,13,14,19,18,17,16,15,20,21,22,23,24
};

// ===================== FUNÇÕES PRINCIPAIS ====================

// Gera cor vermelha com intensidade ajustada pelo brilho
uint32_t gerar_cor_vermelha() {
    return (uint8_t)(50 * fator_brilho) << 16;
}

// Atualiza a matriz com o número especificado
void atualizar_matriz(uint numero) {
    for (uint8_t posicao = 0; posicao < QTD_LEDS; posicao++) {
        // Determina se o LED atual deve estar aceso ou apagado
        uint8_t led_aceso = formato_numerico[numero][ordem_leds[posicao]];
        
        // Envia o comando de cor para o LED
        uint32_t cor = led_aceso ? gerar_cor_vermelha() : 0;
        pio_sm_put_blocking(controlador_pio, maquina_estado, cor);
    }
}


// Manipulador de interrupção para os botões
void tratar_interrupcao_botao(uint gpio, uint32_t eventos) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    
    // Filtro de debounce: ignora eventos com menos de 200ms do último
    if (tempo_atual - ultimo_toque > 200) {
        if (gpio == BOTAO_B) {
            gpio_put(LED_AZUL, !gpio_get(LED_AZUL));
            state_azul = !LED_AZUL;
            if (state_azul == 1){
            printf("Led azul foi acionado \n");
            }
            else{
            printf("Led azul foi desligado \n"); 
            }
        } 
        else if (gpio == BOTAO_A) {
            gpio_put(LED_VERDE, !gpio_get(LED_VERDE));
            if (LED_VERDE == 1){
                printf("Led verde foi acionado \n");
            }
            else{
                printf("Led verde foi desligado \n"); 
            }

        }
    }
}

void desenho(){
    
}

// ===================== PROGRAMA PRINCIPAL ====================
int main() {

      // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_t ssd; // Inicializa a estrutura do display
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  bool cor = true;

    // Configuração inicial do sistema
    controlador_pio = pio0;  // Usa controlador PIO 0
    bool clock_01 = set_sys_clock_khz(128000, false);  // Clock a 128MHz
    
    stdio_init_all();  // Inicializa USB/Serial para debug
    printf("Sistema Iniciado\n");
    if (clock_01) printf("Clock operando em %ld Hz\n", clock_get_hz(clk_sys));

    // ------ Configuração do PIO para controle da matriz ------
    uint offset = pio_add_program(controlador_pio, &animacao_matriz_program);
    maquina_estado = pio_claim_unused_sm(controlador_pio, true);
    animacao_matriz_program_init(controlador_pio, maquina_estado, offset, PINO_MATRIZ);

    // ------ Configuração dos LEDs de status ------
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, 0);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);

    // ------ Configuração dos botões com pull-up ------
    gpio_init(BOTAO_B);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);
    
    gpio_init(BOTAO_A);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &tratar_interrupcao_botao);
    
    // ------ Inicialização da matriz ------
    atualizar_matriz(0);  // Começa exibindo o número 0

    i2c_init(I2C_PORT, 400 * 1000);

  
    // ------ Loop principal ------
    while (true) {
        
                    // Atualiza o conteúdo do display com animações
                    ssd1306_fill(&ssd, !cor); // Limpa o display
                    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
                    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 10); // Desenha uma string
                    ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 30); // Desenha uma string
                    ssd1306_draw_string(&ssd, "azeiTona", 15, 48); // Desenha uma string      
                    ssd1306_send_data(&ssd); // Atualiza o display
                
                    sleep_ms(1000);

         // Certifica-se de que o USB está conectado
            char c;
            scanf("%c", &c);
            printf("Recebido: '%c'\n", c);
            if (c >= '0' && c <= '9') {
                numero_atual = c - '0'; // Converte o caractere para número
                atualizar_matriz(numero_atual);
            }
            sleep_ms(40);
            cor = !cor;
         }
}
