#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define LED_PIN_GREEN 11
#define LED_PIN_BLUE 12
#define LED_PIN_RED 13
#define JOYSTICK_Y_PIN 26  // GPIO para eixo Y
#define JOYSTICK_X_PIN 27  // GPIO para eixo X
#define JOYSTICK_BUTTON 22 // GPIO para botão do Joystick
#define BOTAO_A 5 // GPIO para botão A
#define BOTAO_B 6 // GPIO para botão B

ssd1306_t ssd; // Inicializa a estrutura do display

// Método para desativar/ativar PWMs
volatile uint cont = 0; // Contador de interrupções
void disable_enable_pwm() {
  
  uint slice_num_blue = pwm_gpio_to_slice_num(LED_PIN_BLUE); // Obtém o número do slice PWM do LED azul 
  uint slice_num_red = pwm_gpio_to_slice_num(LED_PIN_RED); // Obtém o número do slice PWM do LED vermelho
  if (cont % 2 == 0) {
    pwm_set_enabled(slice_num_blue, false); // Desativa o slice PWM do LED azul
    pwm_set_enabled(slice_num_red, false); // Desativa o slice PWM do LED vermelho
  } else {
    pwm_set_enabled(slice_num_blue, true); // Ativa o slice PWM do LED azul
    pwm_set_enabled(slice_num_red, true); // Ativa o slice PWM do LED vermelho
  }
  cont++;
}

// Variáveis para debounce
volatile absolute_time_t last_interrupt_time_button_a = 0;
volatile absolute_time_t last_interrupt_time_js_button = 0;

#include "pico/bootrom.h" // Inclusão da biblioteca para interrupção do Botão B

// Interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events) {
  if (gpio == BOTAO_A) { // Botão A
      // Debounce para o Botão A
      absolute_time_t actual_time = get_absolute_time(); // Obtém o tempo atual
      int64_t time_diff = absolute_time_diff_us(last_interrupt_time_button_a, actual_time); // Calcula a diferença de tempo
      if (time_diff < 200000) return; // Se a diferença de tempo for menor que 200ms, retorna sem fazer nada
      last_interrupt_time_button_a = actual_time; // Atualiza o tempo da última interrupção

      disable_enable_pwm(); // Desativa/ativa PWMs
  } 
  else if (gpio == JOYSTICK_BUTTON) { // Botão do Joystick
      // Debounce para o Botão do Joystick
      absolute_time_t actual_time = get_absolute_time(); // Obtém o tempo atual
      int64_t time_diff = absolute_time_diff_us(last_interrupt_time_js_button, actual_time); // Calcula a diferença de tempo
      if (time_diff < 200000) return; // Se a diferença de tempo for menor que 200ms, retorna sem fazer nada
      last_interrupt_time_js_button = actual_time; // Atualiza o tempo da última interrupção

      gpio_put(LED_PIN_GREEN, !gpio_get(LED_PIN_GREEN)); // Inverte o estado do LED verde

      bool cor_borda = true; // Cor da borda
      ssd1306_rect(&ssd, 5, 5, 118, 56, cor_borda, false); // Desenha um retângulo
      ssd1306_send_data(&ssd); // Atualiza o display
  } 
  else if (gpio == BOTAO_B) { // Botão B
   reset_usb_boot(0, 0); // Reseta o Pico e entra no modo de inicialização USB
  }
}

// Configuração dos pinos
void setup() {
  // Configuração inicial dos GPIOs
  gpio_init(JOYSTICK_BUTTON);
  gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
  gpio_pull_up(JOYSTICK_BUTTON);

  gpio_init(BOTAO_A);
  gpio_set_dir(BOTAO_A, GPIO_IN);
  gpio_pull_up(BOTAO_A);

  gpio_init(BOTAO_B);
  gpio_set_dir(BOTAO_B, GPIO_IN);
  gpio_pull_up(BOTAO_B);

  // Configura I2C e display
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Inicializa ADC para o joystick
  adc_init();
  adc_gpio_init(JOYSTICK_X_PIN); // Canal 0 (GPIO26)
  adc_gpio_init(JOYSTICK_Y_PIN); // Canal 1 (GPIO27)

  // Configura LED verde como saída digital
  gpio_init(LED_PIN_GREEN);
  gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
  gpio_put(LED_PIN_GREEN, 0); // Desliga LED verde
}

// Configura PWM para um GPIO específico
void pwm_setup(uint GPIO_LED) {
  gpio_set_function(GPIO_LED, GPIO_FUNC_PWM); // Define função PWM para o pino
  uint slice_num = pwm_gpio_to_slice_num(GPIO_LED); // Obtém o número do slice
  pwm_config config = pwm_get_default_config(); // Configuração padrão
  pwm_config_set_wrap(&config, 4095); // Wrap em 4095 para 12 bits
  pwm_init(slice_num, &config, true); // Inicializa PWM
}

// Função Principal
int main() {
  stdio_init_all(); // Inicializa comunicação serial
  setup(); // Configuração inicial
  
  gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

// I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false); // Limpa o display
  ssd1306_send_data(&ssd);

  // Configura PWM para os LEDs vermelho e azul
  pwm_setup(LED_PIN_RED);
  pwm_setup(LED_PIN_BLUE);

  // Declaração de variáveis
  bool cor = true;
  uint16_t adc_value_x, adc_value_y;
  char str_x[5], str_y[5];

  // Loop infinito
  while (true) {
    // Lê valores do joystick
    adc_select_input(0); // Canal X (ADC0)
    adc_value_x = adc_read();
    adc_select_input(1); // Canal Y (ADC1)
    adc_value_y = adc_read();

    // Controle do LED vermelho (eixo X)
    if (adc_value_y > 2100) {
      pwm_set_gpio_level(LED_PIN_RED, (adc_value_y - 2048) * 2); // Aumenta brilho
    } else if (adc_value_y < 1900) {
      pwm_set_gpio_level(LED_PIN_RED, (2048 - adc_value_y) * 2); // Brilho inverso
    } else {
      pwm_set_gpio_level(LED_PIN_RED, 0); // Desliga no centro
    }

    // Controle do LED azul (eixo Y)
    if (adc_value_x > 2100) {
      pwm_set_gpio_level(LED_PIN_BLUE, (adc_value_x - 2048) * 2);
    } else if (adc_value_x < 1900) {
      pwm_set_gpio_level(LED_PIN_BLUE, (2048 - adc_value_x) * 2);
    } else {
      pwm_set_gpio_level(LED_PIN_BLUE, 0);
    }

    // LCD
    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo

    int square_x , square_y;

    // Mapeamento linear para X
    square_x = (adc_value_y * 120) / 4095;
    
    // Mapeamento linear para Y
    square_y = 56 - (adc_value_x * 56) / 4095;
    
    // Limites
    if (square_x < 8) square_x = 0;
    if (square_x > 120) square_x = 120;
    if (square_y < 8) square_y = 4;
    if (square_y > 56) square_y = 56;
    
    // Desenha o quadrado
    ssd1306_rect(&ssd, square_y, square_x, 8, 8, true, true);
    ssd1306_send_data(&ssd); // Atualiza o display

    printf("X: %d, Y: %d\n", square_x, square_y); // Imprime valores do joystick

    sleep_ms(100);
  }
}
