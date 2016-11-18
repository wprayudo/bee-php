struct schema_key {
	const char *id;
	uint32_t id_len;
};
/*
enum field_type {
	FT_STR = 0,
	FT_NUM,
	FT_OTHER
};

struct schema_field_value {
	uint32_t        field_number;
	char           *field_name;
	uint32_t        field_name_len;
	enum field_type field_type;
};
*/

struct schema_index_value {
	struct schema_key key;
	char      *index_name;
	uint32_t   index_name_len;
	uint32_t   index_number;
//	struct schema_field_value *index_parts;
//	uint32_t   index_parts_len;
};

struct mh_schema_index_t;

struct schema_space_value {
	struct schema_key key;
	char      *space_name;
	uint32_t   space_name_len;
	uint32_t   space_number;
	struct mh_schema_index_t  *index_hash;
//	struct schema_field_value *schema_list;
//	uint32_t   schema_list_len;
};

struct mh_schema_space_t;

struct bee_schema {
	struct mh_schema_space_t *space_hash;
};

int bee_schema_add_spaces(struct bee_schema *, const char *, uint32_t);
int bee_schema_add_indexes(struct bee_schema *, const char *, uint32_t);

int32_t bee_schema_get_sid_by_string(struct bee_schema *, const char *, uint32_t);
int32_t bee_schema_get_iid_by_string(struct bee_schema *, uint32_t, const char *, uint32_t);

struct bee_schema *bee_schema_new(int is_persistent);
void bee_schema_flush (struct bee_schema *);
void bee_schema_delete(struct bee_schema *, int is_persistent);
