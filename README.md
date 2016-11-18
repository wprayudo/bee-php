

# PHP driver for Bee

PHP driver for Bee


# Table of contents

* [Installing and building](#installing-and-building)
  * [Building from source](#building-from-source)
  * [Testing](#testing)
* [Configuration reference](#configuration-reference)
* [API reference](#api-reference)
    * [Predefined constants](#predefined-constants)
    * [Class Bee](#class-bee)
      * [Bee::\_\_construct](#bee__construct)
    * [Connection manipulations](#connection-manipulations)
      * [Bee::disconnect](#beedisconnect)
      * [Bee::flushSchema](#beeflushschema)
      * [Bee::ping](#beeping)
    * [Database queries](#database-queries)
      * [Bee::select](#beeselect)
      * [Bee::insert, Bee::replace](#beeinsert-beereplace)
      * [Bee::call](#beecall)
      * [Bee::evaluate](#beeevaluate)
      * [Bee::delete](#beedelete)
      * [Bee::update](#beeupdate)
      * [Bee::upsert](#beeupsert)
    * [Deprecated](#deprecated)

# Installing and building


To build and install the `bee-php` driver from source, you need:
* PHP version 5 (no less than 5.3.0, but strictly under 6.0.0);
* `php5-dev` package (the package should contain a utility named `phpize`);
* `php-pear` package.

For example, to build and install the driver on Ubuntu, the commands may be as
follows:

```sh
$ sudo apt-get install php5-cli
$ sudo apt-get install php5-dev
$ sudo apt-get install php-pear
$ cd ~
$ git clone https://github.com/wprayudo/bee-php.git
$ cd bee-php
$ phpize
$ ./configure
$ make
$ make install
```

At this point, there is a file named `~/bee-php/modules/bee.so`.
PHP will only find it if the PHP initialization file `php.ini` contains a line
like `extension=./bee.so`, or if PHP is started with the option
`-d extension=~/bee-php/modules/bee.so`.

## Testing

To run tests, the Bee server and PHP/PECL package are required.

```sh
$ ./test-run.py
```

The `test.run.py` program will automatically find and start Bee and 
then run `phpunit.phar` based tests.
If Bee is not defined in the `PATH` environment variable, you can 
define it in the `BEE_DB_PATH` environment variable.

```sh
$ BEE_DB_PATH=/path/to/bee/bin/bee ./test-run.py
```




# Configuration reference

In the configuration file, you can set the following parameters:

* `bee.persistent` - enable persistent connections (don't close 
connections between sessions) (default: True, **cannot be changed at 
runtime**);
* `bee.timeout` - connection timeout (default: 10.0 seconds, can be 
changed at runtime);
* `bee.retry_count` - count of retries for connecting (default: 1, 
can be changed at runtime);
* `bee.retry_sleep` - sleep between connecting retries (default: 
0.1 seconds, can be changed at runtime);
* `bee.request_timeout` - read/write timeout for requests 
(default: 10.0 seconds, can be changed at runtime).

# API reference

Inside this section:

* [Predefined constants](#predefined-constants)
* [Class Bee](#class-bee)
  * [Bee::__construct](#bee__construct)
* [Connection manipulations](#connection-manipulations)
  * [Bee::disconnect](#beedisconnect)
  * [Bee::flushSchema](#beeflushschema)
   * [Bee::ping](#beeping)
* [Database queries](#database-queries)
   * [Bee::select](bee#select)
   * [Bee::insert, replace](#beeinsert-beereplace)
   * [Bee::call](#beecall)
   * [Bee::evaluate](#beeevaluate)
   * [Bee::delete](#beedelete)
   * [Bee::update](#beeupdate)
   * [Bee::upsert](#beeupsert)
* [Deprecated](#deprecated)

## Predefined constants

_**Description**_: Available Bee constants.

* `Bee::ITERATOR_EQ` - "equality" iterator (ALL);
* `Bee::ITERATOR_REQ` - "reverse equality" iterator;
* `Bee::ITERATOR_ALL` - get all tuples;
* `Bee::ITERATOR_LT` - "less than" iterator;
* `Bee::ITERATOR_LE` - "less than or equal" iterator;
* `Bee::ITERATOR_GE` - "greater than or equal" iterator;
* `Bee::ITERATOR_GT` - "greater than" iterator;
* `Bee::ITERATOR_BITS_ALL_SET` - check if all given bits are set 
  (BITSET only);
* `Bee::ITERATOR_BITS_ANY_SET` - check if any given bits are set 
  (BITSET only);
* `Bee::ITERATOR_BITS_ALL_NOT_SET` - check if all given bits are 
  not set (BITSET only);
* `Bee::ITERATOR_OVERLAPS` - find points in an n-dimension cube 
  (RTREE only);
* `Bee::ITERATOR_NEIGHBOR` - find the nearest points (RTREE only).

## Class Bee

``` php
Bee {
     public Bee::__construct ( [ string $host = 'localhost' 
[, int $port = 3301 [, string $user = "guest" [, string $password = NULL 
[, string $persistent_id = NULL ] ] ] ] ] )
     public bool Bee::connect ( void )
     public bool Bee::disconnect ( void )
     public bool Bee::flushSchema ( void )
     public bool Bee::ping ( void )
     public array Bee::select (mixed $space [, mixed $key = 
array() [, mixed $index = 0 [, int $limit = PHP_INT_MAX [, int $offset = 
0 [, $iterator = Bee::ITERATOR_EQ ] ] ] ] ] )
     public array Bee::insert (mixed $space, array $tuple)
     public array Bee::replace (mixed $space, array $tuple)
     public array Bee::call (string $procedure [, mixed args] )
     public array Bee::evaluate (string $expression [, mixed args] )
     public array Bee::delete (mixed $space, mixed $key [, mixed 
$index] )
     public array Bee::update (mixed $space, mixed $key, array 
$ops [, number $index] )
     public array Bee::upsert (mixed $space, mixed $key, array 
$ops [, number $index] )
}
```

### Bee::__construct

```
public Bee::__construct ( [ string $host = 'localhost' [, int 
$port = 3301 [, string $user = "guest" [, string $password = NULL [, 
string $persistent_id = NULL ] ] ] ] ] )
```

_**Description**_: Creates a Bee client.

_**Parameters**_:

* `host`: string, default: `'localhost'`;
* `port`: number, default : `3301`;
* `user`: string, default: `'guest'`;
* `password`: string;
* `persistent_id`: string (if set, then the connection will be persistent,
   unless the `bee.persistent` parameter is set in the configuration file).

_**Return value**_: Bee class instance

### Example

``` php
$beex = new Bee(); // -> new Bee('localhost', 3301);
$beex = new Bee('bee.org'); // -> new 
Bee('bee.org', 3301);
$beex = new Bee('localhost', 16847);
```

## Connection manipulations

Notice that `Bee::connect`, `Bee::open` (an alias for `connect`) and
`Bee::reconnect` are deprecated as any other connection-related
instructions now cause an automatic connect.

To initiate and/or test connection, please use [Bee::ping](#beeping).

### Bee::disconnect

``` php
public bool Bee::disconnect ( void )
```

_**Description**_: Explicitly close a connection to the Bee server.
If persistent connections are in use, then the connection will be saved in 
the connection pool.
You can also use an alias for this method, `Bee::close`.

_**Return value**_: **BOOL**: True

### Bee::flushSchema

``` php
public bool Bee::flushSchema ( void )
```

_**Description**_: Remove the cached space/index schema that was queried from
the client.

_**Return value**_: **BOOL**: True

### Bee::ping

``` php
public bool Bee::ping ( void )
```

_**Description**_: Ping the Bee server. Using `ping` is also the
recommended way to initiate and/or test a connection.

_**Return value**_: **BOOL**: True and raises `Exception` on error.

## Database queries

### Bee::select

``` php
public array Bee::select(mixed $space [, mixed $key = array() [, 
mixed $index = 0 [, int $limit = PHP_INT_MAX [, int $offset = 0 [, 
$iterator = Bee::ITERATOR_EQ ] ] ] ] ] )
```

_**Description**_: Execute a select query on the Bee server.

_**Parameters**_:

* `space`: String/Number, Space ID to select from (mandatory);
* `key`: String/Number or Array, key to select (default: `Array()` i.e.
   an empty array which selects everything in the space);
* `index`: String/Number, Index ID to select from (default: 0);
* `limit`: Number, the maximum number of tuples to return (default: INT_MAX,
  a large value);
* `offset`: Number, offset to select from (default: 0);
* `iterator`: Constant, iterator type.
  See [Predefined constants](#predefined-constants) for more information
  (default: `Bee::ITERATOR_EQ`).
  You can also use strings `'eq'`, `'req'`, `'all'`, `'lt'`, `'le'`, `'ge'`,
  `'gt'`, `'bits_all_set'`, `'bits_any_set'`, `'bits_all_not_set'`,
  `'overlaps'`, `'neighbor'`, `'bits_all_set'`, `'bits_any_set'`, 
  `'bits_all_not_set'` (in either lowercase or uppercase) instead of constants.

_**Return value**_:

* **Array of arrays**: in case of success - a list of tuples that satisfy the
  request, or an empty array if nothing was found;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
// Selects everything from space 'test'
$beex->select("test");
// Selects from space 'test' by primary key with id == 1
$beex->select("test", 1);
// Same effect as the previous statement
$beex->select("test", array(1));
// Selects from space 'test' by secondary key from index 'isec' and == 
{1, 'hello'}
$beex->select("test", array(1, "hello"), "isec");
// Selects 100 tuples from space 'test' after skipping 100 tuples
$beex->select("test", null, null, 100, 100);
// Selects 100 tuples from space 'test' after skipping 100 tuples,
// in reverse equality order.
// Reverse searching goes backward from index end, so this means:
// select penultimate hundred tuples.
$beex->select("test", null, null, 100, 100, Bee::ITERATOR_REQ);
```

### Bee::insert, Bee::replace

``` php
public array Bee::insert(mixed $space, array $tuple)
public array Bee::replace(mixed $space, array $tuple)
```

_**Description**_: Insert (if there is no tuple with the same primary key) or 
Replace tuple.

_**Parameters**_:

* `space`: String/Number, Space ID to select from (mandatory);
* `tuple`: Array, Tuple to Insert/Replace (mandatory).

_**Return value**_:

* **Array** in case of success - the tuple that was inserted;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
// This will succeed, because no tuples with primary key == 1 are in space 'test'.
$beex->insert("test", array(1, 2, "something"));
// This will fail, because we have just inserted a tuple with primary key == 1.
// The error will be ER_TUPLE_FOUND.
$beex->insert("test", array(1, 3, "something completely different"));
// This will succeed, because Replace has no problem with duplicate keys.
$beex->replace("test", array(1, 3, "something completely different"));
```

### Bee::call

``` php
public array Bee::call(string $procedure [, mixed args])
```

_**Description**_: Call a stored procedure.

_**Parameters**_:

* `procedure`: String, procedure to call (mandatory);
* `args`: Any value to pass to the procedure as arguments (default: empty).

_**Return value**_:

* **Array of arrays** in case of success - tuples that were returned by the
  procedure;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
$beex->call("test_2");
$beex->call("test_3", array(3, 4));
```

### Bee::evaluate

``` php
public array Bee::evaluate(string $expression [, mixed args])
```

_**Description**_: Evaluate the Lua code in $expression. The current 
user must have the `'execute'` privilege on `'universe'` in Bee.

_**Parameters**_:

* `expression`: String, Lua code to evaluate (mandatory);
* `args`: Any value to pass to the procedure as arguments (default: empty).

_**Return value**_:

* **Any value** that was returned from the evaluated code;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
$beex->evaluate("return test_2()");
$beex->evaluate("return test_3(...)", array(3, 4));
```

### Bee::delete

``` php
public array Bee::delete(mixed $space, mixed $key [, mixed $index])
```

_**Description**_: Delete a tuple with a given key.

_**Parameters**_:

* `space`: String/Number, Space ID to delete from (mandatory);
* `key`: String/Number or Array, key of the tuple to be deleted (mandatory);
* `index`: String/Number, Index ID to delete from (default: 0).

_**Return value**_:

* **Array** in case of success - the tuple that was deleted;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
// The following code will delete all tuples from space `test`
$tuples = $beex->select("test");
foreach($tuples as $value) {
     $beex->delete("test", array($value[0]));
}
```

### Bee::update

``` php
public array Bee::update(mixed $space, mixed $key, array $ops [, 
number $index] )
```

_**Description**_: Update a tuple with a given key (in Bee, an update
can apply multiple operations to a tuple).

_**Parameters**_:

* `space`: String/Number, Space ID to select from (mandatory);
* `key`: Array/Scalar, Key to match the tuple with (mandatory);
* `ops`: Array of Arrays, Operations to execute if the tuple was found.

_**Operations**_:

`<serializable>` - any simple type which converts to MsgPack (scalar/array).

* Splice operation - take `field`'th field, replace `length` bytes from 
`offset` byte with 'list':
   ```
   array(
     "field" => <number>,
     "op" => ":",
     "offset"=> <number>,
     "length"=> <number>,
     "list" => <string>
   ),
   ```
* Numeric operations:
   ```
   array(
     "field" => <number>,
     "op" => ("+"|"-"|"&"|"^"|"|"),
     "arg" => <number>
   ),
   ```
   - "+" for addition
   - "-" for subtraction
   - "&" for bitwise AND
   - "^" for bitwise XOR
   - "|" for bitwise OR
* Delete `arg` fields from 'field':
   ```
   array(
     "field" => <number>,
     "op" => "#",
     "arg" => <number>
   )
   ```
* Replace/Insert before operations:
   ```
   array(
     "field" => <number>,
     "op" => ("="|"!"),
     "arg" => <serializable>
   )
   ```
   - "=" replace `field`'th field with 'arg'
   - "=" insert 'arg' before `field`'th field

```
array(
   array(
     "field" => <number>,
     "op" => ":",
     "offset"=> <number>,
     "length"=> <number>,
     "list" => <string>
   ),
   array(
     "field" => <number>,
     "op" => ("+"|"-"|"&"|"^"|"|"),
     "arg" => <number>
   ),
   array(
     "field" => <number>,
     "op" => "#",
     "arg" => <number>
   ),
   array(
     "field" => <number>,
     "op" => ("="|"!"),
     "arg" => <serializable>
   )
)
```

_**Return value**_:

* **Array** in case of success - the tuple after it was updated;
* **BOOL**: False and raises `Exception` on error.

#### Example

``` php
$beex->update("test", 1, array(
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
     "arg" => 0x11111,
   ),
));
$beex->update("test", 1, array(
   array(
     "field" => 3,
     "op" => "-",
     "arg" => 10
   ),
   array(
     "field" => 4,
     "op" => "&",
     "arg" => 0x10101,
   )
));
$beex->update("test", 1, array(
   array(
     "field" => 4,
     "op" => "^",
     "arg" => 0x11100,
   )
));
$beex->update("test", 1, array(
   array(
     "field" => 4,
     "op" => "|",
     "arg" => 0x00010,
   )
));
$beex->update("test", 1, array(
   array(
     "field" => 2,
     "op" => ":",
     "offset" => 2,
     "length" => 2,
     "list" => "rrance and phillipe show"
   )
));
```

### Bee::upsert

``` php
public array Bee::upsert(mixed $space, array $tuple, array $ops [, 
number $index] )
```

_**Description**_: Update or Insert command (if a tuple with primary key 
== PK('tuple') exists, then the tuple will be updated with 'ops', otherwise
'tuple' will be inserted).

_**Parameters**_:

* `space`: String/Number, Space ID to select from (mandatory);
* `tuple`: Array, Tuple to Insert (mandatory);
* `ops`: Array of Arrays, Operations to execute if tuple was found. 
  Operations are described earlier, in [Bee::update](#beeupdate).

_**Return value**_:


* **BOOL**: False and raises `Exception` on error (in some cases).

#### Example

``` php
$beex->upsert("test", array(124, 10, "new tuple"), array(
   array(
     "field" => 1,
     "op" => "+",
     "arg" => 10
   )
));
```

## Deprecated

* Global constants, e.g. `BEE_ITER_<name>`;
* `Bee::authenticate` method, please provide credentials in the
  constructor instead;
* `Bee::connect`, `Bee::open` (an alias for `connect`) and
  `Bee::reconnect` methods as any other connection-related instructions 
  now cause an automatic connect;
* `Bee::eval` method, please use the `evaluate` method instead;
* `Bee::flush_schema` method, deprecated in favor of `flushSchema`;
* configuration parameter `bee.con_per_host`, deprecated and removed.
