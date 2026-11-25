#include "pxt.h"

#if MICROBIT_CODAL
#include "nrf.h"
#endif

namespace guitarchord {

#if MICROBIT_CODAL
    static uint16_t seq0[4];
    static uint16_t seq1[4];
    static uint16_t seq2[4];

    // 1～15は従来のパルス幅（1/16単位）。16・17は複合パルス波形。
    static uint8_t waveformCode = 8;

    // フェード用の疑似音量。GPIOの振幅は変えられないため、
    // 各音のパルス幅を割合で狭めて実効エネルギーを下げます。
    static uint8_t volumePercent = 100;

    static uint32_t top0 = 0;
    static uint32_t top1 = 0;
    static uint32_t top2 = 0;
    static uint8_t shape0 = 8;
    static uint8_t shape1 = 8;
    static uint8_t shape2 = 8;
    static bool active0 = false;
    static bool active1 = false;
    static bool active2 = false;

    static uint16_t makeCompare(uint32_t top, uint8_t width) {
        if (width == 0) return 0;
        uint32_t numerator =
            top * (uint32_t)width * (uint32_t)volumePercent;

        uint32_t compare =
            (numerator + 800UL) / 1600UL; // / (16 * 100)

        if (compare < 1)
            compare = 1;
        if (compare >= top)
            compare = top - 1;

        return (uint16_t)compare;
    }

    static uint32_t sequenceLength(uint8_t shape) {
        return shape >= 16 ? 4 : 1;
    }

    // 一つの音の周期を4分割。異なる幅のパルスで倍音構成を変える。
    // 4分割全体が基音の1周期となるようPWM周波数を4倍にする。
    static void fillSequence(uint16_t *seq, uint32_t top, uint8_t shape) {
        if (shape == 16) {
            seq[0] = makeCompare(top, 12);
            seq[1] = 0;
            seq[2] = makeCompare(top, 5);
            seq[3] = 0;
        } else if (shape == 17) {
            seq[0] = makeCompare(top, 12);
            seq[1] = makeCompare(top, 5);
            seq[2] = 0;
            seq[3] = makeCompare(top, 8);
        } else {
            seq[0] = makeCompare(top, shape);
        }
    }

    static void updateActiveSequences() {
        if (active0 && top0 >= 2)
            fillSequence(seq0, top0, shape0);
        if (active1 && top1 >= 2)
            fillSequence(seq1, top1, shape1);
        if (active2 && top2 >= 2)
            fillSequence(seq2, top2, shape2);
    }

    static void startPwm(
        NRF_PWM_Type *pwm,
        NRF_GPIO_Type *gpio,
        uint32_t port,
        uint32_t pin,
        uint16_t *seq,
        uint32_t *storedTop,
        uint8_t *storedWaveform,
        bool *active,
        int frequency
    ) {
        if (frequency < 31)
            frequency = 31;
        if (frequency > 10000)
            frequency = 10000;

        pwm->TASKS_STOP = 1;
        pwm->ENABLE = 0;

        gpio->DIRSET = (1UL << pin);
        gpio->OUTCLR = (1UL << pin);

        pwm->PSEL.OUT[0] =
            (pin << PWM_PSEL_OUT_PIN_Pos) |
            (port << PWM_PSEL_OUT_PORT_Pos) |
            (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);

        pwm->PSEL.OUT[1] = 0xFFFFFFFF;
        pwm->PSEL.OUT[2] = 0xFFFFFFFF;
        pwm->PSEL.OUT[3] = 0xFFFFFFFF;

        pwm->MODE =
            (PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos);

        pwm->PRESCALER =
            (PWM_PRESCALER_PRESCALER_DIV_16 << PWM_PRESCALER_PRESCALER_Pos);

        uint32_t count = sequenceLength(waveformCode);
        uint32_t pwmFrequency = (uint32_t)frequency * count;
        uint32_t top = (1000000UL + pwmFrequency / 2) / pwmFrequency;

        if (top < 2)
            top = 2;
        if (top > 32767)
            top = 32767;

        pwm->COUNTERTOP =
            (top << PWM_COUNTERTOP_COUNTERTOP_Pos);

        pwm->DECODER =
            (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) |
            (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);

        *storedTop = top;
        *storedWaveform = waveformCode;
        *active = true;
        fillSequence(seq, top, *storedWaveform);

        pwm->SEQ[0].PTR = (uint32_t)seq;
        pwm->SEQ[0].CNT = count;
        pwm->SEQ[0].REFRESH = 0;
        pwm->SEQ[0].ENDDELAY = 0;

        // 複合波形はSEQ0とSEQ1を同一パターンにして連続再生する。
        // 従来の単一パルスは旧版同様のSEQ0構成を維持する。
        pwm->SEQ[1].PTR = count > 1 ? (uint32_t)seq : 0;
        pwm->SEQ[1].CNT = count > 1 ? count : 0;
        pwm->SEQ[1].REFRESH = 0;
        pwm->SEQ[1].ENDDELAY = 0;
        pwm->LOOP = 1;
        pwm->SHORTS = PWM_SHORTS_LOOPSDONE_SEQSTART0_Msk;

        pwm->ENABLE =
            (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);

        pwm->TASKS_SEQSTART[0] = 1;
    }

