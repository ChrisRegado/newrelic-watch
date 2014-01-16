/**
 * @section DESCRIPTION
 *
 * This module handles all activities related to displaying New Relic data
 * and scheduling regular data refreshes. Data is fetched on the phone (using
 * PebbleKit JS) and sent to the watch via Pebble App Message.
 */

#ifndef __NEWRELIC_LAYER_H__
#define __NEWRELIC_LAYER_H__

#include <pebble.h>


/** Max string length of a New Relic metric value. */
#define NEWRELIC_VALUE_FIELD_SIZE 24

/** Max length of the final New Relic text printed to the screen. */
#define NEWRELIC_DISPLAY_FIELD_SIZE (NEWRELIC_VALUE_FIELD_SIZE * 3 + 10)
// We have 3 data fields, and the +10 is for labels/spacing.

/**
 * These are the key mappings for KV pairs passed from JS by App Message.
 * They must be kept in sync with the phone JS side via appinfo.json.
 */
enum AppMessageKey {
  // Key:                     // Value:
  UPDATE_REQ_KEY = 0,         // int32 - non-0 is a request msg for new data
  APP_NAME_KEY = 1,           // cstring - New Relic app name we're monitoring
  APP_RESPONSE_TIME_KEY = 2,  // cstring - New Relic app response time
                              //           a string since Pebble has no floats
  APP_THROUGHPUT_KEY = 3,     // int32 - New Relic app RPM 
                              //         (can't send uints from JS App Messages)
  APP_ERROR_RATE_KEY = 4,     // int32 - New Relic app error rate (%)
  UPDATE_FREQ_KEY = 5,        // int32 - Instruction to fetch new New Relic 
                              //         data every this many minutes
};

/**
 * Sends an App Message to the phone to request an update of New Relic data.
 */
void request_newrelic_update(void);

/**
 * Schedule a New Relic data poll on a loop with the given frequency.
 * Repeated invocations overwrite previous timers.
 *
 * @param mins Update New Relic data every this many minutes.
 */
void set_newrelic_update_interval(uint32_t mins);

/**
 * A Pebble AppMessageInboxReceived (incoming App Message) handler that
 * processes New Relic updates from the phone and updates the watch display.
 *
 * @param iter Dictionary iterator containing message key-value pairs. Keys are
 *        defined in the AppMessageKey enum.
 * @param context Application data as specified when registering the callback.
 *        Unused.
 */
void newrelic_app_msg_in_received_handler(DictionaryIterator *iter, void *context);

/**
 * Must be called to initialize the New Relic display layer before any other
 * use of this module. We expect the main app initializer to create a layer for
 * this module to reside in, and then call this to start up the New Relic
 * display. The companion destructor is newrelic_layer_deinit.
 *
 * @param parent_layer The Layer to insert the New Relic display into.
 */
void newrelic_layer_init(Layer *parent_layer);

/**
 * Must be called to destroy the data allocated by newrelic_layer_init.
 */
void newrelic_layer_deinit(void);


#endif  // __NEWRELIC_LAYER_H__
