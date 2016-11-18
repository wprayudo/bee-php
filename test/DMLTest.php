<?php
class DMLTest extends PHPUnit_Framework_TestCase
{
		protected static $bee;

		public static function setUpBeforeClass()
		{
			self::$bee = new Bee('localhost', getenv('PRIMARY_PORT'), 'test', 'test');
		}

		protected function tearDown()
		{
			$tuples = self::$bee->select("test");
			foreach($tuples as $value)
				self::$bee->delete("test", $value[0]);
		}

		private function populate($num)
		{
			for ($i = 0; $i < $num; $i++) {
				$tuple = array($i, (($i + 1) * 2) % 5, "hello, ima tuple " . $i);
				self::$bee->replace("test", $tuple);
			}
		}

		private function devastate($num, $assert=False)
		{
			$this->assertTrue(self::$bee->ping());
			for ($i = 0; $i < $num; $i++) {
				$tuple = array($i, (($i + 1) * 2) % 5, "hello, ima tuple " . $i);
				if ($assert) {
					$this->assertEquals(self::$bee->delete("test", array($i))[0], $tuple);
				} else {
					self::$bee->delete("test", array($i));
				}
			}
		}

		public function test_00_select_basic() {
			$this->assertEmpty(
				self::$bee->select("test")
			);
		}

		public function test_01_select_big() {
			$this->populate(100);
			$this->assertCount(
				100, self::$bee->select("test")
			);
			$this->assertCount(20, self::$bee->select("test", array(1), "secondary"));
			$this->assertCount(20, self::$bee->select("test", array(2), "secondary"));
			$this->assertCount(20, self::$bee->select("test", array(3), "secondary"));
			$this->assertCount(20, self::$bee->select("test", array(4), "secondary"));
			$this->assertCount(20, self::$bee->select("test", array(0), "secondary"));
			$this->assertCount(
				0, self::$bee->select("test", array(3, "hello, ima tuple 94"), "secondary")
			);
			$this->assertCount(
				1, self::$bee->select("test", array(3, "hello, ima tuple 93"), "secondary")
			);
			$this->devastate(100);
		}

		public function test_02_select_diff() {
			$this->populate(100);
			$this->assertCount(100, self::$bee->select("test"));
			$this->assertCount(1, self::$bee->select("test", array(1)));
			$this->assertCount(20, self::$bee->select("test", array(1), "secondary"));
			$this->assertCount(10, self::$bee->select("test", array(1), "secondary", 10));
			$this->assertCount(10, self::$bee->select("test", array(1), "secondary", 11, 10));
			$this->assertCount(9, self::$bee->select("test", array(1), "secondary", 9, 10));
			$this->assertCount(10, self::$bee->select("test", array(1), "secondary", 10, 10));
			$this->devastate(100);
		}

		/**
		 * @expectedException Exception
		 * @expectedExceptionMessage Query error 3
		 **/
		public function test_03_insert_error() {
			self::$bee->insert("test", array(1, 2, "smth"));
			self::$bee->insert("test", array(1, 2, "smth"));
		}

		public function test_04_replaces() {
			$array_ = array(1, 2, "smth");
			$this->assertEquals($array_, self::$bee->insert("test", $array_)[0]);
			$array_ = array(1, 3, "smth completely different");
			$this->assertEquals($array_, self::$bee->replace("test", $array_)[0]);
		}

		public function test_05_delete() {
			$this->populate(5);
			$this->devastate(5, True);
		}

		public function test_06_update() {
			self::$bee->insert("test", array(1, 2, "test"));
			$result_tuple = array(
				1,
				18,
				"terrance and phillipe show",
				88,
				intval(0x01011)
			);
			self::$bee->update("test", 1, array(
				array(
					"field" => 1,
					"op" => "+",
					"arg" => 16
				),
				array(
					"field" => 3,
					"op" => "=",
					"arg" => 98
				),
				array(
					"field" => 4,
					"op" => "=",
					"arg" => intval(0x11111)
				),
			));
			self::$bee->update("test", 1, array(
				array(
					"field" => 3,
					"op" => "-",
					"arg" => 10
				),
				array(
					"field" => 4,
					"op" => "&",
					"arg" => intval(0x10101)
				)
			));
			self::$bee->update("test", 1, array(
				array(
					"field" => 4,
					"op" => "^",
					"arg" => intval(0x11100)
				)
			));
			self::$bee->update("test", 1, array(
				array(
					"field" => 4,
					"op" => "|",
					"arg" => intval(0x00010)
				)
			));
			$tuple = self::$bee->update("test", 1, array(
				array(
					"field" => 2,
					"op" => ":",
					"offset" => 2,
					"length" => 2,
					"list" => "rrance and phillipe show"
				)
			));
			$this->assertEquals($result_tuple, $tuple[0]);
		}

