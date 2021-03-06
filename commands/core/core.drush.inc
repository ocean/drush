<?php

/**
 * @file
 *   Core drush commands.
 */

use Drush\Commands\core\EditCommands;
use Drush\Log\LogLevel;

/**
 * Implementation of hook_drush_help().
 *
 * This function is called whenever a drush user calls
 * 'drush help <name-of-your-command>'
 *
 * @param
 *   A string with the help section (prepend with 'drush:')
 *
 * @return
 *   A string with the help text for your command.
 */
function core_drush_help($section) {
  switch ($section) {
    case 'drush:rsync':
      return dt("Sync the entire drupal directory or a subdirectory to a <destination> using ssh. Excludes reserved files and directories for supported VCSs. Useful for pushing copies of your tree to a staging server, or retrieving a files directory from a remote site. Relative paths start from the Drupal root directory if a site alias is used; otherwise they start from the current working directory.");
    case 'error:DRUSH_DRUPAL_DB_ERROR':
      $message = dt("Drush was not able to start (bootstrap) the Drupal database.\n");
      $message .= dt("Hint: This may occur when Drush is trying to:\n");
      $message .= dt(" * bootstrap a site that has not been installed or does not have a configured database. In this case you can select another site with a working database setup by specifying the URI to use with the --uri parameter on the command line. See `drush topic docs-aliases` for details.\n");
      $message .= dt(" * connect the database through a socket. The socket file may be wrong or the php-cli may have no access to it in a jailed shell. See http://drupal.org/node/1428638 for details.\n");
      $message .= dt('More information may be available by running `drush status`');
      return $message;
    case 'error:DRUSH_DRUPAL_BOOTSTRAP_ERROR':
      $message = dt("Drush was not able to start (bootstrap) Drupal.\n");
      $message .= dt("Hint: This error can only occur once the database connection has already been successfully initiated, therefore this error generally points to a site configuration issue, and not a problem connecting to the database.\n");
      $message .= dt('More information may be available by running `drush status`');
      return $message;
      break;
  }
}

/**
 * Implementation of hook_drush_command().
 *
 * In this hook, you specify which commands your
 * drush module makes available, what it does and
 * description.
 *
 * Notice how this structure closely resembles how
 * you define menu hooks.
 *
 * @return
 *   An associative array describing your command(s).
 */
function core_drush_command() {
  $items = array();
  $items['core-quick-drupal'] = array(
    'description' => 'Download, install, serve and login to Drupal with minimal configuration and dependencies.',
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
    'aliases' => array('qd', 'cutie'),
    'arguments' => array(
      'site' => 'Short name for the site to be created - used as a directory name and as sqlite file name. Optional - if omitted timestamped "quick-drupal" directory will be used instead.',
      'projects' => 'A list of projects to download into the new site. If projects contain extensions (modules or themes) with the same name they will be enabled by default. See --enable option to control this behaviour further.',
    ),
    'examples' => array(
      'drush qd' => 'Download and install stable release of Drupal into a timestamped directory, start server, and open the site logged in as admin.',
      'drush qd --profile=minimal --cache --core=drupal-8.0.x --yes' => 'Fire up dev release of Drupal site with minimal install profile.',
      'drush qd testsite devel --server=:8081/admin --browser=firefox --cache --yes' => 'Fire up stable release (using the cache) of Drupal site called "testsite", download and enable devel module, start a server on port 8081 and open /admin in firefox.',
      'drush qd commercesite --core=commerce_kickstart --profile=commerce_kickstart --cache --yes --watchdog' => 'Download and install the "Commerce Kickstart" distribution/install profile, display watchdog messages on the server console.',
      'drush qd --makefile=mysite.make' => 'Create and install a site from a makefile.',
    ),
  );
  return $items;
}

function core_drush_help_alter(&$command) {
  if ($command['command'] == 'core-quick-drupal') {
    // Add in options/engines.
    drush_core_quick_drupal_options($command);
    // Add in topics for engines
    $command += drush_get_engine_topics();
  }
}

/**
 * Command argument complete callback.
 *
 * @return
 *   Array of available profile names.
 */
function core_site_install_complete() {
  $max = drush_bootstrap_max(DRUSH_BOOTSTRAP_DRUPAL_ROOT);
  if ($max >= DRUSH_BOOTSTRAP_DRUPAL_ROOT) {
    return array('values' => array_keys(drush_find_profiles(DRUPAL_ROOT)));
  }
}

/**
 * @defgroup engines Engine types
 * @{
 */

/**
 * Implementation of hook_drush_engine_type_info().
 */
function core_drush_engine_type_info() {
  $info = array();
  $info['drupal'] = array();
  return $info;
}

/**
 * Implements hook_drush_engine_ENGINE_TYPE().
 */
