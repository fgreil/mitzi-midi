#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <furi_hal.h> // Hardware abstraction layer
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <gui/elements.h> // Button drawing functions
#include "midi_icons.h" // Custom icon definitions

#define TAG "Mitzi_Midi"
#define MAX_MIDI_MESSAGES 8 // Number of MIDI messages to display in history

// MIDI message types (status bytes)
typedef enum {
    MidiNoteOff = 0x80,          // Note Off
    MidiNoteOn = 0x90,           // Note On
    MidiPolyAftertouch = 0xA0,   // Polyphonic Key Pressure
    MidiControlChange = 0xB0,     // Control Change
    MidiProgramChange = 0xC0,     // Program Change
    MidiChannelAftertouch = 0xD0, // Channel Pressure
    MidiPitchBend = 0xE0,         // Pitch Bend
    MidiSystemMessage = 0xF0      // System messages
} MidiMessageType;

// Structure to store a parsed MIDI message
typedef struct {
    uint8_t status;      // Status byte (includes channel)
    uint8_t data1;       // First data byte
    uint8_t data2;       // Second data byte (if applicable)
    uint8_t channel;     // MIDI channel (0-15)
    MidiMessageType type; // Message type
    uint32_t timestamp;  // Time received (in ticks)
} MidiMessage;

// Application state
typedef struct {
    MidiMessage messages[MAX_MIDI_MESSAGES]; // Ring buffer of received messages
    uint8_t message_count;                   // Total messages received
    uint8_t display_offset;                  // Scroll offset for display
    bool usb_connected;                      // USB connection status
    uint32_t last_message_time;              // Timestamp of last message
} MidiState;

// Event types for the application
typedef enum {
    EventTypeKey,        // User input event
    EventTypeMidi,       // MIDI data received
    EventTypeUsbStatus   // USB connection status change
} EventType;

// Application event structure
typedef struct {
    EventType type;
    union {
        InputEvent input;      // For keyboard events
        MidiMessage midi;      // For MIDI events
        bool usb_connected;    // For USB status events
    };
} MidiEvent;

// Main application context
typedef struct {
    MidiState* state;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
} MidiApp;

// Parse MIDI status byte to extract message type and channel
static void parse_midi_status(uint8_t status, MidiMessageType* type, uint8_t* channel) {
    if(status < 0xF0) {
        // Channel messages (0x80-0xEF)
        *type = status & 0xF0;  // Upper nibble = message type
        *channel = status & 0x0F; // Lower nibble = channel (0-15)
    } else {
        // System messages (0xF0-0xFF)
        *type = MidiSystemMessage;
        *channel = 0; // System messages don't have channels
    }
}

// Add a MIDI message to the ring buffer
static void add_midi_message(MidiState* state, const MidiMessage* message) {
    // Shift existing messages down
    if(state->message_count < MAX_MIDI_MESSAGES) {
        state->message_count++;
    } else {
        // Ring buffer is full, shift all messages
        for(uint8_t i = MAX_MIDI_MESSAGES - 1; i > 0; i--) {
            state->messages[i] = state->messages[i - 1];
        }
    }
    
    // Add new message at the top
    state->messages[0] = *message;
    state->last_message_time = furi_get_tick();
}

// Convert MIDI note number to string representation (e.g., C4, A#5)
static void midi_note_to_string(uint8_t note, char* buffer, size_t size) {
    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint8_t octave = (note / 12) - 1;
    uint8_t note_index = note % 12;
    snprintf(buffer, size, "%s%d", note_names[note_index], octave);
}

