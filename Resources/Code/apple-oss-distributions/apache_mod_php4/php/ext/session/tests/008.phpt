--TEST--
bug compatibility: global is used albeit register_globals=0
--SKIPIF--
<?php include('skipif.inc'); 
 if (version_compare(PHP_VERSION,"4.2.3-dev", ">=")) die("skip this is for PHP < 4.2.3");
?>
--INI--
session.use_cookies=0
session.cache_limiter=
register_globals=0
session.bug_compat_42=1
session.bug_compat_warn=0
--FILE--
<?php
error_reporting(E_ALL & ~E_NOTICE);

session_id("abtest");

### Phase 1 cleanup
session_start();
session_destroy();

### Phase 2 $HTTP_SESSION_VARS["c"] does not contain any value
session_id("abtest");
session_register("c");
var_dump($c);
unset($c);
$c = 3.14;
session_write_close();
unset($HTTP_SESSION_VARS);
unset($c);

### Phase 3 $HTTP_SESSION_VARS["c"] is set
session_start();
var_dump($HTTP_SESSION_VARS);
unset($c);
$c = 2.78;

session_write_close();
unset($HTTP_SESSION_VARS);
unset($c);

### Phase 4 final

session_start();
var_dump($c);
var_dump($HTTP_SESSION_VARS);

session_destroy();
?>
--EXPECT--
NULL
array(1) {
  ["c"]=>
  float(3.14)
}
NULL
array(1) {
  ["c"]=>
  float(3.14)
}
