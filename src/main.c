#include <pebble.h>
#include "newrelic_layer.h"
#include "clock_layer.h"


/** Our primary UI window. */
static Window *window;

/** Container layers for sub-modules. */
static Layer *newrelic_layer;
static Layer *clock_layer;

/** 
 * A Pebble AppMessageInboxReceived (incoming App Message) handler, which acts 
 * as a dispatcher for all incoming App Messages. (Pebble only lets you have 
 * one registered handler at a time.)
 *
 * @param iter Dictionary iterator containing message key-value pairs.
 * @param context Application data as specified when registering the callback.
 *        Unused.
 */
static void app_msg_in_received_handler(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Received App Message from phone.");
  newrelic_app_msg_in_received_handler(iter, context);
}

/**
 * A Pebble AppMessageInboxDropped (dropped inbound App Message) handler.
 *
 * @param reason Why the message was dropped (error code).
 * @param context Application data as specified when registering the callback. 
 *        Unused.
 */
static void app_msg_in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "App Message dropped! Reason: %d", reason);
}

/**
 * A Pebble AppMessageOutboxFailed (failed to send App Message) handler.
 *
 * @param failed The message that failed to send.
 * @param reason Why the message failed to send (error code).
 * @param context Application data as specified when registering the callback.
 *        Unused.
 */
static void app_msg_out_failed_handler(DictionaryIterator *failed, 
    AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "App Message failed to send! Reason: %d", reason);
}

/**
 * Performs Pebble App Message initialization. Registers handlers, etc.
 */
static void app_message_init(void) {
  // Register message handlers:
  app_message_register_inbox_received(app_msg_in_received_handler);
  app_message_register_inbox_dropped(app_msg_in_dropped_handler);
  app_message_register_outbox_failed(app_msg_out_failed_handler);

  // Init buffers:
  app_message_open(NEWRELIC_DISPLAY_FIELD_SIZE, NEWRELIC_DISPLAY_FIELD_SIZE);
}

/**
 * A Pebble BluetoothConnectionHandler (BT connection state change callback).
 * Triggers refreshes when the watch reconnects to the phone.
 *
 * @param connected True on BT connection, false on disconnection.
 */
static void handle_bluetooth_connection_change(bool connected) {
  // Our data is likely stale after a BT disconnection, and this should not be
  // a common event.
  if (connected) request_newrelic_update();
}

/**
 * A Pebble TickHandler to receive time change events. Acts as a dispatcher, 
 * since only one handler can be registered at a time.
 *
 * @param tick_time The time at which the tick event was triggered.
 * @param units_chanegd Which unit change triggered this tick event.
 */
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  clock_layer_handle_minute_tick(tick_time, units_changed);
}

/**
 * A Pebble WindowHandlers load callback to initially prepare our main UI
 * window. Creates sub-layers for different display components of the app.
 * The companion destructor is window_unload.
 *
 * @param window The Window being loaded.
 */
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Parent layer for the clock (time and date):
  clock_layer = layer_create((GRect) {
      .origin = { 0, 0 },
      .size = { bounds.size.w, 84 },
      });
  layer_add_child(window_layer, clock_layer);
  clock_layer_init(clock_layer);
  
  // Parent layer for New Relic stuff:
  newrelic_layer = layer_create((GRect) {
      .origin = { 0, 84 },
      .size = { bounds.size.w, 84 },
      });
  layer_add_child(window_layer, newrelic_layer);
  newrelic_layer_init(newrelic_layer);
}

/**
 * A Pebble WindowHandlers unload callback to free resources for unused 
 * windows. Destroys resources created by window_load.
 *
 * @param window The Window being unloaded.
 */
static void window_unload(Window *window) {
  newrelic_layer_deinit();
  layer_destroy(newrelic_layer);

  clock_layer_deinit();
  layer_destroy(clock_layer);
}

/**
 * The main app initializer that starts the creation of Windows and subscribes 
 * to Pebble Event Services. The companion destructor is deinit.
 */
static void init(void) {
  window = window_create();
  app_message_init();
  window_set_window_handlers(window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
      });
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  const bool animated = true;
  window_set_background_color(window, GColorBlack);
  window_stack_push(window, animated);
  bluetooth_connection_service_subscribe(handle_bluetooth_connection_change);
}

/**
 * The main app deinitializer. Destroys resources created by init.
 */
static void deinit(void) {
  window_destroy(window);
}

/**
 * The main entry point for the app, called by the Pebble OS.
 *
 * @return An exit code.
 */
int main(void) {
  init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}
