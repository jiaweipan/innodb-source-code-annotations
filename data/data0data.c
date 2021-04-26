/************************************************************************
SQL data field and tuple

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/
/*SQL数据字段和元组*/
#include "data0data.h"

#ifdef UNIV_NONINL
#include "data0data.ic"
#endif

#include "ut0rnd.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "page0page.h"
#include "dict0dict.h"
#include "btr0cur.h"

byte	data_error;	/* data pointers of tuple fields are initialized
			to point here for error checking */ /*元组字段的数据指针被初始化为指向这里以进行错误检查*/

ulint	data_dummy;	/* this is used to fool the compiler in
			dtuple_validate *//*这是用来欺骗dtuple_validate中的编译器的*/

byte	data_buf[8192];	/* used in generating test tuples */ /*用于生成测试元组*/
ulint	data_rnd = 756511;


/* Some non-inlined functions used in the MySQL interface: */ /*MySQL接口中使用的一些非内联函数:*/
void 
dfield_set_data_noninline(
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len)	/* in: length or UNIV_SQL_NULL */
{
	dfield_set_data(field, data, len);
}
void* 
dfield_get_data_noninline(
	dfield_t* field)	/* in: field */
{
	return(dfield_get_data(field));
}
ulint
dfield_get_len_noninline(
	dfield_t* field)	/* in: field */
{
	return(dfield_get_len(field));
}
ulint 
dtuple_get_n_fields_noninline(
	dtuple_t* 	tuple)	/* in: tuple */
{
	return(dtuple_get_n_fields(tuple));
}
dfield_t* 
dtuple_get_nth_field_noninline(
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n)	/* in: index of field */
{
	return(dtuple_get_nth_field(tuple, n));
}

/****************************************************************
Returns TRUE if lengths of two dtuples are equal and respective data fields
in them are equal when compared with collation in char fields (not as binary
strings). */
/*如果两个dtuple的长度相等，并且它们各自的数据字段在与char字段(而不是二进制字符串)的排序时相等，则返回TRUE。*/
ibool
dtuple_datas_are_ordering_equal(
/*============================*/
				/* out: TRUE if length and fieds are equal
				when compared with cmp_data_data:
				NOTE: in character type fields some letters
				are identified with others! (collation) */ /*当与cmp_data_data比较时，如果长度和字段相等，
				则为TRUE:注意:在字符类型字段中，一些字母与其他字母相识别!(排序)*/
	dtuple_t*	tuple1,	/* in: tuple 1 */
	dtuple_t*	tuple2)	/* in: tuple 2 */
{
	dfield_t*	field1;
	dfield_t*	field2;
	ulint		n_fields;
	ulint		i;

	ut_ad(tuple1 && tuple2);
	ut_ad(tuple1->magic_n = DATA_TUPLE_MAGIC_N);
	ut_ad(tuple2->magic_n = DATA_TUPLE_MAGIC_N);
	ut_ad(dtuple_check_typed(tuple1));
	ut_ad(dtuple_check_typed(tuple2));

	n_fields = dtuple_get_n_fields(tuple1);

	if (n_fields != dtuple_get_n_fields(tuple2)) {

		return(FALSE);
	}
	
	for (i = 0; i < n_fields; i++) {

		field1 = dtuple_get_nth_field(tuple1, i);
		field2 = dtuple_get_nth_field(tuple2, i);

		if (0 != cmp_dfield_dfield(field1, field2)) {
		
			return(FALSE);
		}			
	}
	
	return(TRUE);
}

/*************************************************************************
Creates a dtuple for use in MySQL. */
/*创建一个dtuple在MySQL中使用。*/
dtuple_t*
dtuple_create_for_mysql(
/*====================*/
				/* out, own created dtuple */
	void** 	heap,    	/* out: created memory heap */
	ulint 	n_fields) 	/* in: number of fields */
{
  	*heap = (void*)mem_heap_create(500);
 
  	return(dtuple_create(*((mem_heap_t**)heap), n_fields));  
}

/*************************************************************************
Frees a dtuple used in MySQL. */
/*释放MySQL中使用的dtuple。*/
void
dtuple_free_for_mysql(
/*==================*/
	void*	heap) /* in: memory heap where tuple was created */
{
  	mem_heap_free((mem_heap_t*)heap);
}