		public function test_07_update_no_error() {
			self::$bee->update("test", 0, array());
		}

		/**
		 * @expectedException Exception
		 * @ExpectedExceptionMessage Five fields
		 */
		public function test_08_update_error() {
			self::$bee->update("test", 0, array(
				array(
					"field" => 2,
					"op" => ":",
					"offset" => 2,
					"length" => 2,
				)
			));
		}

		/**
		 * @expectedException Exception
		 * @ExpectedExceptionMessage Field OP must be provided
		 */
		public function test_09_update_error() {
			self::$bee->update("test", 0, array(
					array(
							"field" => 2,
							"op" => "BAD_OP",
							"arg" => 2,
					)
			));
		}

		/**
		 * @expectedException Exception
		 * @ExpectedExceptionMessage Field OP must be provided
		 */
		public function test_10_update_error() {
			self::$bee->update("test", 0, array(
					array(
							"field" => 2,
							"arg" => 2,
					)
			));
		}

		/**
		 * @expectedException Exception
		 * @ExpectedExceptionMessage Three fields must be provided
		 */
		public function test_11_update_error() {
			self::$bee->update("test", 0,
				array(
					array(
						"op" => "^",
						"field" => 2,
						"arg" => 2,
						"unneeeded field" => "LALALLALA"
					)
				)
			);
		}

		public function test_12_call() {
			$this->assertEquals(
				self::$bee->call("test_2"),
				array(
					'0' => array(
						'0' => array('k1' => 'v2', 'k2' => 'v')
					)
				)
			);
			$this->assertEquals(
				self::$bee->call("test_3", array(3, 4)), array('0' => array('0' => 7)));
		}

		public function test_13_eval() {
			$this->assertEquals(
				self::$bee->eval("return test_1()"),
				array(
					'0' => 1,
					'1' => array(
						's' => array('0' => 1, '1' => 1428578535),
						'u' => 1428578535,
						'v' => array(),
						'c' => array(
							'2' => array('0' => 1, '1' => 1428578535),
							'106' => array('0' => 1, '1' => 1428578535)
						),
						'pc' => array(
							'2' => array('0' => 1, '1' => 1428578535, '2' => 9243),
							'106' => array('0' => 1, '1' => 1428578535, '2' => 9243)
						)
					),
					'2' => 1
				)
			);
			$this->assertEquals(
				self::$bee->eval("return test_2()"),
				array('0' => array('k1' => 'v2', 'k2' => 'v'))
			);
			$this->assertEquals(
				self::$bee->eval("return test_3(...)", array(3, 4)),
				array('0' => 7)
			);
			$this->assertEquals(
				self::$bee->evaluate("return test_3(...)", array(3, 4)),
				array('0' => 7)
			);
		}

		public function test_14_select_limit_defaults() {
			self::$bee->insert("test", array(1, 2, "hello"));
			self::$bee->insert("test", array(2, 3, "hello"));
			self::$bee->insert("test", array(3, 4, "hello"));
			self::$bee->insert("test", array(4, 2, "hello"));
			$this->assertEquals(count(self::$bee->select("test", 3, "secondary", null, null, Bee::ITERATOR_GT)), 1);
			$this->assertEquals(count(self::$bee->select("test", 3, "secondary", 0,    null, Bee::ITERATOR_GT)), 0);
			$this->assertEquals(count(self::$bee->select("test", 3, "secondary", 100,  null, Bee::ITERATOR_GT)), 1);
		}

		public function test_15_upsert() {
			self::$bee->replace("test", array(123, 2, "hello, world", "again", "and again"));
			$result_tuple = self::$bee->upsert("test", array(124, 10, "new tuple"), array(
				array(
					"field" => 1,
					"op" => "+",
					"arg" => 10
				)
			));
			$this->assertEquals(array(), $result_tuple);
			$result_tuple = self::$bee->select("test", 124);
			$this->assertEquals(array(124, 10, "new tuple"), $result_tuple[0]);
			$result_tuple = self::$bee->upsert("test", array(124, 10, "new tuple"), array(
				array(
					"field" => 1,
					"op" => "+",
					"arg" => 10
				)
			));
			$this->assertEquals(array(), $result_tuple);
			$result_tuple = self::$bee->select("test", 124);
			$this->assertEquals(array(124, 20, "new tuple"), $result_tuple[0]);
			$result_tuple = self::$bee->upsert("test", array(123, 10, "new tuple"), array(
				array(
					"field" => 3,
					"op" => "#",
					"arg" => 2
				),
				array(
					"field" => 2,
					"op" => ":",
					"offset" => 2,
					"length" => 3,
					"list" => "---"
				)
			));
			$this->assertEquals(array(), $result_tuple);
			$result_tuple = self::$bee->select("test", 123);
			$this->assertEquals(array(123, 2, "he---, world"), $result_tuple[0]);
		}

