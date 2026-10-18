# Technical notes and validation checklist

These are findings from reading the provided application source, **not measured hardware results**.

## Known limitations / review items

1. **Accelerometer range:** The MPU6050 is set to ±2 g per axis, while the acceleration-vector threshold is 3.2 g. Axis clipping may distort impact magnitude; evaluate sensor range, sampling, filtering, and thresholds empirically.
2. **IR polarity:** `HelmetWorn()` interprets GPIO LOW as helmet worn. The original code's comment suggested some modules output HIGH on detection. Verify the actual module polarity.
3. **Initialization dependencies:** The code requires CubeMX-generated I2C1, USART1, USART2, and GPIO initialization. The provided repository alone cannot build.
4. **Blocking operation:** GPS acquisition can block up to 45 seconds, and GSM commands also block. During these periods, normal monitoring does not continue.
5. **NMEA coverage:** GPS accepts `$GPRMC` only. Some receivers emit `$GNRMC`; no NMEA checksum validation is performed.
6. **GSM response handling:** Fixed-size buffering and substring matching are simple and should be hardened for unreliable serial links. A `+CMGS` response is not proof that the recipient received the SMS.
7. **SMS number:** The placeholder is deliberately not a working phone number. Do not commit personal contact details.
8. **False alarm / repeat alert:** The implementation returns to monitoring after sending, without a persistent alert lockout or reset of all state. Evaluate repeated triggering.
9. **Power integrity:** SIM800L transmit bursts can demand substantial current. Verify a suitable regulator, battery, common ground, and voltage levels before connecting hardware.
10. **Safety:** This is an experimental prototype and must not be treated as a validated accident detector or sole emergency notification method.

## Suggested test checklist

- [ ] Confirm actual Blue Pill model and pinout
- [ ] Verify IR polarity and 1-second grace behavior
- [ ] Verify MPU6050 `WHO_AM_I` and sample readings
- [ ] Calibrate threshold with controlled, safe bench tests
- [ ] Confirm button debounce and 15-second cancellation
- [ ] Confirm GPS sentence format and coordinate signs
- [ ] Confirm GSM AT responses and actual SMS delivery
- [ ] Verify SMS failure indication and no-GPS fallback
- [ ] Test power supply stability with all modules connected
- [ ] Record real test results and limitations in the repository