    static void stopPwm(
        NRF_PWM_Type *pwm,
        NRF_GPIO_Type *gpio,
        uint32_t pin,
        bool *active
    ) {
        pwm->TASKS_STOP = 1;
        pwm->SHORTS = 0;
        pwm->ENABLE = 0;
        gpio->OUTCLR = (1UL << pin);
        *active = false;
    }
#endif

    //%
    void setTimbreNative(int timbre) {
#if MICROBIT_CODAL
        switch (timbre) {
            case 0: waveformCode = 8; break; // Square 50%
            case 1: waveformCode = 6; break; // Pulse 37.5%
            case 2: waveformCode = 4; break; // Pulse 25%
            case 3: waveformCode = 2; break; // Pulse 12.5%
            case 4: waveformCode = 1; break;  // Pulse 6.25%
            case 5: waveformCode = 15; break; // Wide pulse 93.75%
            case 6: waveformCode = 16; break; // Double pulse
            case 7: waveformCode = 17; break; // Triple pulse
            default: waveformCode = 8; break;
        }
#endif
    }

    //%
    void setVolumeNative(int volume) {
#if MICROBIT_CODAL
        if (volume < 1)
            volume = 1;
        if (volume > 100)
            volume = 100;

        volumePercent = (uint8_t)volume;
        updateActiveSequences();
#endif
    }

    //%
    void tone1(int frequency) {
#if MICROBIT_CODAL
        startPwm(NRF_PWM0, NRF_P0, 0, 12, seq0, &top0, &shape0, &active0, frequency);
#endif
    }

    //%
    void tone2(int frequency) {
#if MICROBIT_CODAL
        startPwm(NRF_PWM1, NRF_P0, 0, 1, seq1, &top1, &shape1, &active1, frequency);
#endif
    }

    //%
    void tone3(int frequency) {
#if MICROBIT_CODAL
        startPwm(NRF_PWM2, NRF_P1, 1, 2, seq2, &top2, &shape2, &active2, frequency);
#endif
    }

    //%
    void stop1() {
#if MICROBIT_CODAL
        stopPwm(NRF_PWM0, NRF_P0, 12, &active0);
#endif
    }

    //%
    void stop2() {
#if MICROBIT_CODAL
        stopPwm(NRF_PWM1, NRF_P0, 1, &active1);
#endif
    }

    //%
    void stop3() {
#if MICROBIT_CODAL
        stopPwm(NRF_PWM2, NRF_P1, 2, &active2);
#endif
    }

    //%
    void stopAll() {
        stop1();
        stop2();
        stop3();
    }
}