/*************************************************************************
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */ 
/*设置元组中使用的字段数。通常这是在dtuple_create中设置的，但是如果您想稍后将它设置得更小，可以使用它。*/
void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields)	/* in: number of fields */
{
	ut_ad(tuple);

	tuple->n_fields = n_fields;
	tuple->n_fields_cmp = n_fields;
}

/**************************************************************
Checks that a data field is typed. Asserts an error if not. */
/*检查数据字段是否已输入。如果不是，则断言错误。*/
ibool
dfield_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dfield_t*	field)	/* in: data field */
{
	ut_a(dfield_get_type(field)->mtype <= DATA_MYSQL);
	ut_a(dfield_get_type(field)->mtype >= DATA_VARCHAR);

	return(TRUE);
}

/**************************************************************
Checks that a data tuple is typed. Asserts an error if not. */
/*检查数据元组是否有类型。如果不是，则断言错误。*/
ibool
dtuple_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint	 	i;

	for (i = 0; i < dtuple_get_n_fields(tuple); i++) {

		field = dtuple_get_nth_field(tuple, i);

		ut_a(dfield_check_typed(field));
	}

	return(TRUE);
}

/**************************************************************
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set. */
/*验证元组的一致性，必须是完整的，即所有字段必须已设置。*/
ibool
dtuple_validate(
/*============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	byte*	 	data;
	ulint	 	n_fields;
	ulint	 	len;
	ulint	 	i;
	ulint	 	j;

	ut_a(tuple->magic_n = DATA_TUPLE_MAGIC_N);

	n_fields = dtuple_get_n_fields(tuple);

	/* We dereference all the data of each field to test
	for memory traps */

	for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(tuple, i);
		len = dfield_get_len(field);
	
		if (len != UNIV_SQL_NULL) {

			data = field->data;

			for (j = 0; j < len; j++) {

				data_dummy  += *data; /* fool the compiler not
							to optimize out this
							code */
				data++;
			}
		}
	}

	ut_a(dtuple_check_typed(tuple));

	return(TRUE);
}

/*****************************************************************
Pretty prints a dfield value according to its data type. */
/*Pretty根据数据类型打印一个dfield值。*/
void
dfield_print(
/*=========*/
	dfield_t*	dfield)	 /* in: dfield */
{
	byte*	data;
	ulint	len;
	ulint	mtype;
	ulint	i;

	len = dfield_get_len(dfield);
	data = dfield_get_data(dfield);

	if (len == UNIV_SQL_NULL) {
		printf("NULL");

		return;
	}

	mtype = dtype_get_mtype(dfield_get_type(dfield));

	if ((mtype == DATA_CHAR) || (mtype == DATA_VARCHAR)) {
	
		for (i = 0; i < len; i++) {

			if (isprint((char)(*data))) {
				printf("%c", (char)*data);
			} else {
				printf(" ");
			}

			data++;
		}
	} else if (mtype == DATA_INT) {
		ut_a(len == 4); /* only works for 32-bit integers */
		printf("%i", (int)mach_read_from_4(data));
	} else {
		ut_error;
	}
}

/*****************************************************************
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */ 
/*Pretty根据数据类型打印一个dfield值。如果字符串包含不可打印字符，则输出十六进制字符串。*/
void
dfield_print_also_hex(
/*==================*/
	dfield_t*	dfield)	 /* in: dfield */
{
	byte*	data;
	ulint	len;
	ulint	mtype;
	ulint	i;
	ibool	print_also_hex;

	len = dfield_get_len(dfield);
	data = dfield_get_data(dfield);

	if (len == UNIV_SQL_NULL) {
		printf("NULL");

		return;
	}

	mtype = dtype_get_mtype(dfield_get_type(dfield));

	if ((mtype == DATA_CHAR) || (mtype == DATA_VARCHAR)) {

		print_also_hex = FALSE;
	
		for (i = 0; i < len; i++) {

			if (isprint((char)(*data))) {
				printf("%c", (char)*data);
			} else {
				print_also_hex = TRUE;
				printf(" ");
			}

			data++;
		}

		if (!print_also_hex) {

			return;
		}

		printf(" Hex: ");
		
		data = dfield_get_data(dfield);
		
		for (i = 0; i < len; i++) {
			printf("%02lx", (ulint)*data);

			data++;
		}
	} else if (mtype == DATA_INT) {
		ut_a(len == 4); /* inly works for 32-bit integers */
		printf("%i", (int)mach_read_from_4(data));
	} else {
		ut_error;
	}
}

