/**
 * @section DESCRIPTION
 *
 * This module displays the date and time on the watch.
 */

#ifndef __CLOCK_LAYER_H__
#define __CLOCK_LAYER_H__

#include <pebble.h>


/**
 * A Pebble TickHandler to receive time change events and update the clock 
 * display. Subscription must have MINUTE_UNIT resolution or finer.
 * Parent must register/dispatch to this handler since each app can only
 * have one active subscription.
 *
 * @param tick_time The time at which the tick event was triggered.
 * @param units_chanegd Which unit change triggered this tick event.
 */
void clock_layer_handle_minute_tick(struct tm *tick_time, 
    TimeUnits units_changed);

/**
 * Must be called to initialize the clock display before any other use of this 
 * module. We expect the main watch app initializer to create a layer for the 
 * clock to reside in, and then call this to start up the clock display. The 
 * companion destructor is clock_layer_deinit.
 *
 * @param parent_layer The Layer to insert the clock display into.
 */
void clock_layer_init(Layer *parent_layer);

/**
 * Must be called to destroy the data allocated by clock_layer_init.
 */
void clock_layer_deinit(void);


#endif // __CLOCK_LAYER_H__