		public function test_16_hash_select() {
			self::$bee->select("test_hash");
			self::$bee->select("test_hash", []);
			try {
				self::$bee->select("test_hash", null, null, null, null, BEE::ITERATOR_EQ);
				$this->assertFalse(True);
			} catch (Exception $e) {
				$this->assertContains('Invalid key part', $e->getMessage());
			}
		}

		/**
		 * @dataProvider provideIteratorExceptions
		 */
		public function test_17_iterator_types($spc, $itype, $xcmsg = null) {
			if (func_num_args() === 3) {
				try {
					self::$bee->select($spc, null, null, null, null, $itype);
					$this->assertFalse(True);
				} catch (Exception $e) {
					$this->assertContains($xcmsg, $e->getMessage());
				}
			} else {
					self::$bee->select($spc, null, null, null, null, $itype);
					$this->assertTrue(True);
			}
		}

		public static function provideIteratorExceptions() {
			return [
				['test_hash', 'EQ'                 ,'Invalid key part'],
				['test_hash', 'REQ'                ,'Invalid key part'],
				['test_hash', 'ALL'                ,                  ],
				['test_hash', 'LT'                 ,'Invalid key part'],
				['test_hash', 'LE'                 ,'Invalid key part'],
				['test_hash', 'GE'                 ,'Invalid key part'],
				['test_hash', 'GT'                 ,                  ],
				['test_hash', 'BITSET_ALL_SET'     ,'Invalid key part'],
				['test_hash', 'BITSET_ANY_SET'     ,'Invalid key part'],
				['test_hash', 'BITSET_ALL_NOT_SET' ,'Invalid key part'],
				['test_hash', 'BITS_ALL_SET'       ,'Invalid key part'],
				['test_hash', 'BITS_ANY_SET'       ,'Invalid key part'],
				['test_hash', 'BITS_ALL_NOT_SET'   ,'Invalid key part'],
				['test_hash', 'OVERLAPS'           ,'Invalid key part'],
				['test_hash', 'NEIGHBOR'           ,'Invalid key part'],
				['test_hash', 'eq'                 ,'Invalid key part'],
				['test_hash', 'req'                ,'Invalid key part'],
				['test_hash', 'all'                ,                  ],
				['test_hash', 'lt'                 ,'Invalid key part'],
				['test_hash', 'le'                 ,'Invalid key part'],
				['test_hash', 'ge'                 ,'Invalid key part'],
				['test_hash', 'gt'                 ,                  ],
				['test_hash', 'bitset_all_set'     ,'Invalid key part'],
				['test_hash', 'bitset_any_set'     ,'Invalid key part'],
				['test_hash', 'bitset_all_not_set' ,'Invalid key part'],
				['test_hash', 'bits_all_set'       ,'Invalid key part'],
				['test_hash', 'bits_any_set'       ,'Invalid key part'],
				['test_hash', 'bits_all_not_set'   ,'Invalid key part'],
				['test_hash', 'overlaps'           ,'Invalid key part'],
				['test_hash', 'neighbor'           ,'Invalid key part'],
				['test'     , 'bitset_all_set'     ,'does not support requested iterator type'],
				['test'     , 'bitset_any_set'     ,'does not support requested iterator type'],
				['test'     , 'bitset_all_not_set' ,'does not support requested iterator type'],
				['test'     , 'bits_all_set'       ,'does not support requested iterator type'],
				['test'     , 'bits_any_set'       ,'does not support requested iterator type'],
				['test'     , 'bits_all_not_set'   ,'does not support requested iterator type'],
				['test'     , 'overlaps'           ,'does not support requested iterator type'],
				['test'     , 'neighbor'           ,'does not support requested iterator type'],
				['test'     , 'oevrlps'            ,'Bad iterator name'],
				['test'     , 'e'                  ,'Bad iterator name'],
				['test'     , 'nghb'               ,'Bad iterator name'],
				['test'     , 'ltt'                ,'Bad iterator name'],
			];
		}
}
