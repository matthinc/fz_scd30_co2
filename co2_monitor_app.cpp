#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <stdlib.h>

#include <string>

#include "flipperscd30.h"
#include "csv_writer.h"

struct CO2Monitor {
    FuriMessageQueue* event_queue;

    SCD30Data data;
    ViewPort* view_port;
    Gui* gui;
    CsvWriter* csv;
    std::string last_data_ts;
};

static void update_led(int co2_level) {
    int r = 0;
    int g = 0;

    if(co2_level < 500) {
        g = 0xFF;
    } else if(co2_level > 2500) {
        r = 0xFF;
    } else {
        r = ((co2_level - 500.0) / 2000.0) * 0xFF;
        g = 0xFF - r;
    }

    furi_hal_light_set(LightRed, r);
    furi_hal_light_set(LightGreen, g);
    furi_hal_light_set(LightBlue, 0);
}

static void progress_bar(Canvas* canvas, int x, int y, int w, int progress, int max) {
    if(progress < 0) {
        progress = 0;
    } else if(progress > max) {
        progress = max;
    }
    canvas_draw_rframe(canvas, x, y, w, 7, 2);
    canvas_draw_rbox(canvas, x + 2, y + 2, (progress / static_cast<double>(max)) * (w - 4), 3, 0);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    CO2Monitor* context = static_cast<CO2Monitor*>(ctx);

    int co2 = static_cast<int>(context->data.co2_ppm);
    int temp = static_cast<int>(context->data.temperature);
    int hum = static_cast<int>(context->data.humidity);

    update_led(co2);

    int width = canvas_width(canvas);
    int center = width / 2;

    canvas_clear(canvas);

    // CO2 Display
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_rframe(canvas, 5, 1, width - 10, 50, 10);
    canvas_draw_str_aligned(canvas, center, 5, AlignCenter, AlignTop, std::to_string(co2).c_str());
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, center, 22, AlignCenter, AlignTop, "ppm CO2");
    progress_bar(canvas, 10, 35, width - 20, co2, 3000);

    // Temp / humidity
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        center,
        55,
        AlignCenter,
        AlignTop,
        (std::to_string(temp) + " C, " + std::to_string(hum) + " %").c_str());
}

static void input_callback(InputEvent* input, void* ctx) {
    CO2Monitor* context = static_cast<CO2Monitor*>(ctx);
    furi_message_queue_put(context->event_queue, input, FuriWaitForever);
}

extern "C" int32_t co2_monitor_app(void* p) {
    UNUSED(p);

    CO2Monitor* co2_monitor = new CO2Monitor();

    co2_monitor->csv = new CsvWriter(APP_DATA_PATH("co2.csv"));

    co2_monitor->view_port = view_port_alloc();
    co2_monitor->gui = static_cast<Gui*>(furi_record_open("gui"));
    co2_monitor->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(co2_monitor->view_port, draw_callback, co2_monitor);
    view_port_input_callback_set(co2_monitor->view_port, input_callback, co2_monitor);

    gui_add_view_port(co2_monitor->gui, co2_monitor->view_port, GuiLayerFullscreen);

    co2_monitor->gui = static_cast<Gui*>(furi_record_open("gui"));
    co2_monitor->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    gui_add_view_port(co2_monitor->gui, co2_monitor->view_port, GuiLayerFullscreen);

    FlipperSCD30WorkerThread scd30_worker(5);
    scd30_worker.start();

    bool running = true;
    InputEvent event;

    while(running) {
        FuriStatus status = furi_message_queue_get(co2_monitor->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }

        if(scd30_worker.has_data()) {
            SCD30Data new_data = scd30_worker.get_data();

            if(co2_monitor->last_data_ts != new_data.ts) {
                co2_monitor->data = new_data;
                co2_monitor->last_data_ts = new_data.ts;

                co2_monitor->csv->add_row({
                    new_data.ts,
                    std::to_string(new_data.co2_ppm),
                    std::to_string(new_data.temperature),
                    std::to_string(new_data.humidity),
                });
            }
        }

        view_port_update(co2_monitor->view_port);
    }

    scd30_worker.stop();

    // Turn off LED
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    gui_remove_view_port(co2_monitor->gui, co2_monitor->view_port);
    view_port_free(co2_monitor->view_port);
    furi_record_close("gui");

    delete co2_monitor->csv;
    delete co2_monitor;
    return 0;
}