/*
 * Copyright (c) 2013 Tatsuhiko Kubo <cubicdaiya@gmail.com>
 * Copyright (c) 2018 Kristaps Dzonsons <kristaps@bsd.lv>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "diff.h"

struct 	coord {
	int		 x;
	int		 y;
	int		 k;
};

struct 	onp_diff {
	onp_sequence_t	*a;
	onp_sequence_t	*b;
	size_t		 m;
	size_t	 	 n;
	int		*path;
	size_t	 	 delta;
	size_t	 	 offset;
	size_t	 	 size;
	size_t	 	 editdis;
	struct coord	*pathcoords;
	size_t		 pathcoordsz;
	int 		 swapped;
	struct diff	*result;
};

/*
 * Search shortest path and record the path.
 */
static int 
snake(struct onp_diff *diff, int k, int above, int below) 
{
	int 	 r, y, x;
	void	*pp;

	y = above > below ? above : below;
	x = y - k;

	r = above > below ? 
		diff->path[k - 1 + diff->offset] : 
		diff->path[k + 1 + diff->offset];

	while (x < (int)diff->m && y < (int)diff->n && 
	       (diff->swapped ? diff->b[y] == diff->a[x] :
		diff->a[x] == diff->b[y])) {
		++x;
		++y;
	}

	diff->path[k + diff->offset] = diff->pathcoordsz;

	pp = reallocarray
		(diff->pathcoords,
		 diff->pathcoordsz + 1,
		 sizeof(struct coord));
	if (NULL == pp)
		return -1;
	diff->pathcoords = pp;

	assert(x >= 0);
	assert(y >= 0);

	diff->pathcoords[diff->pathcoordsz].x = x;
	diff->pathcoords[diff->pathcoordsz].y = y;
	diff->pathcoords[diff->pathcoordsz].k = r;
	diff->pathcoordsz++;

	return y;
}

static int 
addseq(struct onp_diff *diff, const onp_sequence_t *e, 
	size_t beforeIdx, size_t afterIdx, enum edit type) 
{
	void	*pp;

	pp = reallocarray
		(diff->result->ses,
		 diff->result->sessz + 1,
		 sizeof(struct sesent));
	if (NULL == pp)
		return 0;
	diff->result->ses = pp;
	diff->result->ses[diff->result->sessz].beforeIdx = beforeIdx;
	diff->result->ses[diff->result->sessz].afterIdx = afterIdx;
	diff->result->ses[diff->result->sessz].type = type;
	diff->result->ses[diff->result->sessz].e = e;
	diff->result->sessz++;
	return 1;
}

static int
genseq(struct onp_diff *diff, const struct coord* v, size_t vsz) 
{
	const onp_sequence_t	*x, *y;
	size_t         	 x_idx,  y_idx;  /* line number */
	int		 px_idx, py_idx; /* cordinates */
	int		 complete = 0;
	int		 rc;
	size_t		 i;

	x_idx = y_idx = 1;
	px_idx = py_idx = 0;
	x = diff->a;
	y = diff->b;

	assert(vsz);

	for (i = vsz - 1; ! complete; --i) {
		while (px_idx < v[i].x || py_idx < v[i].y)
			if (v[i].y - v[i].x > py_idx - px_idx) {
				rc = ! diff->swapped ?
					addseq(diff, y, 
					 0, y_idx, EDIT_ADD) :
					addseq(diff, y, 
					 y_idx, 0, EDIT_DELETE);
				++y;
				++y_idx;
				++py_idx;
			} else if (v[i].y - v[i].x < py_idx - px_idx) {
				rc = ! diff->swapped ?
					addseq(diff, x, 
					 x_idx, 0, EDIT_DELETE) :
					addseq(diff, x, 0, 
					 x_idx, EDIT_ADD);
				++x;
				++x_idx;
				++px_idx;
			} else {
				rc = ! diff->swapped ?
					addseq(diff, x, x_idx, 
					 y_idx, EDIT_COMMON) :
					addseq(diff, y, y_idx, 
					 x_idx, EDIT_COMMON);
				++x;
				++y;
				++x_idx;
				++y_idx;
				++px_idx;
				++py_idx;
			}
		if ( ! rc)
			return -1;
		complete = 0 == i;
	}

	return x_idx > diff->m && y_idx > diff->n;
}