/**************************************************************
The following function prints the contents of a tuple. */
/*下面的函数输出元组的内容。*/
void
dtuple_print(
/*=========*/
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint		n_fields;
	ulint		i;

	n_fields = dtuple_get_n_fields(tuple);

	printf("DATA TUPLE: %lu fields;\n", n_fields);

	for (i = 0; i < n_fields; i++) {
		printf(" %lu:", i);	

		field = dtuple_get_nth_field(tuple, i);
		
		if (field->len != UNIV_SQL_NULL) {
			ut_print_buf(field->data, field->len);
		} else {
			printf(" SQL NULL");
		}

		printf(";");
	}

	printf("\n");

	dtuple_validate(tuple);
}

/**************************************************************
The following function prints the contents of a tuple to a buffer. */
/*下面的函数将元组的内容打印到缓冲区。*/
ulint
dtuple_sprintf(
/*===========*/
				/* out: printed length in bytes */
	char*		buf,	/* in: print buffer */
	ulint		buf_len,/* in: buf length in bytes */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint		n_fields;
	ulint		len;
	ulint		i;

	len = 0;

	n_fields = dtuple_get_n_fields(tuple);

	for (i = 0; i < n_fields; i++) {
		if (len + 30 > buf_len) {

			return(len);
		}

		len += sprintf(buf + len, " %lu:", i);	

		field = dtuple_get_nth_field(tuple, i);
		
		if (field->len != UNIV_SQL_NULL) {
			if (5 * field->len + len + 30 > buf_len) {

				return(len);
			}
		
			len += ut_sprintf_buf(buf + len, field->data,
								field->len);
		} else {
			len += sprintf(buf + len, " SQL NULL");
		}

		len += sprintf(buf + len, ";");
	}

	return(len);
}

