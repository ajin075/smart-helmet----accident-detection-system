#include "smart_helmet.h"
#include <string.h>
#include <stdio.h>

/* STM32CubeMX-generated peripheral handles (normally defined in main.c). */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* USER SETTINGS: replace the placeholder before use. */
#define EMERGENCY_NUMBER      "********" 
#define IMPACT_THRESHOLD_G    3.2f
#define ALERT_WINDOW_MS       15000UL
#define GPS_FIX_WAIT_MS       45000UL
#define IR_GRACE_MS           1000UL

/* PIN MAPPING: must match STM32CubeMX-generated GPIO setup. */
#define IR_PORT GPIOA
#define IR_PIN  GPIO_PIN_0
#define BZ_PORT GPIOB
#define BZ_PIN  GPIO_PIN_12
#define BTN_PORT GPIOB
#define BTN_PIN  GPIO_PIN_13
#define LED_PORT GPIOC
#define LED_PIN  GPIO_PIN_13

/* MPU6050: I2C1, 7-bit address 0x68. */
#define MPU_ADDR           (0x68U << 1)
#define MPU_WHO_AM_I       0x75
#define MPU_PWR_MGMT_1     0x6B
#define MPU_SMPLRT_DIV     0x19
#define MPU_CONFIG         0x1A
#define MPU_GYRO_CONFIG    0x1B
#define MPU_ACCEL_CONFIG   0x1C
#define MPU_ACCEL_XOUT_H   0x3B
#define ACCEL_LSB_PER_G    16384.0f

typedef enum {
    ST_WAIT_HELMET,
    ST_MONITOR,
    ST_ALERT,
    ST_SENDING
} HelmetState_t;

static HelmetState_t state = ST_WAIT_HELMET;
static uint32_t alertStarted = 0;
static uint32_t notWornSince = 0;
static uint8_t mpuOk = 0;
static uint8_t accelHits = 0;

static void BuzzerOn(void)  { HAL_GPIO_WritePin(BZ_PORT, BZ_PIN, GPIO_PIN_SET); }
static void BuzzerOff(void) { HAL_GPIO_WritePin(BZ_PORT, BZ_PIN, GPIO_PIN_RESET); }
static void LedOn(void)     { HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET); }
static void LedOff(void)    { HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); }

static uint8_t HelmetWorn(void)
{
    /* Original project logic: LOW = worn. Confirm against the actual IR module. */
    return HAL_GPIO_ReadPin(IR_PORT, IR_PIN) == GPIO_PIN_RESET;
}

static uint8_t ButtonPressed(void)
{
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) != GPIO_PIN_RESET) return 0;
    HAL_Delay(20);
    return HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_RESET;
}

static uint8_t MPU_WriteReg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, 100) == HAL_OK;
}

static uint8_t MPU_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, 100) == HAL_OK;
}

static uint8_t MPU_Init(void)
{
    uint8_t who = 0;
    for (int i = 0; i < 3; i++) {
        HAL_Delay(50);
        if (MPU_ReadRegs(MPU_WHO_AM_I, &who, 1) && who == 0x68) {
            MPU_WriteReg(MPU_PWR_MGMT_1, 0x01);
            HAL_Delay(10);
            MPU_WriteReg(MPU_SMPLRT_DIV, 0x07);
            MPU_WriteReg(MPU_CONFIG, 0x00);
            MPU_WriteReg(MPU_GYRO_CONFIG, 0x00);
            MPU_WriteReg(MPU_ACCEL_CONFIG, 0x00);
            return 1;
        }
    }
    return 0;
}

