#include "confusables_data.h"

#include "system.h"

#include <stddef.h>

static int str_utf8_skeleton(int ch, const int **skeleton, int *skeleton_len)
{
	int i;
	for(i = 0; i < NUM_DECOMPS; i++)
	{
		if(ch == decomp_chars[i])
		{
			int offset = decomp_slices[i].offset;
			int length = decomp_lengths[decomp_slices[i].length];

			*skeleton = &decomp_data[offset];
			*skeleton_len = length;
			return 1;
		}
		else if(ch < decomp_chars[i])
		{
			break;
		}
	}
	*skeleton = NULL;
	*skeleton_len = 1;
	return 0;
}

struct SKELETON
{
	const int *skeleton;
	int skeleton_len;
	const char *str;
};

void str_utf8_skeleton_begin(struct SKELETON *skel, const char *str)
{
	skel->skeleton = NULL;
	skel->skeleton_len = 0;
	skel->str = str;
}

int str_utf8_skeleton_next(struct SKELETON *skel)
{
	int ch;
	while(skel->skeleton_len == 0)
	{
		ch = str_utf8_decode(&skel->str);
		if(ch == 0)
		{
			return 0;
		}
		str_utf8_skeleton(ch, &skel->skeleton, &skel->skeleton_len);
	}
	skel->skeleton_len--;
	if(skel->skeleton != NULL)
	{
		ch = *skel->skeleton;
		skel->skeleton++;
	}
	return ch;
}

int str_utf8_comp_confusable(const char *str1, const char *str2)
{
	struct SKELETON skel1;
	struct SKELETON skel2;

	str_utf8_skeleton_begin(&skel1, str1);
	str_utf8_skeleton_begin(&skel2, str2);

	while(1)
	{
		int ch1 = str_utf8_skeleton_next(&skel1);
		int ch2 = str_utf8_skeleton_next(&skel2);

		if(ch1 == 0 || ch2 == 0)
			return ch1 != ch2;

		if(ch1 != ch2)
			return 1;
	}
}
