<?php
class MockTest extends \PHPUnit_Framework_TestCase
{
    public function testFoo()
    {
        $beex = $this->getMock('Bee');
        $beex->expects($this->once())->method('ping');
        $beex->ping();
    }

    public function testDoo()
    {
        try {
            (new Bee('localhost', getenv('PRIMARY_PORT')))->select('_vindex', [], 'name');
            $this->assertFalse(True);
        } catch (Exception $e) {
            $this->assertTrue(True);
        }
    }
}

