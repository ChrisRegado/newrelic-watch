/************
 * Constants:
 ************/

/** Base URL for the New Relic API. */
NEWRELIC_API_URL = 'https://api.newrelic.com/v2';
/** Maximum amount of time (in ms) to wait for New Relic API responses. */
AJAX_TIMEOUT = 30000;


/***************************
 * Configuration management:
 ***************************/

/**
 * Creates an instance of Options.
 *
 * @class Holds user-supplied configuration data.
 *        Kludge: This class is effectively duplicated for the watch app JS
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

/** A default value for the update frequency option. */
Options.DEFAULT_UPDATE_FREQ = 5;

/**
 * Displays these config options on the page.
 *
 * @this {Options}
 */
Options.prototype.display = function() {
  console.log('Loaded options: ' + JSON.stringify(this));
  $('#api-key').val(this.apiKey);
  $('#app-selector').val(this.appId).change();
  $('#update-freq').val(this.updateFreq || Options.DEFAULT_UPDATE_FREQ);
}

/**
 * Fetch the options as they are currently configured on-page.
 *
 * @return {Options} The current options.
 */
Options.getCurrentOptions = function() {
  return new Options(
    $('#api-key').val(), 
    $('#app-selector').val(),
    $('#update-freq').val()
  );
}

/**
 * Returns an Options object representing the currently persisted options.
 *
 * @return {Options} Currently saved options.
 */
Options.getSavedOptions = function() {
  var options = JSON.parse(decodeURIComponent(
        window.location.hash.substring(1)) || '{}');
  options.__proto__ = Options.prototype;
  return options;
}


/*******************
 * Field population:
 *******************/

/**
 * Displays the given error message on the API Key input field.
 *
 * @param {String} txt Message to display.
 */
function displayKeyError(txt) {
  $('#api-key-error').text(txt);
  $('#api-key-error-container').addClass('error');
  $('#api-key-error').show();
}

/**
 * Removes the API Key input field error state.
 */
function clearKeyError() {
  $('#api-key-error').hide();
  $('#api-key-error').text('');
  $('#api-key-error-container').removeClass('error');
}

/** 
 * Populates the app selection dropdown with all apps available in the 
 * New Relic listing under the currently configured API key.
 */
function populateAppList() {
  var apiKey = $('#api-key').val();
  $('#app-selector').prop('disabled', true);
  $('#app-selector').val('').change();
  if (!apiKey) {
    displayKeyError('Please enter your API key.');
    return;
  } else {
    displayKeyError('Checking key...');
  }

  $.ajax({
    url: NEWRELIC_API_URL + '/applications.json',
    headers: { 'X-Api-Key': apiKey },
    dataType: 'json',
    timeout: AJAX_TIMEOUT,
  })
  .done(function(data) {
    clearKeyError();
    // Wipe the current options and insert all the new ones:
    $('#app-selector').empty();
    $('#app-selector').val('').change();
    $(data['applications']).each(function() {
      $('<option />', {
        val: this['id'],
        text: this['name'],
      }).appendTo($('#app-selector'));
    })
    $('#app-selector').prop('disabled', false);
    // Try to restore the saved option as a default selection:
    var options = Options.getSavedOptions();
    $('#app-selector').val(options.appId).change();
  })
  .fail(function(jqXHR, textStatus, errorThrown) {
    $('#app-selector').empty();
    $('#app-selector').val('').change();
    console.log('Failed to fetch New Relic app list: ' + errorThrown);
    switch (jqXHR.status) {
      case 401:
        displayKeyError('Invalid API key!');
        break;
      case 403:
        displayKeyError('New Relic API access has not been enabled.');
        break;
      default:
        displayKeyError('Unable to fetch New Relic app list!');
        break;
    }
  });
}

/**
 * Try to restore the last-known state on document ready.
 */
$('document').ready(function() {
  Options.getSavedOptions().display();
  populateAppList();
});

/**
 * UI event handlers.
 */
$('document').ready(function() {

  /**
   * Return to Pebble app on cancellation.
   */
  $('#cancel-button').click(function() {
    console.log('Configuration cancelled.');
    document.location = 'pebblejs://close';
  });

  /**
   * Marshal Options to the Pebble app on save.
   */
  $('#save-button').click(function() {
    var location = 'pebblejs://close#' + 
      encodeURIComponent(JSON.stringify(Options.getCurrentOptions()));
    console.log('Saving configuration...');
    document.location = location;
  });

  /**
   * Validate API key format.
   */
  $('#api-key').change(function() {
    $(this).val($(this).val().replace(/[^a-z0-9]/gi,''));
    // Find all apps for the new API key:
    populateAppList();
  });

  /**
   * Disable the save button if configuration is incomplete.
   */
  $('#app-selector').change(function() {
    if ($(this).val()) {
      $('#save-button').prop('disabled', false);
    } else {
      $('#save-button').prop('disabled', true);
    }
  });

  /**
   * Clean up update frequency entries for the user.
   * Validation is also done in the watch app.
   */
  $('#update-freq').change(function() {
    var parsedUpdateFreq = parseInt($(this).val());
    if (parsedUpdateFreq && parsedUpdateFreq >= 1) {
      $(this).val(parsedUpdateFreq);
    } else {
      $(this).val(Options.DEFAULT_UPDATE_FREQ);
    }
  });

});
