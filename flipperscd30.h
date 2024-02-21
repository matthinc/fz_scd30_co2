#pragma once

#include <furi_hal_i2c.h>
#include <furi.h>
#include <furi_hal.h>
#include <string>
#include <vector>

struct SCD30Data {
    float co2_ppm;
    float temperature;
    float humidity;

    std::string ts;

    bool result_valid;
};

class FlipperSCD30 {
public:
    FlipperSCD30() = default;
    virtual ~FlipperSCD30() = default;

    bool send_command(std::vector<uint8_t> cmd);
    bool send_command(std::vector<uint8_t> cmd, std::vector<uint8_t> data);
    bool send_command_and_read(std::vector<uint8_t> cmd, uint8_t* result, int len);

    bool start_measurement();
    bool set_interval(uint16_t interval);
    bool calibrate(uint16_t calibration);

    SCD30Data read_measurements();
};

class FlipperSCD30WorkerThread {
public:
    explicit FlipperSCD30WorkerThread(int interval);
    virtual ~FlipperSCD30WorkerThread();

    void stop();
    void start();

    void calibrate_to(uint16_t ppm);

    SCD30Data get_data();
    bool has_data();

    int interval;
    uint16_t next_calibration = 0;
    bool running = false;
    bool data_available = false;
    FuriThread* thread;
    FlipperSCD30 scd30;
    SCD30Data last_data;
};