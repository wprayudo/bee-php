<?php
class AssertTest extends PHPUnit_Framework_TestCase
{
    protected static $bee, $tm;

    public static function setUpBeforeClass()
    {
        self::$bee = new Bee('localhost', getenv('PRIMARY_PORT'), 'test', 'test');
    }

    protected function tearDown()
    {
        $tuples = self::$bee->select("test");
        foreach($tuples as $value)
            self::$bee->delete("test", Array($value[0]));
    }

    /**
     * @expectedException Exception
     * @expectedExceptionMessage Can't read query
     **/
    public function test_00_timedout() {
        self::$bee->eval("
            function assertf()
                require('fiber').sleep(1)
                return 0
            end");
        self::$bee->call("assertf");

        /* We can reconnect and everything will be ok */
        self::$bee->select("test");
    }
}
