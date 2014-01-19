New Relic Watchface for Pebble
==============================

<img src="http://chrisregado.github.io/newrelic-watch/screenshots/watchface.png" alt="Example watchface"/>

The New Relic Watchface is an app for the [Pebble smartwatch](http://getpebble.com) 
that puts key performance metrics from your New Relic-monitored app on your 
wrist. It will automatically refresh at a regular interval to keep the info 
up to date. Oh, and it tells you what time it is.

This watchface requires at least Pebble app 2.0 on your phone and Pebble OS 2.0
on your watch.


Getting Started
---------------

For this app to do anything useful, you'll need your New Relic API key from the 
New Relic website. To get your key:

1. Sign into [NewRelic.com](https://newrelic.com).

2. Select your account menu (top-right corner) --> Account settings --> 
Integrations tab --> Data sharing --> API access.

3. Click Enable API Access.

4. Note your API key.

To set up the watch app:

1. Download the New Relic Watchface from the Pebble Appstore. If you have an
Android phone or an iOS Pebble app developer build, you can also download the 
[latest PBW release](https://github.com/ChrisRegado/newrelic-watch/releases/latest)
on your phone and open it in the Pebble app to install it.

2. Open the New Relic Watchface settings in the Pebble app on your phone.

  <img src="http://chrisregado.github.io/newrelic-watch/screenshots/config_blank.png" alt="Empty config page" width="320" height="365"/>

3. Enter your New Relic API key and select the web app you'd like to monitor.
You can also set how often your watch should get the latest data from New Relic.

  <img src="http://chrisregado.github.io/newrelic-watch/screenshots/config_complete.png" alt="Complete config page" width="320" height="340"/>

4. Save and enjoy!


Building
--------
This is a standard Pebble watch app. It makes use of the PebbleKit JavaScript
Framework to provide a config page and query the New Relic API, so you'll need
[Pebble SDK 2.0](https://developer.getpebble.com/2/getting-started/) or newer.

Grab the code and run the usual:
````pebble build````


Contributing
------------

Feel free to contribute a feature or bug fix by opening a pull request.
If you discover any problems or have any suggestions, please open an issue.


Credits
-------

Clock modeled after dabdemon's 
[*Yahoo! Weather* watchface](http://www.mypebblefaces.com/apps/9518/7807/).

Neither this app nor its creator are affiliated with or endorsed by New 
Relic, Inc. The New Relic name and logo are the exclusive property of New 
Relic, Inc. See [newrelic.com](http://newrelic.com).
