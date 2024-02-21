#include "flipperscd30.h"

#define UINT16_MSBS(val) static_cast<uint8_t>((val >> 8) & 0xFF)
#define UINT16_LSBS(val) static_cast<uint8_t>(val & 0xFF)

#define WITH_I2C(cmd)                                    \
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external); \
    bool _result = cmd;                                  \
    furi_hal_i2c_release(&furi_hal_i2c_handle_external); \
    return _result;

inline float buffer_to_float(uint8_t* buffer, uint8_t offset) {
    union {
        float f;
        uint8_t c[4];
    };
    for(int i = 0; i < 4; i++) {
        c[i] = buffer[offset + 3 - i];
    }
    return f;
}

// From https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library/blob/main/src/SparkFun_SCD30_Arduino_Library.cpp
static uint8_t crc(std::vector<uint8_t> data) {
    uint8_t crc = 0xFF;
    for(uint32_t x = 0; x < data.size(); x++) {
        crc ^= data[x];
        for(uint32_t i = 0; i < 8; i++) {
            if((crc & 0x80) != 0) {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool FlipperSCD30::send_command(std::vector<uint8_t> cmd) {
    WITH_I2C(
        furi_hal_i2c_tx(&furi_hal_i2c_handle_external, (0x61 << 1), cmd.data(), cmd.size(), 50));
}

bool FlipperSCD30::send_command_and_read(std::vector<uint8_t> cmd, uint8_t* result, int len) {
    WITH_I2C(furi_hal_i2c_trx(
        &furi_hal_i2c_handle_external, (0x61 << 1), cmd.data(), cmd.size(), result, len, 50));
}

bool FlipperSCD30::send_command(std::vector<uint8_t> cmd, std::vector<uint8_t> data) {
    data.push_back(crc(data));
    cmd.insert(cmd.end(), data.begin(), data.end());
    return send_command(cmd);
}

bool FlipperSCD30::start_measurement() {
    return send_command({0x00, 0x10});
}

bool FlipperSCD30::set_interval(uint16_t interval) {
    return send_command({0x46, 0x00}, {UINT16_MSBS(interval), UINT16_LSBS(interval)});
}

bool FlipperSCD30::calibrate(uint16_t calibration) {
    return send_command({0x52, 0x04}, {UINT16_MSBS(calibration), UINT16_LSBS(calibration)});
}

SCD30Data FlipperSCD30::read_measurements() {
    SCD30Data result;
    uint8_t buffer[18];

    result.result_valid = send_command_and_read({0x03, 0x00}, buffer, 18);

    if(result.result_valid) {
        result.co2_ppm = buffer_to_float(buffer, 0);
        result.temperature = buffer_to_float(buffer, 6);
        result.humidity = buffer_to_float(buffer, 12);

        FuriHalRtcDateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);

        result.ts = std::to_string(datetime.year) + "-" + std::to_string(datetime.month) + "-" +
                    std::to_string(datetime.day) + " " + std::to_string(datetime.hour) + ":" +
                    std::to_string(datetime.minute) + ":" + std::to_string(datetime.second);
    }

    return result;
}

static int32_t run(void* context) {
    FlipperSCD30WorkerThread* worker_context =
        reinterpret_cast<FlipperSCD30WorkerThread*>(context);

    worker_context->scd30.start_measurement();
    worker_context->scd30.set_interval(worker_context->interval);

    while(worker_context->running) {
        SCD30Data data = worker_context->scd30.read_measurements();

        if(data.result_valid && data.co2_ppm > 0) {
            worker_context->last_data = data;
            worker_context->data_available = true;
        }

        if(worker_context->next_calibration != 0) {
            worker_context->scd30.calibrate(worker_context->next_calibration);
            worker_context->next_calibration = 0;
        }

        furi_delay_ms(worker_context->interval);
    }

    return 0;
}

FlipperSCD30WorkerThread::FlipperSCD30WorkerThread(int interval)
    : interval(interval) {
    thread = furi_thread_alloc();
    furi_thread_set_name(thread, "SensorWorker");
    furi_thread_set_stack_size(thread, 2048);
    furi_thread_set_context(thread, this);
    furi_thread_set_callback(thread, run);
}

FlipperSCD30WorkerThread::~FlipperSCD30WorkerThread() {
    furi_thread_free(thread);
}

void FlipperSCD30WorkerThread::stop() {
    running = false;
    furi_thread_join(thread);
}

void FlipperSCD30WorkerThread::start() {
    running = true;
    furi_thread_start(thread);
}

void FlipperSCD30WorkerThread::calibrate_to(uint16_t ppm) {
    next_calibration = ppm;
}

bool FlipperSCD30WorkerThread::has_data() {
    return data_available;
}

SCD30Data FlipperSCD30WorkerThread::get_data() {
    return last_data;
}
