

#include <stdio.h>            
#include "pico/stdlib.h"      
#include "hardware/adc.h"     
#include "hardware/pwm.h"  
#include "hardware/i2c.h"   
#include "inc/ssd1306.h"
#include "inc/font.h"
#include <stdlib.h>
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define VRX_PIN 26  // cima - baixo
#define VRY_PIN 27 // direita - esquerda
#define SW_PIN 22
#define LED_VERDE 11
#define LED_AZUL 12  
#define LED_VERMELHO 13
#define SW_PIN 22
#define botaoA 5

// Variáveis globais
static volatile uint a = 1;
static volatile uint32_t last_time = 0; // Armazena o tempo do último evento (em microssegundos)

#define zona_desligada_min 1700
#define zona_desligada_max 2400

#define zona_morta_y_min 1800
#define zona_morta_y_max 2300


// Prototipação da função de interrupção
static void gpio_irq_handler(uint gpio, uint32_t events);
void inicializar_pinos();

uint pwm_init_gpio(uint gpio, uint wrap) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, wrap);
    
    pwm_set_enabled(slice_num, true);  
    return slice_num;  
}
// Variável global para controlar se os LEDs estão ativos
static volatile bool leds_habilitados = true;


int main() {
    
    stdio_init_all();
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

    adc_init(); 
    inicializar_pinos();


    uint pwm_wrap = 4096;  
    uint pwm_slice = pwm_init_gpio(LED_AZUL, pwm_wrap);  
    uint pwm_slice_vermelho = pwm_init_gpio(LED_VERMELHO, pwm_wrap);
    
    uint32_t last_print_time = 0; 
    gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(botaoA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    bool cor = true;
    uint16_t square_y = 35;
    uint16_t square_x = 35;
    while (true) {
        adc_select_input(0);  
        sleep_us(10);
        uint16_t vrx_value = adc_read(); // eixo x
        adc_select_input(1);
        sleep_us(10);
        uint16_t vry_value = adc_read(); // eixo y
        bool sw_value = gpio_get(SW_PIN) == 0;
        uint16_t intensidade_azul = abs(vrx_value - 2047) * 2;
        uint16_t intensidade_vermelho = abs(vry_value - 2047) * 2;

        
        cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_square(&ssd, square_x, square_y);



        // Movimento do quadrado SEM depender do estado dos LEDs
        if(vrx_value < zona_desligada_min){
            square_y += 8; // Move para baixo
        } else if(vrx_value > zona_desligada_max){
            square_y -= 8;  //Move para cima
        }
        if(square_y < 3) square_y = 3;
        if(square_y > HEIGHT - 15) square_y = HEIGHT - 15;

        if(vry_value < zona_morta_y_min){
            square_x -= 8;    // Move para esquerda
        } else if(vry_value > zona_morta_y_max){
            square_x += 8;    // Move para direita
        }
        if(square_x < 3) square_x = 3;
        if(square_x > WIDTH - 15) square_x = WIDTH - 15;

        // Controle dos LEDs apenas se estiverem habilitados
        if(leds_habilitados) {
            if(vrx_value < zona_desligada_min || vrx_value > zona_desligada_max){
                pwm_set_gpio_level(LED_AZUL, intensidade_azul);
            } else {
                pwm_set_gpio_level(LED_AZUL, 0);
            }

            if(vry_value < zona_morta_y_min || vry_value > zona_morta_y_max){
                pwm_set_gpio_level(LED_VERMELHO, intensidade_vermelho);
            } else {
                pwm_set_gpio_level(LED_VERMELHO, 0);
            }
        }

    

        ssd1306_send_data(&ssd); // Atualiza o display

        float duty_cycle = (vrx_value / 4095.0) * 100;  

        
        uint32_t current_time = to_ms_since_boot(get_absolute_time());  
        if (current_time - last_print_time >= 1000) {  
            printf("VRX: %u\n", vrx_value); 
            printf("Duty Cycle LED: %.2f%%\n", duty_cycle); 
            last_print_time = current_time;  
        }

        sleep_ms(100);  
    }

    return 0;  
}

// Função de interrupção com debouncing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    printf("A = %d\n", a);
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) // 200 ms de debouncing
    {
        last_time = current_time; // Atualiza o tempo do último evento
        if(gpio == SW_PIN){
            gpio_put(LED_VERDE, !gpio_get(LED_VERDE)); // Alterna o estado
        }else if(gpio == botaoA){
            leds_habilitados = !leds_habilitados;
            if(!leds_habilitados){
                pwm_set_gpio_level(LED_VERMELHO, 0);
                pwm_set_gpio_level(LED_AZUL, 0);
            }

        }
    }
}

void inicializar_pinos(){
    adc_gpio_init(VRX_PIN); 
    adc_gpio_init(VRY_PIN);
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);

    adc_gpio_init(VRX_PIN); 
    adc_gpio_init(VRY_PIN); 
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN); 
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, false); 
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, false); 
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, false);


}