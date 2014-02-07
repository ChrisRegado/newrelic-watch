/** 
 * We use PebbleKit JS running on the phone to do the heavy lifting aspects
 * of user configuration and interacting with the New Relic API.
 *
 * Note: Pebble only allows one phone-side JS file, so everything needs to be 
 * thrown in here.
 */


/************
 * Constants:
 ************/

/** Base URL for the New Relic API. */
NEWRELIC_API_URL = 'https://api.newrelic.com/v2';
/** URL for our configuration page. */
CONFIG_PAGE_URL = 'http://chrisregado.github.io/newrelic-watch/config/v1.0.2/config.html';
/** Maximum amount of time (in ms) to wait for New Relic API responses. */
AJAX_TIMEOUT = 30000;


/***************************
 * Configuration management:
 ***************************/

/**
 * Creates an instance of Options.
 *
 * @class Holds user-supplied configuration data.
 *        Kludge: This class is effectively duplicated for the config page 
 *        since there's currently no convenient way to share code between the 
 *        two components.
 * @this {Options}
 * @param {string} apiKey The user's New Relic API key.
 * @param {string} appId The New Relic app ID (digits only) of the New Relic
 *        app the user wants to monitor.
 * @param {number} updateFreq The frequency (in minutes) with which we should
 *        fetch new New Relic data.
 */
function Options(apiKey, appId, updateFreq) {
  this.apiKey = apiKey;
  this.appId = appId;
  this.updateFreq = updateFreq || Options.DEFAULT_UPDATE_FREQ;
}

/** Default value for the update frequency option in case the user skips it. */
Options.DEFAULT_UPDATE_FREQ = 5;

/**
 * Cleans this Options object's properties.
 *
 * @this {Options}
 */
Options.prototype.sanitize = function() {
  // Clean apiKey:
  if (this.apiKey) {
    this.apiKey = this.apiKey.replace(/[^a-z0-9]/gi, '');
  }

  // Clean appId:
  if (this.appId) {
    this.appId = this.appId.replace(/[^0-9]/g, '');
  }

  // Clean updateFreq:
  var parsedUpdateFreq = parseInt(this.updateFreq);
  if (parsedUpdateFreq && parsedUpdateFreq >= 1) {
    this.updateFreq = parsedUpdateFreq;
  } else {
    this.updateFreq = DEFAULT_UPDATE_FREQ;
  }
}

/**
 * Persists this object to the Pebble app's local storage. Forces sanitization.
 *
 * @this {Options}
 */
Options.prototype.save = function() {
  this.sanitize();
  window.localStorage.setItem('options', JSON.stringify(this));
  console.log('Saved options: ' + JSON.stringify(this));
}

/**
 * Returns an Options object representing the currently persisted options.
 *
 * @return {Options} Currently saved options.
 */
Options.getSavedOptions = function() {
  var serializedOptions = window.localStorage.getItem('options');
  var opts = JSON.parse(serializedOptions);
  if (opts) {
    return Options.fromObject(opts);
  } else {
    return new Options();
  }
}

/**
 * Convert a plain object with attributes into an Options instance.
 *
 * @param {Object} obj An object containing Options attributes.
 * @return {Options} A proper Options instance with the same attributes.
 */
Options.fromObject = function(obj) {
  var options = new Options(
    obj.apiKey,
    obj.appId,
    obj.updateFreq
  );
  return options;
}


/*********************
 * Data communication:
 *********************/

/**
 * Inform the watch how often it should update New Relic data.
 *
 * Pebble doesn't have a great way to get JS-provided config data onto the 
 * watch, so we have to remember to send it at each startup and on changes, 
 * and we have to retry until the watch acks the setting. (All config changes
 * are idempotent.)
 */
function transmitCurrentUpdateFreq() {
  var mins = Options.getSavedOptions().updateFreq;
  if (!mins) return;
  Pebble.sendAppMessage({ 
    'UPDATE_FREQ_KEY': mins,
  },
  function(e) { 
    /** 
     * Called when the watch acks this App Message.
     */
    console.log('Watched acked update freq setting of ' + mins + ' mins.'); 
  },
  function (e) {
    /** 
     * Called when the watch fails to ack this App Message.
     * Retry until the watch acks. We don't have to worry about config value 
     * changes mixing with retries since retries will fetch the latest config
     * values.
     */
    console.log('Watch failed to acknowledge a change in update frequency! ' +
      'Will retry.');
    setTimeout(function() { transmitCurrentUpdateFreq(); }, 10000);
  });
  console.log('Sent message to watch to set update frequency to ' + mins + 
      ' minutes.');
}

