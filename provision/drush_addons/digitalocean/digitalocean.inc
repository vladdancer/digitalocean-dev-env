<?php
/**
 * This is an include file.  These are the "callback" functions that drush
 * uses to get the job done.  See digitalocean.drush.inc for the definition of
 * the commands.
 *
 * Example make file for d7 http://drupal.org/node/852412
 *
 * site-install issues:
 *  - http://drupal.org/node/992928 - Drupal Core
 *  - http://drupal.org/node/983914 - Drush
 * @file
 */

/**
 * Helper function for executing shell commands.  Throws exceptions on error.
 *
 * @param <type> $cmd the command to run
 * @param <type> $throwexception throw exception on error?
 */
function digitalocean_shell_exec($cmd, $throwexception=true) {
  // Note: for shell commands: 0=success, non-zero=error code
  $ret = drush_shell_exec($cmd);
  if ($throwexception && !$ret) {
    $msg = "Command returned unexpected result: $cmd";
    $output = drush_shell_exec_output();
    foreach ($output as $line) {
      $msg="\n  $line";
    }
    throw new Exception($msg);
  }
}

/**
 * Helper function for running SQL on a local mysql database.
 *
 * @param <type> $sql
 */
function digitalocean_mysql_exec($sql, $dbuser, $dbpassword) {
  digitalocean_shell_exec("echo \"$sql\" | mysql -u$dbuser -p$dbpassword");
}

/**
 * Helper function for dereferencing paths.
 *
 * @param <type> $pathin
 * @return <type>
 */
function digitalocean_fixpath($pathin) {
  // fix ~'s
  //$homedir = "/home/" . exec('whoami');
  $homedir = exec('echo $HOME');
  $pathout = str_replace("~", $homedir, $pathin);
  // realpath fails on non-existant folders.  So it's hard to fix .'s and ..'s
  return $pathout;
}

/**
 * This is the virtual hosts template file for the apache functions.
 */
define('APACHE_TEMPLATE', "
<VirtualHost *:80>
  Alias /#DOMAIN# #CODEPATH#
  <Directory #CODEPATH#>
    Options Indexes FollowSymLinks MultiViews
    AllowOverride All
    Order allow,deny
    Allow from all
  </Directory>
</VirtualHost>
<IfModule mod_ssl.c>
  <VirtualHost *:443>
    Alias /#DOMAIN# #CODEPATH#
    <Directory #CODEPATH#>
      Options Indexes FollowSymLinks MultiViews
      AllowOverride All
      Order allow,deny
      Allow from all
    </Directory>
  </VirtualHost>
</IfModule>");

/**
 * Configure apache virtualhost file using the template.
 * Files put in /etc/apache2/sites-available/$domain.conf, then a2ensite'd
 * Runs: apache2 restart
 *
 * @param <type> $domain
 * @param <type> $params 'codepath'
 */
function digitalocean_create_apache($domain, $params) {
  // get codepath and make sure it exists.
  $codepath = $params['codepath'];

  if (!file_exists($codepath)) mkdir($codepath, 0770, true);

  // get template
  $str_config = APACHE_TEMPLATE;

  // make our own :-)
  $str_config = str_replace("#DOMAIN#", $domain, $str_config);
  $str_config = str_replace("#CODEPATH#", $codepath, $str_config);

  $config_handle = fopen("/etc/apache2/sites-available/$domain.conf", "w+");
  fwrite($config_handle, $str_config);

  // enable using script
  digitalocean_shell_exec("sudo a2ensite $domain.conf");

  // restart apache
  // use sysvinit scripts instead of upstart for more compatibility(debian, etc)
  digitalocean_shell_exec("sudo /etc/init.d/apache2 restart");
}

/**
 * Destroy the apache virtual hosts config. a2dissite, then rm file.
 * Runs: apache2 restart
 *
 * @param <type> $domain
 * @param <type> $params
 */
function digitalocean_destroy_apache($domain, $params) {
  digitalocean_shell_exec("sudo a2dissite $domain.conf", false);
  $filename = "/etc/apache2/sites-available/$domain.conf";
  if (file_exists($filename)) unlink($filename);  // unlink = delete file
  // use sysvinit scripts instead of upstart for more compatibility(debian, etc)
  digitalocean_shell_exec("sudo /etc/init.d/apache2 restart");
}

/**
 * These define SQL commands to run on database.  CREATE USER causes trouble.  Note GRANT ALL ON *.* for site-install
 */
define('DATABASE_CREATE', "
GRANT USAGE ON *.* TO #DOMAIN#@localhost IDENTIFIED BY '#DOMAIN#' WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0 MAX_USER_CONNECTIONS 0;
CREATE DATABASE IF NOT EXISTS #DOMAIN#;
GRANT ALL PRIVILEGES ON *.* TO #DOMAIN#@localhost;
FLUSH PRIVILEGES;");
/**
 * We GRANT before DROP USER to make sure there is something to drop.  USAGE is synonym for 'no priveledges'.
 */
define('DATABASE_DESTROY', "
GRANT USAGE ON *.* TO #DOMAIN#@localhost;
DROP USER #DOMAIN#@localhost;
DROP DATABASE IF EXISTS #DOMAIN#;
FLUSH PRIVILEGES;");

/**
 * Create the database.  Runs SQL.  Replaces .'s with _'s in domain name
 * to get the database name, database username, and password.
 *
 * @param <type> $domain
 * @param <type> $params
 */
function digitalocean_create_database($domain, $params) {
  $dbuser = $params['dbuser'];
  $dbpassword = $params['dbpassword'];
  $domain_ = $params['domain_'];

  $sql = str_replace("#DOMAIN#", $domain_, DATABASE_CREATE);
  digitalocean_mysql_exec($sql, $dbuser, $dbpassword);
}
/**
 * Destroys the database using SQL.
 *
 * @param <type> $domain
 * @param <type> $params
 */
function digitalocean_destroy_database($domain, $params) {
  $dbuser = $params['dbuser'];
  $dbpassword = $params['dbpassword'];
  $domain_ = $params['domain_'];

  $sql = str_replace("#DOMAIN#", $domain_, DATABASE_DESTROY);
  digitalocean_mysql_exec($sql, $dbuser, $dbpassword);
}

define('TIP_FILE', '/etc/motd.tail');
define('CONFIG_PATTERN_LINE', "Your MySQL root user's password is ");

function _digital_ocean_get_default_mysql_password() {
  $result = '';
  if (file_exists(TIP_FILE)) {
    $config = file_get_contents(TIP_FILE);
    $pattern = preg_quote(CONFIG_PATTERN_LINE, '/');
    $pattern = "/^.*$pattern.*\$/m";
    preg_match($pattern, $config, $matches);

    if (!empty($matches[0])) {
      $result = substr($matches[0], strlen(CONFIG_PATTERN_LINE));
    }
  }

  return $result;
}