function core_drush_engine_drupal() {
  $engines = array(
    'environment' => array(),
    'site_install' => array(),
    'pm' => array(),
  );
  return $engines;
}

/**
 * @} End of "Engine types".
 */

/**
 * Callback for core-quick-drupal command.
 */
function drush_core_quick_drupal() {
  $requests = FALSE;
  $make_projects = array();
  $args = func_get_args();
  $name = drush_get_option('use-name');
  drush_set_option('backend', TRUE);
  drush_set_option('strict', FALSE); // We fail option validation because do so much internal drush_invoke().
  $makefile = drush_get_option('makefile');
  $root = \Drush::bootstrapManager()->getRoot();
  if (drush_get_option('use-existing', ($root != FALSE))) {
    if (!$root) {
      return drush_set_error('QUICK_DRUPAL_NO_ROOT_SPECIFIED', 'Must specify site with --root when using --use-existing.');
    }
    // If a --db-url was not explicitly provided, and if there is already
    // a settings.php file provided, then use the db-url defined inside it.
    if (!drush_get_option('db-url', FALSE)) {
      $values = drush_invoke_process('@self', 'site-alias', array('@self'), array('with-db-url' => TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
      if (!empty($values['object']['self']['db-url'])) {
        drush_set_option('db-url', $values['object']['self']['db-url']);
      }
    }
    if (empty($name)) {
      $name = basename($root);
    }
    $base = dirname($root);
  }
  else {
    if (!empty($args) && empty($name)) {
      $name = array_shift($args);
    }
    if (empty($name)) {
      $name = 'quick-drupal-' . gmdate('YmdHis', $_SERVER['REQUEST_TIME']);
    }
    $root = drush_get_option('root', FALSE);
    $core = drush_get_option('core', 'drupal');
    $project_rename = $core;
    if ($root) {
      $base = dirname($root);
      $project_rename = basename($root);
    }
    else {
      $base = getcwd() . '/' . $name;
      $root = $base . '/' . $core;
    }
    if (!empty($makefile)) {
      // Invoke 'drush make $makefile'.
      $result = drush_invoke_process('@none', 'make', array($makefile, $root), array('core-quick-drupal' => TRUE));
      if ($result['error_status'] != 0) {
        return drush_set_error('DRUSH_QD_MAKE', 'Could not make; aborting.');
      }
      $make_projects = array_diff(array_keys($result['object']['projects']), array('drupal'));
    }
    else {
      drush_mkdir($base);
      drush_set_option('destination', $base);
      drush_set_option('drupal-project-rename', $project_rename);
      if (drush_invoke('pm-download', array($core)) === FALSE) {
        return drush_set_error('QUICK_DRUPAL_CORE_DOWNLOAD_FAIL', 'Drupal core download/extract failed.');
      }
    }
  }
  if (!drush_get_option('db-url', FALSE)) {
    drush_set_option('db-url', 'sqlite://sites/' . strtolower(drush_get_option('sites-subdir', 'default')) . "/files/$name.sqlite");
  }
  // We have just created a site root where one did not exist before.
  // We therefore must manually reset the selected root to
  // our new root, and force a bootstrap to DRUSH_BOOTSTRAP_DRUPAL_ROOT.
  \Drush::bootstrapManager()->setRoot($root);
  if (!drush_bootstrap_to_phase(DRUSH_BOOTSTRAP_DRUPAL_ROOT)) {
    return drush_set_error('QUICK_DRUPAL_ROOT_LOCATE_FAIL', 'Unable to locate Drupal root directory.');
  }
  if (!empty($args)) {
    $requests = pm_parse_arguments($args, FALSE);
  }
  if ($requests) {
    // Unset --destination, so that downloads go to the site directories.
    drush_unset_option('destination');
    if (drush_invoke('pm-download', $requests) === FALSE) {
      return drush_set_error('QUICK_DRUPAL_PROJECT_DOWNLOAD_FAIL', 'Project download/extract failed.');
    }
  }
  drush_invoke('site-install', array(drush_get_option('profile')));
  // Log in with the admin user.
  // TODO: If site-install is given a sites-subdir other than 'default',
  // then it will bootstrap to DRUSH_BOOTSTRAP_DRUPAL_SITE get the installer
  // to recognize the desired site directory. This somehow interferes
  // with our desire to bootstrap to DRUSH_BOOTSTRAP_DRUPAL_LOGIN here.
  // We could do the last few steps in a new process if uri is not 'default'.
  drush_set_option('user', '1');
  if (!drush_bootstrap_to_phase(DRUSH_BOOTSTRAP_DRUPAL_LOGIN)) {
    return drush_set_error('QUICK_DRUPAL_INSTALL_FAIL', 'Drupal core install failed.');
  }
  $enable = array_merge(pm_parse_arguments(drush_get_option('enable', $requests)), $make_projects);
  if (!empty($enable)) {
    if (drush_invoke('pm-enable', $enable) === FALSE) {
     return drush_set_error('QUICK_DRUPAL_PROJECT_ENABLE_FAIL', 'Project enable failed.');
    }
  }
  $server = drush_get_option('server', '/');
  if ($server) {
    $server_uri = runserver_uri($server);
    _drush_core_qd_cache_uri($server_uri);
  }
  if (!drush_get_option('no-server', FALSE)) {
    if ($server) {
      // Current CLI user is also the web server user, which is for development
      // only. Hence we can safely make the site directory writable. This makes
      // it easier to delete and edit settings.php.
      $boot = \Drush::bootstrap();
      @chmod($boot->conf_path(), 0700);
      drush_invoke_process(array('root' => $root, 'uri' => $server_uri), 'runserver', array($server));
    }
  }
  else {
    drush_print(dt('Login URL: ') . drush_invoke('user-login'));
  }
}

// Write a drushrc.php to cache the server information for future Drush calls
function _drush_core_qd_cache_uri($uri) {
  $server = $uri['host'];
  if (!empty($uri["port"])) {
    $server .= ':' . $uri["port"];
  }
  $dir = DRUPAL_ROOT . '/drush';
  $target_file = $dir . '/drushrc.php';
  drush_log(dt("Caching 'uri' !uri in !target", array('!uri' => $server, '!target' => $target_file)), LogLevel::OK);
  $create_file = TRUE;
  if (file_exists($target_file)) {
    // Don't bother to ask with --use-existing; just
    // continue.
    if (drush_get_option('use-existing', FALSE)) {
      $create_file = FALSE;
    }
    else {
      $create_file = drush_confirm(dt('!target already exists. Overwrite?', array('!target' => $target_file)));
    }
  }
  $content = <<<EOT
<?php

// Inserted by `drush quick-drupal`.  This allows `drush user-login`
// and similar commands to work seemlessly.  Remove if using
// Drupal multisite feature.
\$options['uri'] = "$server";
EOT;
  if ($create_file) {
    drush_mkdir($dir);
    file_put_contents($target_file, $content);
  }
}

/**
 * Include options and engines for core-quick-drupal command, aggregated from
 * other command options that are available. We prefix option descriptions,
 * to make the long list more navigable.
 *
 * @param $command
 *   The command array, by reference. Used to add
 *   site-install options and add options and engines for core-quick-drupal.
 */
function drush_core_quick_drupal_options(&$command) {
  $options = array(
    'core' => 'Drupal core to download. Defaults to "drupal" (latest stable version).',
    'use-existing' => 'Use an existing Drupal root, specified with --root. Overrides --core. Defaults to true when run from an existing site.',
    'profile' => 'The install profile to use. Defaults to standard.',
    'enable' => 'Specific extensions (modules or themes) to enable. By default, extensions with the same name as requested projects will be enabled automatically.',
    'server' => 'Host IP address and port number to bind to and path to open in web browser (hyphen to clear a default path), all elements optional. See runserver examples for shorthand.',
    'no-server' => 'Avoid starting runserver (and browser) for the created Drupal site.',
    'browser' => 'Optional name of a browser to open site in. If omitted the OS default browser will be used. Set --no-browser to disable.',
    'use-name' => array('hidden' => TRUE, 'description' => 'Overrides "name" argument.'),
    'makefile' => array('description' => 'Makefile to use. Makefile must specify which version of Drupal core to build.', 'example-value' => 'mysite.make', 'value' => 'optional'),
    'root' => array('description' => 'Path to Drupal root.', 'example-value' => '/path/to/root', 'value' => 'optional'),
  );
  $commands = drush_get_commands();
  if (isset($commands['pm-download'])) {
    foreach ($commands['pm-download']['options'] as $option => $description) {
      if (is_array($description)) {
        $description = $description['description'];
      }
      $options[$option] = 'Download option: ' . $description;
    }
    // Unset a few options that are not usable here, as we control them ourselves
    // or they are otherwise implied by the environment.
    unset($options['destination']);
    unset($options['drupal-project-rename']);
    unset($options['default-major']);
    unset($options['use-site-dir']);
  }
  if (isset($commands['site-install'])) {
    foreach ($commands['site-install']['options'] as $option => $description) {
      if (is_array($description)) {
        $description = $description['description'];
      }
      $options[$option] = 'Site install option: ' . $description;
    }
    unset($options['sites-subdir']);
  }
  if (isset($commands['runserver'])) {
    foreach ($commands['runserver']['options'] as $option => $description) {
      $options[$option] = 'Runserver option: ' . $description;
    }
    unset($options['user']);
  }

  $command['options'] = $options;
  $command['engines'] = $commands['pm-download']['engines'];
}