// Format MIDI message for display
static void format_midi_message(const MidiMessage* msg, char* buffer, size_t size) {
    char note_str[8];
    
    switch(msg->type) {
    case MidiNoteOn:
        if(msg->data2 > 0) {
            midi_note_to_string(msg->data1, note_str, sizeof(note_str));
            snprintf(buffer, size, "NoteOn  Ch%02d %s Vel%03d", 
                    msg->channel + 1, note_str, msg->data2);
        } else {
            // Note On with velocity 0 is treated as Note Off
            midi_note_to_string(msg->data1, note_str, sizeof(note_str));
            snprintf(buffer, size, "NoteOff Ch%02d %s", 
                    msg->channel + 1, note_str);
        }
        break;
        
    case MidiNoteOff:
        midi_note_to_string(msg->data1, note_str, sizeof(note_str));
        snprintf(buffer, size, "NoteOff Ch%02d %s Vel%03d", 
                msg->channel + 1, note_str, msg->data2);
        break;
        
    case MidiControlChange:
        snprintf(buffer, size, "CC      Ch%02d #%03d=%03d", 
                msg->channel + 1, msg->data1, msg->data2);
        break;
        
    case MidiProgramChange:
        snprintf(buffer, size, "ProgChg Ch%02d Prg%03d", 
                msg->channel + 1, msg->data1);
        break;
        
    case MidiPitchBend:
        {
            int16_t bend = ((msg->data2 << 7) | msg->data1) - 8192;
            snprintf(buffer, size, "PitchBd Ch%02d %+05d", 
                    msg->channel + 1, bend);
        }
        break;
        
    case MidiChannelAftertouch:
        snprintf(buffer, size, "ChPress Ch%02d Val%03d", 
                msg->channel + 1, msg->data1);
        break;
        
    case MidiPolyAftertouch:
        midi_note_to_string(msg->data1, note_str, sizeof(note_str));
        snprintf(buffer, size, "PolyAT  Ch%02d %s P%03d", 
                msg->channel + 1, note_str, msg->data2);
        break;
        
    case MidiSystemMessage:
        snprintf(buffer, size, "System  0x%02X", msg->status);
        break;
        
    default:
        snprintf(buffer, size, "Unknown 0x%02X", msg->status);
        break;
    }
}

// Render callback for GUI - draws the interface
static void render_callback(Canvas* canvas, void* ctx) {
    MidiApp* app = ctx;
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
    
    // Draw USB connection status
    canvas_set_font(canvas, FontSecondary);
    if(app->state->usb_connected) {
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "USB: Connected");
    } else {
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "USB: Waiting...");
    }
    
    // Draw MIDI message history
    canvas_set_font(canvas, FontKeyboard);
    uint8_t y = 22;
    char msg_buffer[32];
    
    uint8_t messages_to_show = (app->state->message_count < 4) ? 
                               app->state->message_count : 4;
    
    for(uint8_t i = 0; i < messages_to_show; i++) {
        uint8_t msg_index = i + app->state->display_offset;
        if(msg_index < app->state->message_count) {
            format_midi_message(&app->state->messages[msg_index], msg_buffer, sizeof(msg_buffer));
            canvas_draw_str(canvas, 1, y, msg_buffer);
            y += 9;
        }
    }
    
    // If no messages yet, show helpful text
    if(app->state->message_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Waiting for MIDI...");
    }
    
    // Draw footer with instructions
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "Back: exit");
    canvas_draw_str_aligned(canvas, 127, 57, AlignRight, AlignTop, "f418.eu");
    
    // Show scroll indicators if there are more messages
    if(app->state->message_count > 4) {
        if(app->state->display_offset > 0) {
            elements_button_top(canvas, "↑");
        }
        if(app->state->display_offset + 4 < app->state->message_count) {
            elements_button_bottom(canvas, "↓");
        }
    }
    
    furi_mutex_release(app->mutex);
}