/******************************************************************
Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index. */
/*将长字段的一部分移动到大记录向量中，使元组的大小低于数据库中允许的最大记录大小。
仅从那些对确定元组在索引中的插入位置没有必要的字段中移动数据。*/
big_rec_t*
dtuple_convert_big_rec(
/*===================*/
				/* out, own: created big record vector,
				NULL if we are not able to shorten
				the entry enough, i.e., if there are
				too many short fields in entry */ 
				/*创建了一个大记录向量，如果我们不能足够缩短条目，即如果条目中有太多短字段，则为NULL*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry */
	ulint*		ext_vec,/* in: array of externally stored fields,
				or NULL: if a field already is externally
				stored, then we cannot move it to the vector
				this function returns *//*如果一个字段已经被外部存储，那么我们不能将它移到该函数返回的向量中*/
	ulint		n_ext_vec)/* in: number of elements is ext_vec */
{
	mem_heap_t*	heap;
	big_rec_t*	vector;
	dfield_t*	dfield;
	ulint		size;
	ulint		n_fields;
	ulint		longest;
	ulint		longest_i		= ULINT_MAX;
	ibool		is_externally_stored;
	ulint		i;
	ulint		j;
	
	size = rec_get_converted_size(entry);

	heap = mem_heap_create(size + dtuple_get_n_fields(entry)
					* sizeof(big_rec_field_t) + 1000);

	vector = mem_heap_alloc(heap, sizeof(big_rec_t));

	vector->heap = heap;
	vector->fields = mem_heap_alloc(heap, dtuple_get_n_fields(entry)
					* sizeof(big_rec_field_t));

	/* Decide which fields to shorten: the algorithm is to look for
	the longest field which does not occur in the ordering part
	of any index on the table */
	/*决定缩短哪个字段:算法是寻找最长的字段，没有出现在表上任何索引的排序部分*/
	n_fields = 0;

	while ((rec_get_converted_size(entry)
					>= page_get_free_space_of_empty() / 2)
	       || rec_get_converted_size(entry) >= REC_MAX_DATA_SIZE) {

		longest = 0;
		for (i = dict_index_get_n_unique_in_tree(index);
				i < dtuple_get_n_fields(entry); i++) {

			/* Skip over fields which already are externally
			stored */
			/*跳过已经在外部存储的字段*/
			is_externally_stored = FALSE;

			if (ext_vec) {
				for (j = 0; j < n_ext_vec; j++) {
					if (ext_vec[j] == i) {
						is_externally_stored = TRUE;
					}
				}
			}
				
			/* Skip over fields which are ordering in some index */
			/* 跳过已经在外部存储的字段*/
			if (!is_externally_stored &&
			    dict_field_get_col(
			    	dict_index_get_nth_field(index, i))
			    ->ord_part == 0) {

				dfield = dtuple_get_nth_field(entry, i);

				if (dfield->len != UNIV_SQL_NULL &&
			        		dfield->len > longest) {

			        	longest = dfield->len;

			        	longest_i = i;
				}
			}
		}
	
		if (longest < BTR_EXTERN_FIELD_REF_SIZE + 10
						+ REC_1BYTE_OFFS_LIMIT) {

			/* Cannot shorten more */

			mem_heap_free(heap);

			return(NULL);
		}

		/* Move data from field longest_i to big rec vector;
		we do not let data size of the remaining entry
		drop below 128 which is the limit for the 2-byte
		offset storage format in a physical record. This
		we accomplish by storing 128 bytes of data in entry
		itself, and only the remaining part to big rec vec. */
		/*将数据从字段longest_i移动到大的矩形向量;我们不让剩下的条目的数据大小低于128，
		这是一个物理记录的2字节偏移存储格式的限制。我们通过在条目本身中存储128字节的数据来实现这一点，只将剩下的部分留给big rec vec。*/
		dfield = dtuple_get_nth_field(entry, longest_i);
		vector->fields[n_fields].field_no = longest_i;

		vector->fields[n_fields].len = dfield->len
						- REC_1BYTE_OFFS_LIMIT;

		vector->fields[n_fields].data = mem_heap_alloc(heap,
						vector->fields[n_fields].len);

		/* Copy data (from the end of field) to big rec vector */
		/*将数据（从字段末尾）复制到大rec向量*/
		ut_memcpy(vector->fields[n_fields].data,
				((byte*)dfield->data) + dfield->len
						- vector->fields[n_fields].len,
				vector->fields[n_fields].len);
		dfield->len = dfield->len - vector->fields[n_fields].len
						+ BTR_EXTERN_FIELD_REF_SIZE;

		/* Set the extern field reference in dfield to zero */ /*将dfield中的extern字段引用设置为零*/
		memset(((byte*)dfield->data)
			+ dfield->len - BTR_EXTERN_FIELD_REF_SIZE,
					0, BTR_EXTERN_FIELD_REF_SIZE);
		n_fields++;
	}	

	vector->n_fields = n_fields;
	return(vector);
}

/******************************************************************
Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec. */
/*将存储在向量中的数据放回条目。注意，为了确保entry中的字段可以容纳数据，vector必须使用dtuple_convert_big_rec从entry创建。*/
void
dtuple_convert_back_big_rec(
/*========================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: entry whose data was put to vector */
	big_rec_t*	vector)	/* in, own: big rec vector; it is
				freed in this function */
{
	dfield_t*	dfield;
	ulint		i;	

	for (i = 0; i < vector->n_fields; i++) {
	
		dfield = dtuple_get_nth_field(entry,
						vector->fields[i].field_no);
		/* Copy data from big rec vector */

		ut_memcpy(((byte*)dfield->data)
				+ dfield->len - BTR_EXTERN_FIELD_REF_SIZE,
			  vector->fields[i].data,
		          vector->fields[i].len);
		dfield->len = dfield->len + vector->fields[i].len
						- BTR_EXTERN_FIELD_REF_SIZE;
	}	

	mem_heap_free(vector->heap);
}

/******************************************************************
Frees the memory in a big rec vector. */
/*释放一个大的矩形向量中的内存。*/
void
dtuple_big_rec_free(
/*================*/
	big_rec_t*	vector)	/* in, own: big rec vector; it is
				freed in this function */
{
	mem_heap_free(vector->heap);
}
				
#ifdef notdefined

