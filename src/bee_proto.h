#ifndef PHP_TP_H
#define PHP_TP_H

#define SALT64_SIZE       44
#define SALT_SIZE         64
#define PHP_SCRAMBLE_SIZE 20
#define GREETING_SIZE     128
#define SALT_PREFIX_SIZE  64

#define SPACE_SPACE 281
#define SPACE_INDEX 289

#define INDEX_SPACE_NAME    2
#define INDEX_INDEX_NAME    2

#include <stdint.h>

#include <php_bee.h>

/* header */
enum beex_header_key_t {
	BEEX_CODE      = 0x00,
	BEEX_SYNC      = 0x01,
	BEEX_SCHEMA_ID = 0x05
};

/* request body */
enum beex_body_key_t {
	BEEX_SPACE      = 0x10,
	BEEX_INDEX      = 0x11,
	BEEX_LIMIT      = 0x12,
	BEEX_OFFSET     = 0x13,
	BEEX_ITERATOR   = 0x14,
	BEEX_KEY        = 0x20,
	BEEX_TUPLE      = 0x21,
	BEEX_FUNCTION   = 0x22,
	BEEX_USERNAME   = 0x23,
	BEEX_EXPRESSION = 0x27,
	BEEX_OPS        = 0x28
};

/* response body */
enum beex_response_key_t {
	BEEX_DATA  = 0x30,
	BEEX_ERROR = 0x31
};

/* request types */
enum beex_request_type {
	BEEX_OK      = 0x00,
	BEEX_SELECT  = 0x01,
	BEEX_INSERT  = 0x02,
	BEEX_REPLACE = 0x03,
	BEEX_UPDATE  = 0x04,
	BEEX_DELETE  = 0x05,
	BEEX_CALL    = 0x06,
	BEEX_AUTH    = 0x07,
	BEEX_EVAL    = 0x08,
	BEEX_UPSERT  = 0x09,
	BEEX_PING    = 0x40
};

enum beex_iterator_type {
	ITERATOR_EQ                 = 0,
	ITERATOR_REQ                = 1,
	ITERATOR_ALL                = 2,
	ITERATOR_LT                 = 3,
	ITERATOR_LE                 = 4,
	ITERATOR_GE                 = 5,
	ITERATOR_GT                 = 6,
	ITERATOR_BITS_ALL_SET       = 7,
	ITERATOR_BITSET_ALL_SET     = 7,
	ITERATOR_BITS_ANY_SET       = 8,
	ITERATOR_BITSET_ANY_SET     = 8,
	ITERATOR_BITS_ALL_NOT_SET   = 9,
	ITERATOR_BITSET_ALL_NOT_SET = 9,
	ITERATOR_OVERLAPS           = 10,
	ITERATOR_NEIGHBOR           = 11,
};

struct beex_response {
	uint64_t    bitmap;
	const char *buf;
	uint32_t    code;
	uint32_t    sync;
	const char *error;
	size_t      error_len;
	const char *data;
	size_t      data_len;
};

int64_t php_tp_response(struct beex_response *r, char *buf, size_t size);

void php_tp_encode_auth(smart_string *str, uint32_t sync, char * const username,
			size_t username_len, char * const scramble);
void php_tp_encode_ping(smart_string *str, uint32_t sync);
void php_tp_encode_select(smart_string *str, uint32_t sync, uint32_t space_no,
			  uint32_t index_no, uint32_t limit, uint32_t offset,
			  uint32_t iterator, zval *key);
void php_tp_encode_insert_or_replace(smart_string *str, uint32_t sync,
				     uint32_t space_no, zval *tuple,
				     uint32_t type);
void php_tp_encode_delete(smart_string *str, uint32_t sync, uint32_t space_no,
			  uint32_t index_no, zval *tuple);
void php_tp_encode_call(smart_string *str, uint32_t sync, char *proc,
			uint32_t proc_len, zval *tuple);
void php_tp_encode_eval(smart_string *str, uint32_t sync, char *proc,
			uint32_t proc_len, zval *tuple);
void php_tp_encode_update(smart_string *str, uint32_t sync, uint32_t space_no,
			  uint32_t index_no, zval *key, zval *args);
void php_tp_encode_upsert(smart_string *str, uint32_t sync, uint32_t space_no,
			  zval *tuple, zval *args);
#endif /* PHP_TP_H */
