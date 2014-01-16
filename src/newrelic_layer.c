#include <pebble.h>
#include "newrelic_layer.h"


/** Child layers for the New Relic display. */
static TextLayer *newrelic_app_name_text_layer, *newrelic_labels_text_layer,
                 *newrelic_data_text_layer, *last_update_text_layer;

// Docs are in the header file.
void request_newrelic_update(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Requesting New Relic data update from phone.");
  Tuplet update_req = TupletInteger(UPDATE_REQ_KEY, 1);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to request New Relic update from phone!");
    return;
  }
  dict_write_tuplet(iter, &update_req);
  dict_write_end(iter);
  app_message_outbox_send();
}

/**
 * Updates the "last update" display timestamp to the current time.
 */
static void set_last_update_to_now(void) {
  static char last_update_display_data[] = "xx:xx am";
  clock_copy_time_string(last_update_display_data, sizeof(last_update_display_data));
  text_layer_set_text(last_update_text_layer, last_update_display_data);
}

/**
 * Outputs New Relic data to the display. 
 *
 * @param iter A DictionaryIterator for New Relic data to display, where keys 
 *        are defined by the AppMessageKey enum.
 */
static void display_newrelic_data(DictionaryIterator *iter) {
  // Pebble requires static allocation for any text we want to put on-screen.
  // To enable updates of one field at a time, we'll also store the previously 
  // set value for each:
  static char app_response_time[NEWRELIC_VALUE_FIELD_SIZE] = "";
  static int32_t app_throughput = 0;  // no uints in PebbleKit JS
  static char app_error_rate[NEWRELIC_VALUE_FIELD_SIZE] = "";
  // The final string to render:
  static char final_display_data[NEWRELIC_DISPLAY_FIELD_SIZE];

  // Try to extract all possible fields from the dict:
  Tuple *app_response_time_tuple = dict_find(iter, APP_RESPONSE_TIME_KEY);
  Tuple *app_throughput_tuple = dict_find(iter, APP_THROUGHPUT_KEY);
  Tuple *app_error_rate_tuple = dict_find(iter, APP_ERROR_RATE_KEY);

  // Grab all provided values:
  if (app_response_time_tuple) strncpy(app_response_time, 
      app_response_time_tuple->value->cstring, sizeof(app_response_time) - 1);
  if (app_throughput_tuple) app_throughput = app_throughput_tuple->value->int32;
  if (app_error_rate_tuple) strncpy(app_error_rate, 
      app_error_rate_tuple->value->cstring, sizeof(app_error_rate) - 1);

  // Put the data on-screen:
  snprintf(final_display_data, sizeof(final_display_data), "%sms\n%d\n%s%%",
      app_response_time, (int) app_throughput, app_error_rate);
  text_layer_set_text(newrelic_data_text_layer, final_display_data);

  set_last_update_to_now();

  APP_LOG(APP_LOG_LEVEL_INFO, "Updated New Relic data display to:\n%s", 
      final_display_data);
}

/**
 * Outputs the New Relic app name we're monitoring to the display.
 *
 * @param iter A DictionaryIterator containing the app name (as per the 
 *        AppMessageKey enum).
 */
static void display_newrelic_app_name(DictionaryIterator *iter) {
  static char app_name[NEWRELIC_VALUE_FIELD_SIZE] = "";
  Tuple *app_name_tuple = dict_find(iter, APP_NAME_KEY);
  if (app_name_tuple) strncpy(app_name, app_name_tuple->value->cstring,
      sizeof(app_name) - 1);
  text_layer_set_text(newrelic_app_name_text_layer, app_name);
  APP_LOG(APP_LOG_LEVEL_INFO, "Updated New Relic app name display to: %s", 
      app_name);
}

/**
 * A Pebble AppTimerCallback that triggers a New Relic data update and
 * reschedules the timer.
 *
 * @param mins Schedule another update in this many minutes. (Value gets set 
 *        when the timer for this callback is registered.)
 */
static void newrelic_update_timer_handler(void *mins) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, 
      "New Relic update timer fired. Rescheduling for %d minutes.", (int) mins);
  request_newrelic_update();
  set_newrelic_update_interval((uint32_t) mins);
}

// Docs are in the header file.
void set_newrelic_update_interval(uint32_t mins) {
  static AppTimer *newrelic_update_timer = NULL;
  // static to simplify the issue of user config updates requiring loop resets
  if (newrelic_update_timer != NULL) {
    app_timer_cancel(newrelic_update_timer);
  }
  newrelic_update_timer = app_timer_register(mins * 60000, 
      newrelic_update_timer_handler, (void *) mins);
}