/******************************************************************
Generates random numbers, where 10/16 is uniformly
distributed between 0 and n1, 5/16 between 0 and n2,
and 1/16 between 0 and n3. */
/*生成随机数，其中10/16在0和n1之间均匀分布，5/16在0和n2之间均匀分布，1/16在0和n3之间均匀分布。*/
static
ulint
dtuple_gen_rnd_ulint(
/*=================*/
			/* out: random ulint */
	ulint	n1,	
	ulint	n2,
	ulint	n3)
{
	ulint 	m;
	ulint	n;

	m = ut_rnd_gen_ulint() % 16;
	
	if (m < 10) {
		n = n1;
	} else if (m < 15) {
		n = n2;
	} else {
		n = n3;
	}
	
	m = ut_rnd_gen_ulint();

	return(m % n);
}

/***************************************************************
Generates a random tuple. */
/*生成一个随机元组。*/
dtuple_t*
dtuple_gen_rnd_tuple(
/*=================*/
				/* out: pointer to the tuple */
	mem_heap_t*	heap)	/* in: memory heap where generated */
{
	ulint		n_fields;
	dfield_t*	field;
	ulint		len;
	dtuple_t*	tuple;	
	ulint		i;
	ulint		j;
	byte*		ptr;

	n_fields = dtuple_gen_rnd_ulint(5, 30, 300) + 1;

	tuple = dtuple_create(heap, n_fields);

	for (i = 0; i < n_fields; i++) {

		if (n_fields < 7) {
			len = dtuple_gen_rnd_ulint(5, 30, 400);
		} else {
			len = dtuple_gen_rnd_ulint(7, 5, 17);
		}

		field = dtuple_get_nth_field(tuple, i);
		
		if (len == 0) {
			dfield_set_data(field, NULL, UNIV_SQL_NULL);
		} else {
			ptr = mem_heap_alloc(heap, len);
			dfield_set_data(field, ptr, len - 1);

			for (j = 0; j < len; j++) {
				*ptr = (byte)(65 + 
					dtuple_gen_rnd_ulint(22, 22, 22));
				ptr++;
			}
		}

		dtype_set(dfield_get_type(field), DATA_VARCHAR,
							DATA_ENGLISH, 500, 0);
	}

	ut_a(dtuple_validate(tuple));

	return(tuple);
}

