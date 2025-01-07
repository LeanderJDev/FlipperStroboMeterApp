/*
My first flipper application
Simple strobometer which can be used to measure the speed of a rotating object.

Parts of the code are taken from
https://github.com/instantiator/flipper-zero-tutorial-app/
https://github.com/flipperdevices/flipperzero-firmware/blob/dev/applications/examples/example_thermo/example_thermo.c

*/

#include <furi.h>

#include <furi_hal_power.h>
#include <furi_hal_pwm.h>

#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>

#define TAG "strobometer"

/* generated by fbt from .png files in images folder */
#include <strobometer_icons.h>

#define STROBOSCOPE_PIN (FuriHalPwmOutputIdLptim2PA4)

/** the app context struct */
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    int frequency; // frequency of the strobometer
    unsigned short places;
    unsigned short selected; // selected place in the frequency (0-(places-1))
    bool output; // output state of the strobometer
} StrobometerAppContext;

/* Draw the GUI of the application. The screen is completely redrawn during each call. */

static void input_box(Canvas* canvas, int x, int y, const char* value, bool selected) {
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, value);
    if(selected) {
        canvas_draw_rframe(canvas, x - 3, y - 3, 16, 20, 3);
        canvas_draw_icon(canvas, x + 2, y - 8, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, x + 2, y + 18, &I_ButtonDown_7x4);
    }
}

static void strobometer_app_draw_callback(Canvas* canvas, void* ctx) {
    StrobometerAppContext* context = ctx;
    char text_store[32];

    // canvas_set_font(canvas, FontPrimary);
    // canvas_draw_str_aligned(canvas, middle_x, 12, AlignCenter, AlignBottom, "Strobometer");
    // canvas_draw_line(canvas, 0, 16, 128, 16);

    // Draw RPM Input View
    for(int i = 0; i < context->places; i++) {
        snprintf(text_store, sizeof(text_store), "%i", context->frequency / (int)pow(10, i) % 10);
        input_box(
            canvas,
            85 - 16 * i,
            canvas_height(canvas) / 2U - 7,
            text_store,
            i == context->selected);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 101, canvas_height(canvas) / 2U, AlignLeft, AlignCenter, "RPM");

    // Draw Output Button
    if(context->output) {
        elements_button_center(canvas, "Stop");
    } else {
        elements_button_center(canvas, "Run");
    }
}

static void strobometer_app_input_callback(InputEvent* event, void* ctx) {
    StrobometerAppContext* context = ctx;
    furi_message_queue_put(context->event_queue, event, FuriWaitForever);
}

static void strobometer_app_run(StrobometerAppContext* context) {
    furi_hal_power_enable_otg();

    view_port_enabled_set(context->view_port, true);

    for(bool is_running = true; is_running;) {
        InputEvent event;

        const FuriStatus status =
            furi_message_queue_get(context->event_queue, &event, FuriWaitForever);

        if(status != FuriStatusOk) {
            continue;
        }

        if(event.type != InputTypeRepeat && event.type != InputTypePress) {
            continue;
        }

        // Change the selected digit

        if(event.key == InputKeyLeft) {
            context->selected = (context->selected + 1) % context->places;
        } else if(event.key == InputKeyRight) {
            context->selected = (context->selected + (context->places - 1)) % context->places;
        }

        if(event.key == InputKeyUp) {
            context->frequency += (int)pow(10, context->selected);
        } else if(event.key == InputKeyDown) {
            context->frequency -= (int)pow(10, context->selected);
        }

        if(context->frequency < 0) {
            context->frequency = pow(10, context->places) - 1;
        } else if(context->frequency > pow(10, context->places) - 1) {
            context->frequency = 0;
        }

        if(event.type != InputTypePress) {
            continue;
        }

        if(event.key == InputKeyBack) {
            is_running = false;
        }

        if(event.key == InputKeyOk) {
            context->output = !context->output;
            if(context->output) {
                furi_hal_pwm_start(STROBOSCOPE_PIN, context->frequency / 60, 10);
            } else {
                furi_hal_pwm_stop(STROBOSCOPE_PIN);
            }
        }

        view_port_update(context->view_port);
    }

    if(furi_hal_pwm_is_running(STROBOSCOPE_PIN)) {
        furi_hal_pwm_stop(STROBOSCOPE_PIN);
    }

    furi_hal_power_disable_otg();
}

static StrobometerAppContext* strobometer_app_context_alloc(void) {
    StrobometerAppContext* context = malloc(sizeof(StrobometerAppContext));

    context->view_port = view_port_alloc();
    view_port_draw_callback_set(context->view_port, strobometer_app_draw_callback, context);
    view_port_input_callback_set(context->view_port, strobometer_app_input_callback, context);

    context->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    context->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(context->gui, context->view_port, GuiLayerFullscreen);

    context->places = 6;

    return context;
}

static void strobometer_app_context_free(StrobometerAppContext* context) {
    view_port_enabled_set(context->view_port, false);
    gui_remove_view_port(context->gui, context->view_port);

    furi_message_queue_free(context->event_queue);
    view_port_free(context->view_port);

    furi_record_close(RECORD_GUI);
}

void strobometer_app_set_log_level() {
#ifdef FURI_DEBUG
    furi_log_set_level(FuriLogLevelTrace);
#else
    furi_log_set_level(FuriLogLevelInfo);
#endif
}

int32_t strobometer_app(void* p) {
    UNUSED(p);
    strobometer_app_set_log_level();

    FURI_LOG_I(TAG, "Test app starting...");

    StrobometerAppContext* context = strobometer_app_context_alloc();

    strobometer_app_run(context);

    strobometer_app_context_free(context);

    return 0;
}