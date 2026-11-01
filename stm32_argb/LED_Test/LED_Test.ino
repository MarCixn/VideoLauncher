// Prosty blink - tylko rejestry

#define RCC_APB2ENR   (*(volatile uint32_t*)0x40021018)
#define GPIOC_CRH     (*(volatile uint32_t*)0x40011004)
#define GPIOC_ODR     (*(volatile uint32_t*)0x4001100C)
#define GPIOC_BRR     (*(volatile uint32_t*)0x40011014)

void setup() {
    // Włącz zegar GPIOC
    RCC_APB2ENR |= (1 << 4);
    
    // Czekaj
    for(volatile int i = 0; i < 1000; i++);
    
    // PC13 output push-pull 2MHz
    GPIOC_CRH &= ~(0xF << 20);
    GPIOC_CRH |= (0x2 << 20);
    
    // Zapal LED (LOW = ON)
    GPIOC_BRR = (1 << 13);
}

void loop() {
    GPIOC_ODR ^= (1 << 13);
    for(volatile uint32_t i = 0; i < 100000; i++);
}
