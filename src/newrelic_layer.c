#include <pebble.h>
#include "newrelic_layer.h"


/** Child layers for the New Relic display. */
static TextLayer *newrelic_app_name_text_layer, *left_data_text_layer, 
                 *right_data_text_layer, *last_update_text_layer;
static TextLayer *error_cover_text_layer;
static Layer *line_layer;

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
 * A Pebble LayerUpdateProc to redraw our metric divider line.
 *
 * @param layer The layer that needs to be rendered.
 * @param ctx The destination graphics context to draw into.
 */
static void line_layer_update_callback(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
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
 * Converts an (unsigned) int into a human-readable string.
 * Example: 1850000 becomes "1.8m".
 *
 * @param num The number to make human readable.
 * @param result Buffer to return the human readable string in.
 * @param result_len Length of the result buffer. Should be at least 13 bytes 
 *        (# digits in int + decimal point + unit + \0).
 */
static void uint_to_human_readable(unsigned int num, char *result, 
    int result_len) {
  const char *units[] = {"", "k", "m", "b"}; // uints cap out in billions

  // No Pebble floats/doubles, so we have to do this via string manipulation.
  char num_str[11] = "";
  snprintf(num_str, sizeof(num_str), "%d", num);
  char *fractional_part = num_str;
  unsigned int integer_part = num;
  unsigned int unit = 0;

  // Reduce the left side of the decimal to <1000:
  for (unit = 0; integer_part >= 1000; unit++) {
    integer_part /= 1000;
  }

  // How many digits do we have in the integer part?
  snprintf(result, result_len, "%d", integer_part);
  if (unit == 0) return;    // just a plain itoa
  unsigned int num_digits_in_integer_part = strlen(result);
  fractional_part = num_str + num_digits_in_integer_part;

  snprintf(result, result_len, "%d.%.1s%s", integer_part, fractional_part,
      units[unit]);
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
  static char human_readable_app_throughput[NEWRELIC_VALUE_FIELD_SIZE] = "";
  static char app_error_rate[NEWRELIC_VALUE_FIELD_SIZE] = "";
  static char app_apdex_score[NEWRELIC_VALUE_FIELD_SIZE] = "";
  // The final strings to render on-screen:
  static char final_left_display_data[NEWRELIC_DISPLAY_FIELD_SIZE / 2];
  static char final_right_display_data[NEWRELIC_DISPLAY_FIELD_SIZE / 2];

  // Try to extract all possible fields from the dict:
  Tuple *app_response_time_tuple = dict_find(iter, APP_RESPONSE_TIME_KEY);
  Tuple *app_throughput_tuple = dict_find(iter, APP_THROUGHPUT_KEY);
  Tuple *app_error_rate_tuple = dict_find(iter, APP_ERROR_RATE_KEY);
  Tuple *app_apdex_score_tuple = dict_find(iter, APP_APDEX_SCORE_KEY);

  // Grab all provided values:
  if (app_response_time_tuple) strncpy(app_response_time, 
      app_response_time_tuple->value->cstring, sizeof(app_response_time) - 1);
  if (app_throughput_tuple) {
    app_throughput = app_throughput_tuple->value->int32;
    uint_to_human_readable(app_throughput, human_readable_app_throughput,
        sizeof(human_readable_app_throughput));
  }
  if (app_error_rate_tuple) strncpy(app_error_rate, 
      app_error_rate_tuple->value->cstring, sizeof(app_error_rate) - 1);
  if (app_apdex_score_tuple) strncpy(app_apdex_score,
      app_apdex_score_tuple->value->cstring, sizeof(app_apdex_score - 1));

  // Put the data on-screen:
  snprintf(final_left_display_data, sizeof(final_left_display_data), 
      "%s\n%sms", human_readable_app_throughput, app_response_time);
  text_layer_set_text(left_data_text_layer, final_left_display_data);
  snprintf(final_right_display_data, sizeof(final_right_display_data), 
      "%sap\n%s%%", app_apdex_score, app_error_rate);
  text_layer_set_text(right_data_text_layer, final_right_display_data);

  set_last_update_to_now();

  layer_set_hidden((Layer *) error_cover_text_layer, true);

  APP_LOG(APP_LOG_LEVEL_INFO, "Updated New Relic data display to:\n%s\n%s", 
      final_left_display_data, final_right_display_data);
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
  Tuple *update_freq_tuple = dict_find(iter, UPDATE_FREQ_KEY);
  Tuple *app_name_tuple = dict_find(iter, APP_NAME_KEY);
  Tuple *app_response_time_tuple = dict_find(iter, APP_RESPONSE_TIME_KEY);
  Tuple *app_throughput_tuple = dict_find(iter, APP_THROUGHPUT_KEY);
  Tuple *app_error_rate_tuple = dict_find(iter, APP_ERROR_RATE_KEY);
  Tuple *app_apdex_score_tuple = dict_find(iter, APP_APDEX_SCORE_KEY);

  // Dispatch:
  
  if (app_name_tuple) {
    display_newrelic_app_name(iter);
  }

  if (app_response_time_tuple || app_throughput_tuple || app_error_rate_tuple 
      || app_apdex_score_tuple) {
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
      .origin = { 0, 7 },
      .size = { bounds.size.w, 20 },
      });
  text_layer_set_text(newrelic_app_name_text_layer, "Loading...");
  text_layer_set_text_alignment(newrelic_app_name_text_layer, 
      GTextAlignmentCenter);
  text_layer_set_font(newrelic_app_name_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SIGNIKA_REGULAR_16)));
  text_layer_set_background_color(newrelic_app_name_text_layer, GColorBlack);
  text_layer_set_overflow_mode(newrelic_app_name_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(newrelic_app_name_text_layer, GColorWhite);
  layer_add_child(parent_layer, 
      text_layer_get_layer(newrelic_app_name_text_layer));
  
  // The main New Relic layer contains all of the New Relic metrics:
  left_data_text_layer = text_layer_create((GRect) { 
      .origin = { 0, 25 },
      .size = { bounds.size.w / 2 - 3, 40 },
      });
  text_layer_set_text_alignment(left_data_text_layer, GTextAlignmentRight);
  text_layer_set_font(left_data_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SIGNIKA_REGULAR_16)));
  text_layer_set_background_color(left_data_text_layer, GColorClear);
  text_layer_set_overflow_mode(left_data_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(left_data_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(left_data_text_layer));
  
  right_data_text_layer = text_layer_create((GRect) { 
      .origin = { bounds.size.w / 2 + 4, 25 },
      .size = { bounds.size.w / 2 - 4, 40 },
      });
  text_layer_set_text_alignment(right_data_text_layer, GTextAlignmentLeft);
  text_layer_set_font(right_data_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SIGNIKA_REGULAR_16)));
  text_layer_set_background_color(right_data_text_layer, GColorClear);
  text_layer_set_overflow_mode(right_data_text_layer, 
      GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(right_data_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(right_data_text_layer));
  
  // Create the line that divides our metrics.
  line_layer = layer_create((GRect) {
      .origin = { bounds.size.w / 2, 31 },
      .size = { 1, 26 },
      });
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(parent_layer, line_layer);

  // To display a "last update" timestamp, we create a child layer superimposed 
  // on the main New Relic layer, right at the bottom. Our main layer text 
  // arrangement leaves space for it.
  last_update_text_layer = text_layer_create((GRect) { 
      .origin = { 0, bounds.size.h - 12 },
      .size = { bounds.size.w, 13 },
      });
  text_layer_set_text_alignment(last_update_text_layer, GTextAlignmentRight);
  text_layer_set_font(last_update_text_layer, fonts_load_custom_font(
        resource_get_handle(RESOURCE_ID_FONT_SIGNIKA_REGULAR_12)));
  text_layer_set_background_color(last_update_text_layer, GColorClear);
  text_layer_set_text_color(last_update_text_layer, GColorWhite);
  layer_add_child(parent_layer, text_layer_get_layer(last_update_text_layer));
  
  // This layer covers the entire New Relic display area to display
  // errors/alerts. Currently only used for first-time loads.
  error_cover_text_layer = text_layer_create((GRect) {
      .origin = { 0, 0 },
      .size = { bounds.size.w, bounds.size.h },
      });
  text_layer_set_text_alignment(error_cover_text_layer, GTextAlignmentCenter);
  text_layer_set_font(error_cover_text_layer,
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SIGNIKA_REGULAR_16)));
  text_layer_set_background_color(error_cover_text_layer, GColorBlack);
  text_layer_set_overflow_mode(error_cover_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_color(error_cover_text_layer, GColorWhite);
  text_layer_set_text(error_cover_text_layer, 
      "Loading...\nIf stuck, check watchface settings & Internet connectivity.");
  layer_add_child(parent_layer, text_layer_get_layer(error_cover_text_layer));
  
  request_newrelic_update();    // our first data fetch
}

// Docs are in the header file.
void newrelic_layer_deinit(void) {
  text_layer_destroy(left_data_text_layer);
  text_layer_destroy(last_update_text_layer);
  layer_destroy(line_layer);
  text_layer_destroy(error_cover_text_layer);
}
