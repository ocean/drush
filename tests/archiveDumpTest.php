<?php

namespace Unish;

/*
 * Tests for archive.drush.inc
 *
 * @group archive
 * @group slow
 */
class archiveDumpCase extends CommandUnishTestCase {

  /*
   * Test dump and extraction.
   */
  function testArchiveDump() {

    $env = 'testarchivedump';
    // $this->setUpDrupal($env, TRUE);
    $this->setUpDrupal(1, TRUE);
    // $root = $this->sites[$env]['root'];
    $root = $this->webroot();
    // $docroot = 'web';
    // $uri = 'dev';

    // Create the alias for D7 site.
    // $aliases['archivedump'] = array(
    //   'root' => UNISH_SANDBOX . '/' . $docroot,
    //   // 'root' => $root,
    //   'uri' => $env,
    // );
    // $contents = $this->file_aliases($aliases);
    // $alias_path = "$root/aliases.drushrc.php";
    // file_put_contents($alias_path, $contents);

    $name = "example";
    $dump_dest = "dump.tar.gz";
    $options = array(
      'root' => $root,
      'uri' => $env,
      'yes' => NULL,
      'destination' => 'dump.tar.gz',
    );
    $this->drush('archive-dump', 'default', $options);
    $exec = sprintf('file %s/%s', UNISH_SANDBOX, $dump_dest);
    $this->execute($exec);
    $output = $this->getOutput();
    $expected = UNISH_SANDBOX . "/dump.tar.gz: gzip compressed data, from Unix";
    $this->assertEquals($expected, $output);
 
    // Untar it, make sure it looks right.
    $exec = sprintf('tar -xzf %s/%s', UNISH_SANDBOX, $dump_dest);
    $untar_dest = UNISH_SANDBOX . '/untar';
    $exec = sprintf('mkdir %s && cd %s && tar xf %s/%s', $untar_dest, $untar_dest, UNISH_SANDBOX, $dump_dest);
    $this->execute($exec);
    $this->execute(sprintf('head %s/unish_%s.sql | grep "MySQL dump"', $untar_dest, $env));
    $this->execute('test -f ' . $untar_dest . '/MANIFEST.info');
    $this->execute('test -d ' . $untar_dest . '/' . $docroot);
  }
}