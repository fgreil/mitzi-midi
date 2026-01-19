#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <gui/elements.h> // to access button drawing functions
#include "midik_icons.h" // Custom icon definitions

#define TAG "Mitzi Midi"

// Main application context
typedef struct {
    FartState* state;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
} FartApp;

// Render callback for GUI - draws the interface
static void render_callback(Canvas* canvas, void* ctx) {
    FartApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    
    canvas_clear(canvas);
    
    // Draw header with icon and title
    canvas_set_font(canvas, FontPrimary);
	canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
	canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Mitzi Midi");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.1");
	
    // Draw date rotated 90 degrees on right edge
	canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
	canvas_draw_str(canvas, 128, 47, "2026-01");		
	canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    
    // Draw footer text and button
	canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "Back: exit");
	// canvas_draw_str_aligned(canvas, 127, 50, AlignRight, AlignTop, "f418.eu/");
	canvas_draw_str_aligned(canvas, 127, 57, AlignRight, AlignTop, "f418.eu");
	elements_button_center(canvas, "OK");
	
    char str_buffer[64];
    uint8_t y = 19;
    
  
}

// Input callback - queues input events for processing
static void fart_input_callback(InputEvent* input_event, void* ctx) {
    FartApp* app = ctx;
    FartEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// Main application entry point
int32_t fart_main(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "Midi capturing app starting...");
    
    // Allocate app structure and resources
    FartApp* app = malloc(sizeof(FartApp));
    app->state = malloc(sizeof(FartState));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(FartEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, fart_render_callback, app);
    view_port_input_callback_set(view_port, fart_input_callback, app);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    FURI_LOG_I(TAG, "GUI initialized, entering main loop");
    
    // Main event loop
    MidiEvent event;
    bool running = true;
    
    while(running) {
        // Wait for events with 100ms timeout
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress || event.input.type == InputTypeRepeat) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    
                    switch(event.input.key) {
                    case InputKeyUp:
                        // Navigate up through parameters
                        if(app->state->selected > 0) {
                            app->state->selected--;
                        }
                        break;
                        
                    case InputKeyDown:
                        // Navigate down through parameters
                        if(app->state->selected < 3) {
                            app->state->selected++;
                        }
                        break;
                        
                    case InputKeyRight:
                        // Increase selected parameter value
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0: param = &app->state->wet_dry; break;
                            case 1: param = &app->state->length; break;
                            case 2: param = &app->state->pitch; break;
                            case 3: param = &app->state->bubbliness; break;
                            }
                            if(param && *param < 5) (*param)++;
                        }
                        break;
                        
                    case InputKeyLeft:
                        // Decrease selected parameter value
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0: param = &app->state->wet_dry; break;
                            case 1: param = &app->state->length; break;
                            case 2: param = &app->state->pitch; break;
                            case 3: param = &app->state->bubbliness; break;
                            }
                            if(param && *param > 1) (*param)--;
                        }
                        break;
                        
                    case InputKeyOk:
                        // Play the fart sound (if not already playing)
                        if(!app->state->playing) {
                            FURI_LOG_I(TAG, "Playing fart: W/D=%d L=%d P=%d B=%d",
                                app->state->wet_dry, app->state->length, app->state->pitch,
                                app->state->bubbliness);
                            // Play fart in separate thread to not block UI
                            furi_mutex_release(app->mutex);
                            play_fart(app->state, app->notifications);
                            furi_mutex_acquire(app->mutex, FuriWaitForever);
                        }
                        break;
                        
                    case InputKeyBack:
                        // Exit the application
                        FURI_LOG_I(TAG, "Exit requested");
                        running = false;
                        break;
                        
                    default:
                        break;
                    }
                    
                    furi_mutex_release(app->mutex);
                    view_port_update(view_port);
                }
            }
        }
    }
    
    FURI_LOG_I(TAG, "Cleaning up...");
    
    // Cleanup all resources
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app->state);
    free(app);
 
    return 0;
}