/* Impact threshold, confirmed across three consecutive samples. */
static uint8_t AccelImpactDetected(void)
{
    uint8_t raw[6];
    int16_t ax, ay, az;
    if (!MPU_ReadRegs(MPU_ACCEL_XOUT_H, raw, 6)) return 0;
    ax = (int16_t)((raw[0] << 8) | raw[1]);
    ay = (int16_t)((raw[2] << 8) | raw[3]);
    az = (int16_t)((raw[4] << 8) | raw[5]);
    float fx = (float)ax / ACCEL_LSB_PER_G;
    float fy = (float)ay / ACCEL_LSB_PER_G;
    float fz = (float)az / ACCEL_LSB_PER_G;
    float mag2 = fx * fx + fy * fy + fz * fz;
    float thr2 = IMPACT_THRESHOLD_G * IMPACT_THRESHOLD_G;
    if (mag2 > thr2) {
        if (++accelHits >= 3) { accelHits = 0; return 1; }
    } else {
        accelHits = 0;
    }
    return 0;
}

/* NEO-6M GPS on USART1: parse $GPRMC sentences. */
static float ConvDDMM(char *s)
{
    float v = 0.0f;
    while (*s >= '0' && *s <= '9') { v = v * 10.0f + (float)(*s - '0'); s++; }
    if (*s == '.') {
        s++;
        float m = 0.1f;
        while (*s >= '0' && *s <= '9') { v += (float)(*s - '0') * m; m *= 0.1f; s++; }
    }
    int deg = (int)(v / 100.0f);
    float min = v - (float)deg * 100.0f;
    return (float)deg + min / 60.0f;
}

static uint8_t ParseGPRMC(char *line, float *lat, float *lon)
{
    char *f[8] = {0};
    uint8_t n = 0;
    char *p = line;
    f[n++] = p;
    while (*p && n < 8) {
        if (*p == ',') { *p = 0; f[n++] = p + 1; }
        p++;
    }
    if (n < 7) return 0;
    if (f[2][0] != 'A') return 0;
    *lat = ConvDDMM(f[3]);
    *lon = ConvDDMM(f[5]);
    if (f[4][0] == 'S') *lat = -*lat;
    if (f[6][0] == 'W') *lon = -*lon;
    return 1;
}

static uint8_t GPS_GetFix(float *lat, float *lon, uint32_t waitMs)
{
    char line[110];
    uint8_t li = 0;
    char c;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < waitMs) {
        if (HAL_UART_Receive(&huart1, (uint8_t *)&c, 1, 100) == HAL_OK) {
            if (c == '$') { li = 0; line[li++] = c; continue; }
            if (c == '\r') continue;
            if (c == '\n') {
                if (li > 0) {
                    line[li] = 0;
                    li = 0;
                    if (strncmp(line, "$GPRMC", 6) == 0 && ParseGPRMC(line, lat, lon)) return 1;
                }
                continue;
            }
            if (li < sizeof(line) - 1) line[li++] = c;
        }
    }
    return 0;
}

/* SIM800L GSM on USART2: AT-command SMS sender. */
static uint8_t gsmLine[64];
static uint8_t gsmLen = 0;

static void Gsm_SendStr(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), 1000);
}

static void Gsm_Flush(void)
{
    uint8_t b;
    while (HAL_UART_Receive(&huart2, &b, 1, 20) == HAL_OK) {}
    gsmLen = 0;
}

static uint8_t Gsm_Wait(const char *expect, uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    char c;
    gsmLen = 0;
    while (HAL_GetTick() - start < ms) {
        if (HAL_UART_Receive(&huart2, (uint8_t *)&c, 1, 50) == HAL_OK) {
            if (gsmLen < sizeof(gsmLine) - 1) gsmLine[gsmLen++] = (uint8_t)c;
            gsmLine[gsmLen] = 0;
            if (strstr((char *)gsmLine, expect)) return 1;
        }
    }
    return 0;
}

