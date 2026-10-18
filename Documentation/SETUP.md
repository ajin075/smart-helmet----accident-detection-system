# STM32CubeIDE setup (reconstruction guide)

This is a **guide**, not a claim that the provided files form a complete CubeIDE project.

1. Install STM32CubeIDE and create a **new STM32 project** for the exact STM32F103C8T6 variant on your board.
2. Configure GPIO: PA0 input for IR; PB12 output for buzzer; PB13 input with pull-up for cancel button. PC13 is initialized by `Helmet_Init()` (active-low onboard LED).
3. Enable I2C1 for MPU6050; choose pins and speeds consistent with the actual wiring.
4. Enable USART1 RX for GPS and USART2 TX/RX for GSM; configure baud rates to match your actual modules. The original project baud rates are unknown.
5. Generate HAL initialization code and confirm `hi2c1`, `huart1`, and `huart2` exist in the generated code.
6. Copy `Firmware/smart_helmet.c` into `Core/Src/` and `Firmware/smart_helmet.h` into `Core/Inc/` of the newly generated project.
7. After generated GPIO, I2C, and UART initialization calls in `main()`, call `Helmet_Init()` **once**. Inside `while (1)`, call `Helmet_Run()` repeatedly.
8. Replace `EMERGENCY_NUMBER` in `smart_helmet.c` with a real number **only in your local build**; avoid publishing private phone numbers in public repositories.
9. Build, check compiler output, and validate all module connections and functions on the actual board before using the system.

Example integration **snippet only**, not a replacement for generated `main.c`:

```c
/* USER CODE BEGIN Includes */
#include "smart_helmet.h"
/* USER CODE END Includes */

/* After MX_GPIO_Init(), MX_I2C1_Init(), MX_USART1_UART_Init(),
   and MX_USART2_UART_Init(): */
Helmet_Init();

/* Inside the generated while (1) loop: */
Helmet_Run();
```

**Missing artifacts:** Original `.ioc`, CubeIDE `.project`/`.cproject`, generated `main.c`, clock configuration, startup/linker files, HAL drivers, and exact pin/baud/wiring documentation.