// Input callback - queues input events for processing
static void input_callback(InputEvent* input_event, void* ctx) {
    MidiApp* app = ctx;
    MidiEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// USB MIDI receive callback (placeholder - needs USB HAL integration)
// This would be called by USB interrupt handler when MIDI data arrives
static void usb_midi_rx_callback(uint8_t* data, size_t length, void* ctx) {
    MidiApp* app = ctx;
    
    // USB MIDI packets are 4 bytes: [Cable/CIN][Status][Data1][Data2]
    // CIN = Code Index Number (upper nibble of byte 0)
    // Cable = Virtual cable number (lower nibble of byte 0)
    
    for(size_t i = 0; i + 3 < length; i += 4) {
        uint8_t cin = (data[i] >> 4) & 0x0F;
        uint8_t status = data[i + 1];
        uint8_t data1 = data[i + 2];
        uint8_t data2 = data[i + 3];
        
        // Skip if no valid MIDI message (CIN == 0)
        if(cin == 0) continue;
        
        MidiMessage msg = {0};
        msg.status = status;
        msg.data1 = data1;
        msg.data2 = data2;
        msg.timestamp = furi_get_tick();
        
        parse_midi_status(status, &msg.type, &msg.channel);
        
        // Queue the MIDI event
        MidiEvent event = {.type = EventTypeMidi, .midi = msg};
        furi_message_queue_put(app->event_queue, &event, 0);
        
        FURI_LOG_D(TAG, "MIDI: CIN=%X Status=%02X Data=%02X %02X", 
                  cin, status, data1, data2);
    }
}

// Initialize USB MIDI interface
static bool init_usb_midi(MidiApp* app) {
    // TODO: Initialize USB MIDI class device
    // This requires integration with Flipper's USB HAL
    // For now, return false to indicate USB not yet implemented
    
    FURI_LOG_W(TAG, "USB MIDI HAL integration not yet implemented");
    FURI_LOG_I(TAG, "To implement: Set up USB device with MIDI class");
    FURI_LOG_I(TAG, "  - Configure USB endpoints for MIDI");
    FURI_LOG_I(TAG, "  - Register RX callback: usb_midi_rx_callback()");
    FURI_LOG_I(TAG, "  - Handle USB connect/disconnect events");
    
    return false;
}

// Deinitialize USB MIDI interface
static void deinit_usb_midi(void) {
    // TODO: Clean up USB MIDI resources
    FURI_LOG_I(TAG, "USB MIDI cleanup");
}

// Main application entry point
int32_t midi_main(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "USB MIDI capturing app starting...");
    
    // Allocate app structure and resources
    MidiApp* app = malloc(sizeof(MidiApp));
    app->state = malloc(sizeof(MidiState));
    memset(app->state, 0, sizeof(MidiState));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(16, sizeof(MidiEvent));
    
    // Initialize USB MIDI
    app->state->usb_connected = init_usb_midi(app);
    
    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    
    FURI_LOG_I(TAG, "GUI initialized, entering main loop");
    
    // Main event loop
    MidiEvent event;
    bool running = true;
    
    while(running) {
        // Wait for events with 100ms timeout
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            
            switch(event.type) {
            case EventTypeKey:
                if(event.input.type == InputTypePress || event.input.type == InputTypeRepeat) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        // Scroll up through message history
                        if(app->state->display_offset > 0) {
                            app->state->display_offset--;
                        }
                        break;
                        
                    case InputKeyDown:
                        // Scroll down through message history
                        if(app->state->display_offset + 4 < app->state->message_count) {
                            app->state->display_offset++;
                        }
                        break;
                        
                    case InputKeyOk:
                        // Clear message history
                        FURI_LOG_I(TAG, "Clearing MIDI message history");
                        app->state->message_count = 0;
                        app->state->display_offset = 0;
                        break;
                        
                    case InputKeyBack:
                        // Exit the application
                        FURI_LOG_I(TAG, "Exit requested");
                        running = false;
                        break;
                        
                    default:
                        break;
                    }
                }
                break;
                
            case EventTypeMidi:
                // New MIDI message received
                add_midi_message(app->state, &event.midi);
                FURI_LOG_I(TAG, "MIDI message: Type=0x%02X Ch=%d D1=%d D2=%d",
                          event.midi.type, event.midi.channel, 
                          event.midi.data1, event.midi.data2);
                break;
                
            case EventTypeUsbStatus:
                // USB connection status changed
                app->state->usb_connected = event.usb_connected;
                FURI_LOG_I(TAG, "USB status: %s", 
                          event.usb_connected ? "Connected" : "Disconnected");
                break;
            }
            
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }
    
    FURI_LOG_I(TAG, "Cleaning up...");
    
    // Cleanup USB
    deinit_usb_midi();
    
    // Cleanup GUI and resources
    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app->state);
    free(app);
    
    FURI_LOG_I(TAG, "USB MIDI app stopped");
    return 0;
}
