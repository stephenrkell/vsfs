#ifndef BITMAP_H_
#define BITMAP_H_

#include <assert.h>
// want a truly word-sized integer... let's use uintptr_t for now
#include <stdint.h>

#ifndef WORD_BITSIZE
#define WORD_BITSIZE __INTPTR_WIDTH__
#endif
typedef uintptr_t bitmap_word_t;
#define BITMAP_WORD_NBITS (8*sizeof(bitmap_word_t))
_Static_assert(BITMAP_WORD_NBITS == WORD_BITSIZE, "bitmap word size is platform word size");

#define popcount_y_(n) popcount ## n
#define popcount_x_(n) popcount_y_(n)
#define popcount_word popcount_x_(WORD_BITSIZE)
// FIXME: replace these with the fast versions!
static inline int popcount64(uint64_t x) {
	int c = 0;
	int i;
	for (i = 0; i < 64; i++) {
		c += x & 1;
		x >>= 1;
	}
	return c;
}

static inline int popcount32(uint32_t x) {
	int c = 0;
	int i;
	for (i = 0; i < 32; i++) {
		c += x & 1;
		x >>= 1;
	}
	return c;
}
#define BITMAP_FOR_EACH_BIT_SET(b, b_end, action) \
    for (bitmap_word_t *p = (b); p != (b_end); ++p) { \
        bitmap_word_t word = *p; \
        /* little-endian means we start with the LSB */ \
        for (unsigned i = 0; i < BITMAP_WORD_NBITS; ++i) { \
            if (word & (0x1ul << i)) { action( (p-(b)) + i ); } \
        } \
    }

#define BOTTOM_N_BITS_SET_T(t, n) \
 ( ( (n)==0 ) ? 0 : ((n) == 8*sizeof(t) ) \
 	? (~((t)0)) \
	: ((((t)1u) << ((n))) - 1))
#define BOTTOM_N_BITS_CLEAR_T(t, n) (~(BOTTOM_N_BITS_SET_T(t, (n))))
#define TOP_N_BITS_SET_T(t, n)      (BOTTOM_N_BITS_CLEAR_T(t, 8*(sizeof(t))-((n))))
#define TOP_N_BITS_CLEAR_T(t, n)    (BOTTOM_N_BITS_SET_T(t, 8*(sizeof(t))-((n))))

#define BOTTOM_N_BITS_SET(n)   BOTTOM_N_BITS_SET_T(uintptr_t, n)
#define BOTTOM_N_BITS_CLEAR(n) BOTTOM_N_BITS_CLEAR_T(uintptr_t, n)
#define TOP_N_BITS_SET(n)      TOP_N_BITS_SET_T(uintptr_t, n)
#define TOP_N_BITS_CLEAR(n)    TOP_N_BITS_CLEAR(uintptr_t, n)

#define NBITS(t) ((sizeof (t))<<3)
#define UNSIGNED_LONG_NBITS (NBITS(unsigned long))

/* In our bitmaps, the lowest-indexed bit is the least significant bit in the word.
 * This is best for forward searches ("find the next higher").
 */

static inline _Bool bitmap_get(bitmap_word_t *p_bitmap, unsigned long index)
{
	return p_bitmap[index / BITMAP_WORD_NBITS] & (1ul << (index % BITMAP_WORD_NBITS));
}
static inline void bitmap_set(bitmap_word_t *p_bitmap, unsigned long index)
{
	p_bitmap[index / BITMAP_WORD_NBITS] |= (1ul << (index % BITMAP_WORD_NBITS));
}
static inline void bitmap_clear(bitmap_word_t *p_bitmap, unsigned long index)
{
	p_bitmap[index / BITMAP_WORD_NBITS] &= ~(1ul << (index % BITMAP_WORD_NBITS));
}
/* Here we do a reverse search for the first bit set at or below
 * bit position start_idx.
 * We return the position (index) of that bit, or (bitmap_word_t) -1 if no set bit was found.
 * Optionally, via out_test_bit we return the bitmask identifying the found bit.
 */
static inline unsigned long bitmap_rfind_first_set_leq(bitmap_word_t *p_bitmap, bitmap_word_t *p_limit, long start_idx, unsigned long *out_test_bit)
{
	bitmap_word_t *p_base = p_bitmap;
	p_bitmap += start_idx / BITMAP_WORD_NBITS;
	start_idx %= BITMAP_WORD_NBITS;
	if (p_bitmap > p_limit) return (unsigned long) -1;
	while (1)
	{
		while (start_idx >= 0)
		{
			bitmap_word_t test_bit = 1ul << start_idx;
			if (*p_bitmap & test_bit)
			{
				if (out_test_bit) *out_test_bit = test_bit;
				return start_idx + (p_bitmap - p_base) * BITMAP_WORD_NBITS;
			}
			--start_idx;
		}
		// now start_idx < 0
		if (p_bitmap == p_base) break;
		start_idx = BITMAP_WORD_NBITS - 1;
		--p_bitmap;
	}
	return (unsigned long) -1;
}
/* Here we do a forward search for the first bit set starting at position start_idx.
 * We return its position, or (bitmap_word_t)-1 if not found.
 * Optionally, on success we also output the bitmask identifying that bit.
 */
