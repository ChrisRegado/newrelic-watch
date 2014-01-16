#include <pebble.h>
#include "clock_layer.h"


/** Child layers for the date and time displays. */
static TextLayer *weekday_text_layer, *date_text_layer, *time_text_layer;

// Docs are in the header file.
void clock_layer_handle_minute_tick(struct tm *tick_time, 
    TimeUnits units_changed) {
  // Pebble requires static vars for on-screen text because they're used by 
  // the system later:
  static char time_text[] = "00:00";
  static char weekday_text[16] = "";
  static char date_text[16] = "";

  // Set the date components:
  strftime(weekday_text, sizeof(weekday_text), "%A", tick_time);
  strftime(date_text, sizeof(date_text), "%B %d", tick_time);
  text_layer_set_text(weekday_text_layer, weekday_text);
  text_layer_set_text(date_text_layer, date_text);

  // Set the time component:
  char *time_format;
  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }
  strftime(time_text, sizeof(time_text), time_format, tick_time);
  // Drop the leading 0 for 12 hour clocks before 10:00:
  //if (!clock_is_24h_style() && (time_text[0] == '0')) {
  //  memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  //}
  text_layer_set_text(time_text_layer, time_text);
}

// Docs are in the header file.
void clock_layer_init(Layer *parent_layer) {
  GRect bounds = layer_get_bounds(parent_layer);
  
  // Layer that displays the day of the week:
  weekday_text_layer = text_layer_create((GRect) {
      .origin = { 0, 0 },
      .size = { bounds.size.w, 20 },
      });
  text_layer_set_text_color(weekday_text_layer, GColorWhite);
  text_layer_set_background_color(weekday_text_layer, GColorClear);
  text_layer_set_font(weekday_text_layer, 
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_16)));
  layer_add_child(parent_layer, text_layer_get_layer(weekday_text_layer));

  // Layer that displays the time:
  time_text_layer = text_layer_create((GRect) {
      .origin = { 0, 11 },
      .size = { bounds.size.w, 63 },
      });
  text_layer_set_text_color(time_text_layer, GColorWhite);
  text_layer_set_background_color(time_text_layer, GColorClear);
  text_layer_set_text_alignment(time_text_layer, GTextAlignmentCenter);
  text_layer_set_font(time_text_layer, fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_FUTURA_CONDENSED_BOLD_53)));
  layer_add_child(parent_layer, text_layer_get_layer(time_text_layer));

  // Layer that displays the month/day:
  date_text_layer = text_layer_create((GRect) {
      .origin = { 0, 64 },
      .size = { bounds.size.w, 20 },
      });
  text_layer_set_text_color(date_text_layer, GColorWhite);
  text_layer_set_background_color(date_text_layer, GColorClear);
  text_layer_set_text_alignment(date_text_layer, GTextAlignmentRight);
  text_layer_set_font(date_text_layer, 
      fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_16)));
  layer_add_child(parent_layer, text_layer_get_layer(date_text_layer));
}

// Docs are in the header file.
void clock_layer_deinit(void) {
  text_layer_destroy(weekday_text_layer);
  text_layer_destroy(date_text_layer);
  text_layer_destroy(time_text_layer);
}
