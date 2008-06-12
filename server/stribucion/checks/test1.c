#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <miscmac.h>
#include <log.h>
#include "header.h"

struct h_test {
	char const *name;
	char const *msg;
	int nb_fields;
	struct {
		char const *name, *value;
	} results[5];
};

static void do_test(struct h_test const *test)
{
	printf("Test %s ", test->name);
	size_t msg_len = strlen(test->msg)+1;
	char *rw_msg = malloc(msg_len);
	assert(rw_msg);
	memcpy(rw_msg, test->msg, msg_len);
	struct header *head = header_new(rw_msg);
	assert(head);
	assert(head->nb_fields == test->nb_fields);
	for (unsigned r=0; r<sizeof_array(test->results); r++) {
		if (! test->results[r].name) break;
		unsigned key = header_key(test->results[r].name);
		char const *value = header_search(head, test->results[r].name, key);
		if (value) {
			assert(test->results[r].value);
			assert(0==strcmp(value, test->results[r].value));
		} else {
			assert(!test->results[r].value);
		}
		putchar('.');
	}
	puts("ok");
	free(rw_msg);
}

int main(void)
{
	struct h_test tests[] = {
		{
			.name = "triming",
			.msg =
				"NormalName: NormalValue\n"
				"RightTrimName   :RightTrimValue   \n"
				"LongTrimTest: WatchOut!  \n  for there's another line\n"
				"TrimName   :   BothTrimValue  \n \n"
				"Compactable  Name: Compactable Value \n that goes for \n   multiple  lines\n",
			.nb_fields = 5,
			.results = {
				{ .name = "NormalName", .value = "NormalValue", },
				{ .name = "RightTrimName", .value = "RightTrimValue", },
				{ .name = "LongTrimTest", .value = "WatchOut! for there's another line", },
				{ .name = "TrimName", .value = "BothTrimValue", },
				{ .name = "Compactable Name", .value = "Compactable Value that goes for multiple lines", },
			}
		}, {
			.name = "emptyness",
			.msg = "",
			.nb_fields = 0,
			.results = {
				{ .name = "foo", .value = NULL, },
				{ .name = NULL, .value = NULL, },
			},
		}, {
			.name = "uniqueness",
			.msg = "SingleName: SingleValue\n",
			.nb_fields = 1,
			.results = {
				{ .name = "SingleName", .value = "SingleValue", },
				{ .name = "Glop", .value = NULL, },
				{ .name = NULL, .value = NULL, },
			},
		}
	};

	(void)log_init();
	for (unsigned t=0; t<sizeof_array(tests); t++) {
		do_test(tests+t);
	}
	return EXIT_SUCCESS;
}

