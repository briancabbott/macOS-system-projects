--TEST--
var_export() and objects with numeric indexes properties
--FILE--
<?php
$a = (object) array (1, 3, "foo" => "bar");
var_export($a);
?>
--EXPECT--
class stdClass {
  var $foo = 'bar';
}