/*******************************************************************
Generates a test tuple for sort and comparison tests. */
/*为排序和比较测试生成测试元组。*/
void
dtuple_gen_test_tuple(
/*==================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i)	/* in: a number < 512 */
{
	ulint		j;
	dfield_t*	field;
	void*		data	= NULL;
	ulint		len	= 0;

	for (j = 0; j < 3; j++) {
		switch (i % 8) {
			case 0:
				data = ""; len = 0; break;
			case 1:
				data = "A"; len = 1; break;
			case 2:
				data = "AA"; len = 2; break;
			case 3:
				data = "AB"; len = 2; break;
			case 4:
				data = "B"; len = 1; break;
			case 5:
				data = "BA"; len = 2; break;
			case 6:
				data = "BB"; len = 2; break;
			case 7:
				len = UNIV_SQL_NULL; break;
		}

		field = dtuple_get_nth_field(tuple, 2 - j);
		
		dfield_set_data(field, data, len);
		dtype_set(dfield_get_type(field), DATA_VARCHAR,
				DATA_ENGLISH, 100, 0);
		
		i = i / 8;
	}
	
	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_test_tuple3(
/*===================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 1000000 */
	ulint		type,	/* in: DTUPLE_TEST_FIXED30, ... */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		third_size;

	ut_ad(tuple && buf);
	ut_ad(i < 1000000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000000");

	buf[1] = (byte)('0' + (i / 100000) % 10);
	buf[2] = (byte)('0' + (i / 10000) % 10);
	buf[3] = (byte)('0' + (i / 1000) % 10);
	buf[4] = (byte)('0' + (i / 100) % 10);
	buf[5] = (byte)('0' + (i / 10) % 10);
	buf[6] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);

	i = i % 1000; /* ut_rnd_gen_ulint() % 1000000; */

	ut_strcpy((char*)buf + 8, "0000000");

	buf[9] = (byte)('0' + (i / 100000) % 10);
	buf[10] = (byte)('0' + (i / 10000) % 10);
	buf[11] = (byte)('0' + (i / 1000) % 10);
	buf[12] = (byte)('0' + (i / 100) % 10);
	buf[13] = (byte)('0' + (i / 10) % 10);
	buf[14] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf + 8, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 2);

	data_rnd += 8757651;

	if (type == DTUPLE_TEST_FIXED30) {
		third_size = 30;
	} else if (type == DTUPLE_TEST_RND30) {
		third_size = data_rnd % 30;
	} else if (type == DTUPLE_TEST_RND3500) {
		third_size = data_rnd % 3500;
	} else if (type == DTUPLE_TEST_FIXED2000) {
		third_size = 2000;
	} else if (type == DTUPLE_TEST_FIXED3) {
		third_size = 3;
	} else {
		ut_error;
	}
	
	if (type == DTUPLE_TEST_FIXED30) {
		dfield_set_data(field,
			"12345678901234567890123456789", third_size);
	} else {
		dfield_set_data(field, data_buf, third_size);
	}
	
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_search_tuple3(
/*=====================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 or 2 fields */
	ulint		i,	/* in: a number < 1000000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 1000000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000000");

	buf[1] = (byte)('0' + (i / 100000) % 10);
	buf[2] = (byte)('0' + (i / 10000) % 10);
	buf[3] = (byte)('0' + (i / 1000) % 10);
	buf[4] = (byte)('0' + (i / 100) % 10);
	buf[5] = (byte)('0' + (i / 10) % 10);
	buf[6] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	if (dtuple_get_n_fields(tuple) == 1) {

		return;
	}

	field = dtuple_get_nth_field(tuple, 1);

	i = (i * 1000) % 1000000;

	ut_strcpy((char*)buf + 8, "0000000");

	buf[9] = (byte)('0' + (i / 100000) % 10);
	buf[10] = (byte)('0' + (i / 10000) % 10);
	buf[11] = (byte)('0' + (i / 1000) % 10);
	buf[12] = (byte)('0' + (i / 100) % 10);
	buf[13] = (byte)('0' + (i / 10) % 10);
	buf[14] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf + 8, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for TPC-A speed test. */
/*为TPC-A速度测试生成测试元组。*/
void
dtuple_gen_test_tuple_TPC_A(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		third_size;

	ut_ad(tuple && buf);
	ut_ad(i < 10000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000");

	buf[0] = (byte)('0' + (i / 1000) % 10);
	buf[1] = (byte)('0' + (i / 100) % 10);
	buf[2] = (byte)('0' + (i / 10) % 10);
	buf[3] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);
	
	dfield_set_data(field, buf + 8, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 2);

	third_size = 90;
	
	dfield_set_data(field, data_buf, third_size);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */
/*为b树速度测试生成测试元组。*/
void
dtuple_gen_search_tuple_TPC_A(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 10000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000");

	buf[0] = (byte)('0' + (i / 1000) % 10);
	buf[1] = (byte)('0' + (i / 100) % 10);
	buf[2] = (byte)('0' + (i / 10) % 10);
	buf[3] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for TPC-C speed test. */

void
dtuple_gen_test_tuple_TPC_C(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 12 fields */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		size;
	ulint		j;

	ut_ad(tuple && buf);
	ut_ad(i < 100000);
	
	field = dtuple_get_nth_field(tuple, 0);

	buf[0] = (byte)('0' + (i / 10000) % 10);
	buf[1] = (byte)('0' + (i / 1000) % 10);
	buf[2] = (byte)('0' + (i / 100) % 10);
	buf[3] = (byte)('0' + (i / 10) % 10);
	buf[4] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	for (j = 0; j < 10; j++) {

		field = dtuple_get_nth_field(tuple, 2 + j);

		size = 24;
	
		dfield_set_data(field, data_buf, size);
		dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH,
								100, 0);
	}

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple_TPC_C(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 100000);
	
	field = dtuple_get_nth_field(tuple, 0);

	buf[0] = (byte)('0' + (i / 10000) % 10);
	buf[1] = (byte)('0' + (i / 1000) % 10);
	buf[2] = (byte)('0' + (i / 100) % 10);
	buf[3] = (byte)('0' + (i / 10) % 10);
	buf[4] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

#endif /* notdefined */