static struct onp_diff *
onp_alloc_diff(onp_sequence_t *a, size_t asz, 
	onp_sequence_t *b, size_t bsz) 
{
	struct onp_diff *diff;

	diff = calloc(1, sizeof(struct onp_diff));

	if (NULL == diff)
		return NULL;

	if (asz > bsz) {
		diff->a = b;
		diff->b = a;
		diff->m = bsz;
		diff->n = asz;
		diff->swapped = 1;
	} else {
		diff->a = a;
		diff->b = b;
		diff->m = asz;
		diff->n = bsz;
		diff->swapped = 0;
	}

	assert(diff->n >= diff->n);
	diff->delta = diff->n - diff->m;
	diff->offset = diff->m + 1;
	diff->size = diff->m + diff->n + 3;

	return diff;
}

static void 
onp_free_diff(struct onp_diff *diff) 
{

	free(diff->path);
	free(diff->pathcoords);
	free(diff);
}

static int 
onp_compose(struct onp_diff *diff, struct diff *result) 
{
	int		 rc = 0;
	int		 p = -1;
	int		 k;
	int		*fp = NULL;
	int		 r;
	struct coord	*epc = NULL;
	size_t		 epcsz = 0;
	size_t		 i;
	void		*pp;

	/* Initialise the path from origin to target. */

	fp = malloc(sizeof(int) * diff->size);
	diff->path = malloc(sizeof(int) * diff->size);
	diff->result = result;

	if (NULL == fp || NULL == diff->path)
		goto out;

	for (i = 0; i < diff->size; i++)
		fp[i] = diff->path[i] = -1;

	/*
	 * Run the actual algorithm.
	 * This computes the full path in diff->path from the origin to
	 * the target.
	 */

	do {
		p++;
		for (k = -p; 
		     k <= (ssize_t)diff->delta - 1; k++) {
			fp[k + diff->offset] = snake(diff, k, 
				fp[k - 1 + diff->offset] + 1, 
				fp[k + 1 + diff->offset]);
			if (fp[k + diff->offset] < 0)
				goto out;
		}
		for (k = diff->delta + p; 
		     k >= (ssize_t)diff->delta + 1; k--) {
			fp[k + diff->offset] = snake(diff, k, 
				fp[k - 1 + diff->offset] + 1, 
				fp[k + 1 + diff->offset]);
			if (fp[k + diff->offset] < 0)
				goto out;
		}

		fp[diff->delta + diff->offset] = 
			snake(diff, diff->delta, 
				fp[diff->delta - 1 + diff->offset] + 1,
				fp[diff->delta + 1 + diff->offset]);
		if (fp[diff->delta + diff->offset] < 0)
			goto out;
	} while (fp[diff->delta + diff->offset] != (ssize_t)diff->n);

	/* Now compute edit distance. */

	diff->editdis = diff->delta + 2 * p;

	/*
	 * Here we compute the shortest edit script and the least common
	 * subsequence from the path.
	 */

	r = diff->path[diff->delta + diff->offset];

	while(-1 != r) {
		pp = reallocarray
			(epc, epcsz + 1, 
			 sizeof(struct coord));
		if (NULL == pp)
			goto out;
		epc = pp;
		epc[epcsz].x = diff->pathcoords[r].x;
		epc[epcsz].y = diff->pathcoords[r].y;
		epcsz++;
		r = diff->pathcoords[r].k;
	}

	if (epcsz)
		genseq(diff, epc, epcsz);

	rc = 1;
out:
	free(fp);
	free(epc);
	return rc;
}

int
diff(struct diff *d, onp_sequence_t *p1, size_t sz1, 
	onp_sequence_t *p2, size_t sz2)
{
	struct onp_diff	*p;
	int		 rc;

	if (NULL == d)
		return 0;

	memset(d, 0, sizeof(struct diff));

	p = onp_alloc_diff(p1, sz1, p2, sz2);
	if (NULL == p)
		return -1;

	rc = onp_compose(p, d);
	onp_free_diff(p);

	if (0 == rc) {
		free(d->ses);
		return -1;
	}

	return 1;
}