/** 
 * Fetches our core New Relic metrics for the app as configured in the 
 * currently saved Options, and sends the data to the watch.
 */
function fetchNewrelicData() {
  var options = Options.getSavedOptions();
  if (!options.apiKey || !options.appId) {
    console.log('Trying to poll New Relic API, but config data missing!');
    return;
  }
  console.log('Polling New Relic API for new data.');
  var req = new XMLHttpRequest();
  req.open('GET', NEWRELIC_API_URL + '/applications/' + options.appId + '.json', 
      true);
  req.setRequestHeader('X-Api-Key', options.apiKey);
  req.timeout = AJAX_TIMEOUT;
  req.onload = function(e) {
    if (req.status == 200) {
      console.log('Received successful response from New Relic API: ' + 
          req.responseText);
      var response = JSON.parse(req.responseText);
      var appSummary = response['application']['application_summary'];
      if (appSummary) {
        var apdexScore = appSummary['apdex_score'] || 0;  // null apdex if 0rpm
        Pebble.sendAppMessage({ 
          'APP_NAME_KEY': response['application']['name'],
          'APP_RESPONSE_TIME_KEY': appSummary['response_time'].toString(),
          // ^ Pebble doesn't have floats.
          'APP_THROUGHPUT_KEY': appSummary['throughput'],
          'APP_ERROR_RATE_KEY': appSummary['error_rate'].toString(),
          'APP_APDEX_SCORE_KEY': apdexScore.toFixed(2).toString(),
        });
      } else {
        // The application is not reporting data.
        Pebble.sendAppMessage({
          'APP_NAME_KEY': response['application']['name'],
          'APP_RESPONSE_TIME_KEY': '0',
          'APP_THROUGHPUT_KEY': 0,
          'APP_ERROR_RATE_KEY': '0',
          'APP_APDEX_SCORE_KEY': '0.00',
        });
      }
      console.log('Sent new New Relic data to watch.');
    } else { 
      console.log('Error fetching New Relic data! Response code ' + 
          req.status + ', body: ' + req.responseText); 
    }
  }
  req.onerror = function(e) { 
    console.log('Network error while fetching New Relic data!'); 
  }
  req.ontimeout = function(e) { 
    console.log('Timeout fetching New Relic data!'); 
  }
  req.send(null);
}


/************************ 
 * Pebble event handlers:
 ************************/

/**
 * Once PebbleKit is initialized, copy some initial app config from the phone
 * to the watch so it has all the state it needs.
 */
Pebble.addEventListener('ready',
  function(e) {
    console.log('PebbleKit JS initialized.');
    transmitCurrentUpdateFreq();
    fetchNewrelicData();
    // PEBBLE BUG?: When an inbound app message triggers the initialization of
    // the JS app on the phone, that message's "appmessage" event often fails
    // to fire. We eagerly send New Relic data here to counter that, but it 
    // sometimes results in doing duplicate data fetches at startup.
  }
);

/* When the user closes the config page, Pebble passes this handler the anchor 
 * text of the URL that triggered the close, which contains our stringified 
 * Options object.
 */
Pebble.addEventListener('webviewclosed', function(e) {
  console.log('Configuration window closed: ' + JSON.stringify(e));
  if (!e.response || e.response == '{}') {
    console.log('No options returned from configuration.');
    return;
  }
  try { 
    // Cancelling config with the Android back button returns 'CANCELLED' as 
    // the response, which wasn't in the Pebble docs... Let's be safe.
    console.log('User submitted (possibly new) configuration.');
    var serializedOptions = JSON.parse(decodeURIComponent(e.response));
    Options.fromObject(serializedOptions).save();
  } catch (err) {
    console.log('Error updating config. ' + err.message);
    return;
  }
  transmitCurrentUpdateFreq();
  fetchNewrelicData();
});

/** 
 * When the user launches the config window, load the page and pass it any 
 * previously-saved options.
 */
Pebble.addEventListener('showConfiguration', function() {
  var options = Options.getSavedOptions();
  console.log('Prepping config page. Previously saved options: ' + 
    JSON.stringify(options));
  var url = CONFIG_PAGE_URL + '#' + encodeURIComponent(JSON.stringify(options));
  console.log('Loading configuration page: ' + url);
  Pebble.openURL(url);
});

/** 
 * Handler/dispatcher for Pebble App Messages received by the phone from the
 * watch.
 */
Pebble.addEventListener('appmessage', function(e) {
  console.log('Phone received message from watch: ' + JSON.stringify(e));
  if (e['payload']['UPDATE_REQ_KEY']) {
    fetchNewrelicData();
  }
});
