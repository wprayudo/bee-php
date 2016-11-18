<?php
class MsgPackTest extends PHPUnit_Framework_TestCase
{
    protected static $bee;

    public static function setUpBeforeClass()
    {
        self::$bee = new Bee('localhost', getenv('PRIMARY_PORT'), 'test', 'test');
        self::$bee->ping();
    }

    public function test_00_msgpack_call() {
        $resp = self::$bee->call('test_4', [
                '4TL2tLIXqMqyGQm_kiE7mRrS96I5E8nqU', 'B627', 0, [
                    'browser_stats_first_session_hits' => 1
                ]
        ]);
        $this->assertEquals($resp[0][0], 2);
        $resp = self::$bee->call('test_4', [
                '4TL2tLIXqMqyGQm_kiE7mRrS96I5E8nqU', 'B627', 0, [
                    'browser_stats_first_session_hit' => 1
                ]
        ]);
        $this->assertEquals($resp[0][0], 2);
    }

    /**
     * @expectedException Exception
     * @expectedExceptionMessage Bad key type for PHP Array
     **/
    public function test_01_msgpack_array_key() {
        self::$bee->select("msgpack", array(2));
    }

    /**
     * @expectedException Exception
     * @expectedExceptionMessage Bad key type for PHP Array
     **/
    public function test_02_msgpack_float_key() {
        self::$bee->select("msgpack", array(1));
    }

    /**
     * @expectedException Exception
     * @expectedExceptionMessage Bad key type for PHP Array
     **/
    public function test_03_msgpack_array_of_float_as_key() {
        self::$bee->select("msgpack", array(3));
    }

    public function test_04_msgpack_integer_keys_arrays() {
        self::$bee->replace("msgpack", array(4,
                "Integer keys causing server to error",
                array(2 => 'maria', 5 => 'db')
            )
        );
    }

    public function test_05_msgpack_string_keys() {
        self::$bee->replace("msgpack", array(5,
                "String keys in response forbids client to take values by keys",
                array(2 => 'maria', 5 => 'db', 'lol' => 'lal')
            )
        );
        $resp = self::$bee->select("msgpack", array(5));
        $this->assertEquals($resp[0][2]['lol'], 'lal');
        $this->assertTrue(True);
        $resp = self::$bee->select("msgpack", array(6));
        $this->assertEquals($resp[0][2]['megusta'], array(1, 2, 3));
        $this->assertTrue(True);
    }
}
