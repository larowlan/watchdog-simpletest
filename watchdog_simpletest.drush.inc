<?php

/**
 * Implementation of hook_drush_help().
 */
function watchdog_simpletest_drush_help($section) {
  switch ($section)  {
    case 'drush:watchdog-simpletest-show':
      return dt('Show simpletest watchdog messages. Arguments and options can be combined to configure which messages to show.');
  }
}

/**
 * Implementation of hook_drush_command().
 */
function watchdog_simpletest_drush_command() {
  $items['watchdog-simpletest-show'] = array(
    'description' => 'Show simpletest watchdog messages.',
    'drupal dependencies' => drush_drupal_major_version() >= 6 ? array('dblog', 'simpletest') : array('watchdog'),
    'arguments' => array(
      'wid' => 'Optional id of a watchdog message to show in detail. If not provided, a listing of most recent 10 messages will be displayed. Alternatively if a string is provided, watchdog messages will be filtered by it.',
    ),
    'options' => array(
      'count' => 'The number of messages to show. Defaults to 10.',
      'severity' => 'Restrict to messages of a given severity level.',
      'type' => 'Restrict to messages of a given type.',
      'tail' => 'Continuously show new watchdog messages until interrupted.',
      'sleep-delay' => 'To be used in conjunction with --tail. This is the number of seconds to wait between each poll to the database. Delay is 1 second by default.'
    ),
    'examples' => array(
      'drush watchdog-simpletest-show' => 'Show a listing of most recent 10 messages.',
      'drush watchdog-simpletest-show 64' => 'Show in detail message with id 64.',
      'drush watchdog-simpletest-show "cron run succesful"' => 'Show a listing of most recent 10 messages containing the string "cron run succesful".',
      'drush watchdog-simpletest-show --count=46' => 'Show a listing of most recent 46 messages.',
      'drush watchdog-simpletest-show --severity=notice' => 'Show a listing of most recent 10 messages with a severity of notice.',
      'drush watchdog-simpletest-show --type=php' => 'Show a listing of most recent 10 messages of type php.',
      'drush watchdog-simpletest-show --tail' => 'Show a listing of most recent 10 messages and continue showing messages as they are registered in the watchdog.',
      'drush watchdog-simpletest-show --tail --sleep-delay=2' => 'Do a tail of the watchdog with a delay of two seconds between each poll to the database.',
    ),
    'aliases' => array('wd-sshow', 'wss'),
  );
  return $items;
}

/**
 * Command callback.
 */
function drush_core_watchdog_simpletest_show($arg = NULL) {
  if (is_numeric($arg)) {
    drush_core_watchdog_simpletest_show_one($arg);
  }
  else {
    drush_core_watchdog_simpletest_show_many($arg);
  }
}

/**
 * Print a watchdog message.
 *
 * @param $wid
 *    The id of the message to show.
 */
function drush_core_watchdog_simpletest_show_one($wid) {
  $rsc = drush_db_select('watchdog', '*', 'wid = :wid', array(':wid' => $wid), 0, 1);
  $result = drush_db_fetch_object($rsc);
  if (!$result) {
    return drush_set_error(dt('Watchdog message #!wid not found.', array('!wid' => $wid)));
  }
  $result = core_watchdog_format_result($result, TRUE);
  foreach ($result as $key => $value) {
    $uc = ucfirst($key);
    $upper->$uc = $value;
  }
  drush_print_table(drush_key_value_to_array_table($upper));
  print "\n";
}

/**
 * Print a table of watchdog messages.
 *
 * @param $filter
 *   String to filter the message's text by.
 */
function drush_core_watchdog_simpletest_show_many($filter = NULL) {
  $count = drush_get_option('count', 10);
  $type = drush_get_option('type');
  $severity = drush_get_option('severity');
  $tail = drush_get_option('tail', FALSE);

  $where = core_watchdog_query($type, $severity, $filter);
  if ($where === FALSE) {
    return drush_log(dt('Aborting.'));
  }

  $prefix_rsc = drush_db_select('simpletest_test_id', 'last_prefix', NULL, NULL, 0, 1, 'test_id', 'DESC');
  $prefix = drush_db_fetch_object($prefix_rsc);
  if (empty($prefix->last_prefix)) {
    return drush_log(dt('Aborting.'));
  }

  $rsc = drush_db_select($prefix->last_prefix . 'watchdog', '*', $where['where'], $where['args'], 0, $count, 'wid', 'DESC');
  if ($rsc === FALSE) {
    return drush_log(dt('Aborting, no active test.'));
  }
  $header = array(dt('Id'), dt('Date'), dt('Severity'), dt('Type'), dt('Message'));
  while ($result = drush_db_fetch_object($rsc)) {
    $row = core_watchdog_format_result($result);
    $table[] = array($row->wid, $row->date, $row->severity, $row->type, $row->message);
  }
  if (empty($table)) {
    return drush_log(dt('No log messages available.'), 'ok');
  }
  else {
    drush_log(dt('Most recent !count watchdog log messages:', array('!count' => $count)));
    if ($tail) {
      $table = array_reverse($table);
    }
    array_unshift($table, $header);
    $tbl = drush_print_table($table, TRUE);
  }

  if ($tail) {
    // We will reuse the table object to display each line generated while in tail mode.
    // To make it possible some hacking is done on the object: remove the header and reset the rows on each iteration.
    $tbl->_headers = NULL;
    // Obtain the last wid.
    $last = array_pop($table);
    $last_wid = $last[0];
    // Adapt the where snippet.
    if ($where['where'] != '') {
      $where['where'] .= ' AND ';
    }
    $where['where'] .= 'wid > :wid';
    // sleep-delay
    $sleep_delay = drush_get_option('sleep-delay', 1);
    while (TRUE) {
      $where['args'][':wid'] = $last_wid;
      $table = array();
      // Reset table rows.
      $tbl->_data = array();
      $prefix_rsc = drush_db_select('simpletest_test_id', 'last_prefix', NULL, NULL, 0, 1, 'test_id', 'DESC');
      $prefix = drush_db_fetch_object($prefix_rsc);
      if (empty($prefix->last_prefix)) {
        return drush_log(dt('Aborting, no active test.'));
      }

      $rsc = drush_db_select($prefix->last_prefix . 'watchdog', '*', $where['where'], $where['args'], NULL, NULL, 'wid', 'ASC');
      while ($result = drush_db_fetch_object($rsc)) {
        $row = core_watchdog_format_result($result);
        $table[] = array($row->wid, $row->date, $row->severity, $row->type, $row->message);
        #$tbl->addRow(array($row->wid, $row->date, $row->severity, $row->type, $row->message));
        $last_wid = $row->wid;
      }
      $tbl->addData($table);
      print $tbl->_buildTable();
      sleep($sleep_delay);
    }
  }
  else {
    print "\n";
  }
}