static uint8_t Gsm_SendSMS(const char *number, const char *message)
{
    uint8_t tries;
    for (tries = 0; tries < 5; tries++) {
        Gsm_Flush();
        Gsm_SendStr("AT\r");
        if (Gsm_Wait("OK", 1500)) break;
        HAL_Delay(1000);
    }
    if (tries == 5) return 0;
    Gsm_Flush();
    Gsm_SendStr("AT+CMGF=1\r");
    if (!Gsm_Wait("OK", 2000)) return 0;
    char cmd[40];
    sprintf(cmd, "AT+CMGS=\"%s\"\r", number);
    Gsm_Flush();
    Gsm_SendStr(cmd);
    if (!Gsm_Wait(">", 5000)) return 0;
    HAL_UART_Transmit(&huart2, (uint8_t *)message, strlen(message), 3000);
    uint8_t ctrlZ = 0x1A;
    HAL_UART_Transmit(&huart2, &ctrlZ, 1, 3000);
    if (Gsm_Wait("+CMGS", 20000)) return 1;
    return Gsm_Wait("OK", 5000);
}

static void FloatToStr(float v, char *out)
{
    if (v < 0.0f) { *out++ = '-'; v = -v; }
    int d = (int)v;
    int m = (int)((v - (float)d) * 100000.0f + 0.5f);
    if (m >= 100000) { m = 0; d++; }
    sprintf(out, "%d.%05d", d, m);
}

static void SendAccidentAlert(void)
{
    float lat = 0.0f, lon = 0.0f;
    char msg[170];
    strcpy(msg, "ACCIDENT ALERT! ");
    if (GPS_GetFix(&lat, &lon, GPS_FIX_WAIT_MS)) {
        char la[16], lo[16];
        FloatToStr(lat, la);
        FloatToStr(lon, lo);
        strcat(msg, "Location: http://maps.google.com/?q=");
        strcat(msg, la);
        strcat(msg, ",");
        strcat(msg, lo);
    } else {
        strcat(msg, "Locating failed - please call the rider.");
    }
    if (Gsm_SendSMS(EMERGENCY_NUMBER, msg)) {
        for (int i = 0; i < 3; i++) { LedOn(); HAL_Delay(300); LedOff(); HAL_Delay(300); }
    } else {
        for (int i = 0; i < 6; i++) { LedOn(); HAL_Delay(100); LedOff(); HAL_Delay(100); }
    }
}

void Helmet_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef led;
    led.Pin = LED_PIN;
    led.Mode = GPIO_MODE_OUTPUT_PP;
    led.Pull = GPIO_NOPULL;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &led);
    LedOff();
    BuzzerOff();
    mpuOk = MPU_Init();
    if (mpuOk) {
        for (int i = 0; i < 2; i++) { LedOn(); HAL_Delay(500); LedOff(); HAL_Delay(500); }
    } else {
        for (int i = 0; i < 5; i++) { LedOn(); HAL_Delay(100); LedOff(); HAL_Delay(100); }
    }
    state = ST_WAIT_HELMET;
}

void Helmet_Run(void)
{
    switch (state) {
        case ST_WAIT_HELMET:
            if (HelmetWorn()) state = ST_MONITOR;
            break;
        case ST_MONITOR:
            if (HelmetWorn()) {
                notWornSince = 0;
            } else {
                if (notWornSince == 0) notWornSince = HAL_GetTick();
                if (HAL_GetTick() - notWornSince >= IR_GRACE_MS) {
                    notWornSince = 0;
                    state = ST_WAIT_HELMET;
                    break;
                }
            }
            if (mpuOk && AccelImpactDetected()) {
                BuzzerOn();
                alertStarted = HAL_GetTick();
                state = ST_ALERT;
            }
            HAL_Delay(20);
            break;
        case ST_ALERT:
            if (((HAL_GetTick() / 200UL) & 1UL) == 0UL) BuzzerOn();
            else BuzzerOff();
            if (ButtonPressed()) {
                BuzzerOff();
                state = ST_MONITOR;
                break;
            }
            if (HAL_GetTick() - alertStarted >= ALERT_WINDOW_MS) {
                BuzzerOn();
                state = ST_SENDING;
            }
            break;
        case ST_SENDING:
            SendAccidentAlert();
            BuzzerOff();
            state = ST_MONITOR;
            break;
        default:
            state = ST_WAIT_HELMET;
            break;
    }
}