static inline unsigned long bitmap_find_first_set1_geq(bitmap_word_t *p_bitmap, bitmap_word_t *p_limit, unsigned long start_idx, unsigned long *out_test_bit)
{
	bitmap_word_t *p_base = p_bitmap;
	p_bitmap += start_idx / BITMAP_WORD_NBITS;
	start_idx %= BITMAP_WORD_NBITS;
	if (p_bitmap >= p_limit) return (unsigned long) -1;
	while (1)
	{
		while (start_idx < BITMAP_WORD_NBITS)
		{
			unsigned long test_bit = 1ul << start_idx;
			if (*p_bitmap & test_bit)
			{
				if (out_test_bit) *out_test_bit = test_bit;
				return start_idx + (p_bitmap - p_base) * BITMAP_WORD_NBITS;
			}
			++start_idx;
		}
		// now start_idx < 0
		++p_bitmap;
		if (p_bitmap == p_limit) break;
		start_idx = 0;
	}
	return (unsigned long) -1;
}
static inline unsigned long bitmap_find_first_set(bitmap_word_t *p_bitmap, bitmap_word_t *p_limit, unsigned long *out_test_bit)
{
	bitmap_word_t *p_initial_bitmap;
			
	while (*p_bitmap == (bitmap_word_t) 0
				&& p_bitmap < p_limit) ++p_bitmap;
	if (p_bitmap == p_limit) return (unsigned long) -1;
	
	/* Find the lowest free bit in this bitmap. */
	unsigned long test_bit = 1;
	unsigned test_bit_index = 0;
	// while the test bit is unset...
	while (!(*p_bitmap & test_bit))
	{
		if (__builtin_expect(test_bit != 1ul<<(BITMAP_WORD_NBITS - 1), 1))
		{
			test_bit <<= 1;
			++test_bit_index;
		}
		else assert(0); // all 1s --> we shouldn't have got here
	}
	/* FIXME: thread-safety */
	unsigned free_index = (p_bitmap - p_initial_bitmap) * BITMAP_WORD_NBITS
			+ test_bit_index;
	
	if (out_test_bit) *out_test_bit = test_bit;
	return free_index;	
}
static inline unsigned long bitmap_find_first_clear(bitmap_word_t *p_bitmap, bitmap_word_t *p_limit, unsigned long *out_test_bit)
{
	bitmap_word_t *p_initial_bitmap = p_bitmap;
	while (*p_bitmap == (bitmap_word_t) -1
				&& p_bitmap < p_limit) ++p_bitmap;
	if (p_bitmap == p_limit) return (unsigned long) -1;
	
	/* Find the lowest free bit in this bitmap. */
	unsigned long test_bit = 1;
	unsigned test_bit_index = 0;
	while (*p_bitmap & test_bit)
	{
		if (__builtin_expect(test_bit != 1ul<<(BITMAP_WORD_NBITS - 1), 1))
		{
			test_bit <<= 1;
			++test_bit_index;
		}
		else assert(0); // all 1s --> we shouldn't have got here
	}
	/* FIXME: thread-safety */
	unsigned free_index = (p_bitmap - p_initial_bitmap) * BITMAP_WORD_NBITS
			+ test_bit_index;
	
	if (out_test_bit) *out_test_bit = test_bit;
	return free_index;
}
static inline unsigned long bitmap_count_set(bitmap_word_t *p_bitmap, bitmap_word_t *p_limit,
	unsigned long start_idx_ge, unsigned long end_idx_lt)
{
	if (end_idx_lt <= start_idx_ge) return 0;
	bitmap_word_t *p_startword = p_bitmap + (start_idx_ge / BITMAP_WORD_NBITS);
	bitmap_word_t *p_endword = p_bitmap + ((end_idx_lt + (BITMAP_WORD_NBITS-1) / BITMAP_WORD_NBITS));
	start_idx_ge %= BITMAP_WORD_NBITS;
	end_idx_lt %= BITMAP_WORD_NBITS;
	if (p_startword >= p_limit) return (unsigned long) -1;
	if (p_endword >= p_limit) return (unsigned long) -1;
	unsigned long count = 0;
	while (p_startword != p_endword)
	{
		bitmap_word_t word;
		if (start_idx_ge)
		{
			// only count the higher-addressed (most-significant) 
			// BITMAP_WORD_NBITS - start_idx_ge
			// bits.
			word = (*p_startword) >> (BITMAP_WORD_NBITS - start_idx_ge);
		} else word = *p_startword;
		count += popcount_word(word);
		++p_startword;
		start_idx_ge = 0; // start from the beginning of the next word
	}
	// now just handle the last word. BEWARE: start_idx_ge may still be nonzero,
	// if our first word and last words are the same.
	// create a bitmask in which only bits [start_idx_ge, end_idx_lt) are set.
	bitmap_word_t word;
	unsigned nbits = end_idx_lt - start_idx_ge;
	if (nbits < BITMAP_WORD_NBITS)
	{
		bitmap_word_t bitmask = 
			/* set bottom bits up to our end idx */
			BOTTOM_N_BITS_SET_T(bitmap_word_t, end_idx_lt) & 
			/* set top bits down to our start idx */
			TOP_N_BITS_SET_T(bitmap_word_t, BITMAP_WORD_NBITS - start_idx_ge)
			/* ANDed together... */
			;
		word = *p_startword & bitmask;
	} else word = *p_startword;
	count += popcount_word(word);
	return count;
}

#endif
