<?php

include("digitalocean.inc");

/**
 * @file
 *   digitalocean drush commands.
 *
 *   These commands are designed to be run inside the digitalocean development environment.
 */

/**
 * Simple helper function to keep all the arguments in a list.
 * @return <type>
 */
function _digitalocean_arg_list() {
  return array(
    'database'=>'database',
    'apache'  =>'apache'
  );
}

/**
 * Implements hook_drush_command()
 * @return array
 */
function digitalocean_drush_command() {
  $items = array();

  $items['digitalocean-create'] = array(
    'description' => "Create a development website (" . implode(', ', _digitalocean_arg_list()) . ").",
    'arguments' => array(
      'what' => 'Optional. One or more items to create: [' . implode('|', _digitalocean_arg_list()) . '|all] Defaults to "all"',
    ),
    'options' => array(
      'domain'       => 'Required (all).  Domain name of new site.  Use this to reference site with other commands.  Becomes dbname, dbuser, dbpass',
      'dbuser'       => 'Optional (db).  Root mysql user.  Defaults to "root"',
      'dbpassword'   => 'Optional (db).  Root mysql password.  Defaults to system',
      'codepath'     => 'Optional (apache).  Path to drupal code.  Defaults to "/var/www/[domain]"',
    ),
    'examples' => array(
      'Create D7 site in one command: setup apache, create db' => 'drush digitalocean-create --domain=example1',
      'Only setup apache vhost (/etc/apache2/sites-enabled/)' => 'drush digitalocean-create apache --domain=example --codepath=/path/to/drupal',
      'Only setup database (mysql)' => 'drush digitalocean-create database --domain=example',
    ),
    'aliases' => array('doc'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.
  );

  $items['digitalocean-destroy'] = array(
    'description' => "Destroy a development website (" . implode(', ', _digitalocean_arg_list()) . ").",
    'arguments' => array(
      'what' => 'Optional. One or more items to destroy: [' . implode('|', _digitalocean_arg_list()) . '|all] Defaults to "all"',
    ),
    'options' => array(
      'domain' => 'Domain name to destroy.',
      'codepath' => 'Path to drupal code.  Defaults to /var/www/[domain]',
      'yes'      => 'Optional. Skip confirmation',
    ),
    'examples' => array(
      'Simplest format' => 'drush dod --domain=example',
      'Complete format' => 'drush digitalocean-destroy all --domain=example --codepath=/path/to/drupal',
      'Only Remove Apache (/etc/apache2/sites-enabled/)' => 'drush digitalocean-destroy apache --domain=example',
      'Only Drop Database (mysql db and user)' => 'drush digitalocean-destroy database --domain=example',
    ),
    'aliases' => array('dod'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.
  );

  $items['digitalocean-fixperms'] = array(
    'description' => "Fix permissions for a development website:
  \$ sudo chown -R :www-data <codepath>
  \$ sudo chmod -R a=,u=rwX,g=rX <codepath>
  \$ sudo find <codepath>/sites -type d -name files -exec chmod -R a=,ug=rwX '{}' \;
  \$ sudo find <codepath>/sites -type d -name files_private -exec chmod -R a=,ug=rwX '{}' \;",
    'arguments' => array(
    ),
    'options' => array(
      'domain' => 'Domain name to fix permissions.',
      'codepath' => 'Path to drupal code.  Defaults to /var/www/[domain]',
    ),
    'examples' => array(
      'Simplest format' => 'drush dof --domain=example',
      'Use different codepath' => 'drush digitalocean-fixperms --domain=example --codepath=/path/to/drupal',
    ),
    'aliases' => array('dof'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.
  );
  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function digitalocean_drush_help($section) {
  switch ($section) {
  }
}

/**
 * Helper function to check input parameters
 * @param  $required any parameters that are required
 * @return array  if $ret['fail'] is true, then check failed, and an error was logged.
 */
function _digitalocean_params($required) {
  // check arguments
  $commands = drush_get_arguments();
  $options = drush_get_context('cli');
  $ret = array();

  // Commands

  // what steps do we take: "" -> "all" -> all our setup steps.
  if (count($commands)==1) {
    $commands = array("all");
  }
  if(array_search("all", $commands)!==false) {
    $commands = _digitalocean_arg_list();
  }

  // Options

  // Domain always required
  // remove /'s
  $options['domain'] = str_replace('/', '', $options['domain']);
  if (strlen($options['domain']) > 15) {
    $ret['fail'] = true;
    drush_log("  '--domain' must be less than 16 characters long, for mysql user name to work.", 'error');
  }
  if (strlen($options['domain']) < 3) {
    $ret['fail'] = true;
    drush_log("  '--domain' must be at least 3 characters long.", 'error');
  }

  // defaults
  $defaults['domain_'] = $domain_ = str_replace('.','_',$options['domain']);
  // database
  $defaults['dbuser'] = 'root';
  $defaults['dbpassword'] = _digital_ocean_get_default_mysql_password();
  $defaults['codepath'] = digitalocean_fixpath("/var/www/" . $options['domain']);

  // Merge it all together - 2nd array overwrites first.
  $ret = array_merge($ret, $commands);
  $ret = array_merge($ret, $defaults);
  $ret = array_merge($ret, $options);

  // Check required
  foreach($required as $require) {
    if (empty($ret[$require])) {
      $ret['fail'] = true;
      drush_log("  '--$require' is a required option for this command.", 'error');
    }
  }
  return $ret;
}

/**
 * Drush callbackfunction for 'drush digitalocean-fixperms'.
 *
 * This function fixes permissions for a drupal site.
 *
 * @reurn true on success
 */
function drush_digitalocean_fixperms() {
  $params = _digitalocean_params(array('domain'));
  if (isset($params['fail'])) return drush_set_error(0, dt('Bad parameters.  Try "drush help digitalocean-fixperms"'));

  $codepath = $params['codepath'];

  digitalocean_shell_exec("sudo chown -R $USER:www-data $codepath");
  digitalocean_shell_exec("sudo chmod -R a=,u=rwX,g=rX $codepath");
  // find directories named "files" and run chmod on them.
  digitalocean_shell_exec("sudo find $codepath/sites -type d -name files -exec chmod -R a=,ug=rwX '{}' \;");
  digitalocean_shell_exec("sudo find $codepath/sites -type d -name files_private -exec chmod -R a=,ug=rwX '{}' \;");
}

/**
 * Drush callbackfunction for 'drush digitalocean-create'.
 *
 * This function goes step-by-step in creating the new site.
 *
 * @reurn true on success
 */
function drush_digitalocean_create() {
  $params = _digitalocean_params(array('domain'));
  if (isset($params['fail'])) return drush_set_error(0, dt('Bad parameters.  Try "drush help digitalocean-create"'));
  $domain = $params['domain'];
  $domain_ = $params['domain_'];

  drush_log(' ', 'ok'); // add newline at top of output

  try {
    if (array_search("database", $params)) {
      drush_log("Creating database: $domain_", 'ok');
      digitalocean_create_database($domain, $params);
      drush_log('                                                ... done.', 'ok');
    }
    if (array_search("apache", $params)) {  // apache after code to handle mkdir
      drush_log("Creating apache config (/etc/apache2/sites-enabled/$domain)", 'ok');
      digitalocean_create_apache($domain, $params);
      drush_log('                                                ... done.', 'ok');
    }
  } catch (Exception $e) {
    $message = $e->getMessage();
    drush_log("Command failed: $message", "error");
    return false;
  }

  drush_log("Command completed successfully.", "success");
  return true;
}

/**
 * Drush callback function for 'drush digitalocean-destroy'
 *
 * Works step-by-step to destroy the site.
 *
 * @return true on success
 */
function drush_digitalocean_destroy() {
  $params = _digitalocean_params(array('domain'));
  if (isset($params['fail'])) return drush_set_error(0, dt('Bad parameters.  Try "drush help digitalocean-destroy"'));
  $domain = $params['domain'];

  $prompt = dt(
    'This will destroy the code, database and user, apache config, and dns config for @domain. Continue?',
    array('@domain' => $domain)
  );
  if (!drush_confirm($prompt)) {
    return drush_user_abort();
  }

  drush_log(' ', 'ok'); // add newline at top of output

  try {
    if (array_search("database", $params)) {
      drush_log('Destroying database and user ' . $params['domain_'] . ' ...', 'ok');
      digitalocean_destroy_database($domain, $params);
      drush_log('                                        ... done.', 'ok');
    }
    if (array_search("apache", $params)) {
      drush_log("Destroying apache config (/etc/apache2/sites-enabled/$domain) ...", 'ok');
      digitalocean_destroy_apache($domain, $params);
      drush_log('                                        ... done.', 'ok');
    }
  } catch (Exception $e) {
    $message = $e->getMessage();
    drush_log("Command failed: $message", "error");
    return false;
  }

  drush_log("Command completed successfully.", "success");
  return true;
}