// Docs are in the header file.
void newrelic_app_msg_in_received_handler(DictionaryIterator *iter, 
    void *context) {
  // All possible incoming message keys:
  Tuple *app_name_tuple = dict_find(iter, APP_NAME_KEY);
  Tuple *app_response_time_tuple = dict_find(iter, APP_RESPONSE_TIME_KEY);
  Tuple *app_throughput_tuple = dict_find(iter, APP_THROUGHPUT_KEY);
  Tuple *app_error_rate_tuple = dict_find(iter, APP_ERROR_RATE_KEY);
  Tuple *update_freq_tuple = dict_find(iter, UPDATE_FREQ_KEY);

  // Dispatch:
  
  if (app_name_tuple) {
    display_newrelic_app_name(iter);
  }

  if (app_response_time_tuple || app_throughput_tuple || app_error_rate_tuple) {
    display_newrelic_data(iter);
  }

  if (update_freq_tuple) {
    int signed_mins = update_freq_tuple->value->int32;
    if (signed_mins < 1) {
      APP_LOG(APP_LOG_LEVEL_ERROR, 
          "Tried to set update frequency to an invalid value (%d)!", signed_mins);
    } else {
      uint32_t mins = (uint32_t) signed_mins;
      set_newrelic_update_interval(mins);
      APP_LOG(APP_LOG_LEVEL_INFO, "Update frequency now set to %d minutes.", 
          (int) mins);
    }
  }

}

// Docs are in the header file.
void newrelic_layer_init(Layer *parent_layer) {
  GRect bounds = layer_get_bounds(parent_layer);
  
  // The first line contains the app name, which is in its own layer so we can
  // easily truncate it (because app names can get fairly lengthy).
  newrelic_app_name_text_layer = text_layer_create((GRect) { 
      .origin = { 0, 5 },
      .size = { bounds.size.w, 20 },
      });
  text_layer_set_text(newrelic_app_name_text_layer, "Loading...");
  text_layer_set_text_alignment(newrelic_app_name_text_layer, 
      GTextAlignmentCenter);
  text_layer_set_font(newrelic_app_name_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_16)));
  text_layer_set_background_color(newrelic_app_name_text_layer, GColorBlack);
  text_layer_set_overflow_mode(newrelic_app_name_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(newrelic_app_name_text_layer, GColorWhite);
  layer_add_child(parent_layer, 
      text_layer_get_layer(newrelic_app_name_text_layer));
  
  // The labels layer identifies what the New Relic numbers are:
  newrelic_labels_text_layer = text_layer_create((GRect) { 
      .origin = { 0, 23 },
      .size = { 40, 48 },
      });
  text_layer_set_text(newrelic_labels_text_layer, "RT:\nRPM:\nErr:");
  text_layer_set_text_alignment(newrelic_labels_text_layer, GTextAlignmentRight);
  text_layer_set_font(newrelic_labels_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_16)));
  text_layer_set_background_color(newrelic_labels_text_layer, GColorClear);
  text_layer_set_overflow_mode(newrelic_labels_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(newrelic_labels_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(newrelic_labels_text_layer));

  // The main New Relic layer contains all of the New Relic metrics:
  newrelic_data_text_layer = text_layer_create((GRect) { 
      .origin = { 45, 23 },
      .size = { bounds.size.w - 45, 48 },
      });
  text_layer_set_text_alignment(newrelic_data_text_layer, GTextAlignmentLeft);
  text_layer_set_font(newrelic_data_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_16)));
  text_layer_set_background_color(newrelic_data_text_layer, GColorClear);
  text_layer_set_overflow_mode(newrelic_data_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(newrelic_data_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(newrelic_data_text_layer));
  
  // To display a "last update" timestamp, we create a child layer superimposed 
  // on the main New Relic layer, right at the bottom. Our main layer text 
  // arrangement leaves space for it.
  last_update_text_layer = text_layer_create((GRect) { 
      .origin = { 0, bounds.size.h - 14 },
      .size = { bounds.size.w, 14 },
      });
  text_layer_set_text_alignment(last_update_text_layer, GTextAlignmentRight);
  text_layer_set_font(last_update_text_layer, 
      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(last_update_text_layer, GColorClear);
  text_layer_set_text_color(last_update_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(last_update_text_layer));

  request_newrelic_update();    // our first data fetch
}

// Docs are in the header file.
void newrelic_layer_deinit(void) {
  text_layer_destroy(newrelic_labels_text_layer);
  text_layer_destroy(newrelic_data_text_layer);
  text_layer_destroy(last_update_text_layer);
}
