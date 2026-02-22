/* blip_buf $vers. http://www.slack.net/~ant/ */

#include "blip_buf.h"
#include "fixed.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

/* Library Copyright (C) 2003-2009 Shay Green. This library is free software;
you can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
library is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

typedef fix32<20> fixed_t;

static const int time_bits  = 20;
static const int frac_bits  = 20; 
static const int bass_shift = 9; /* affects high-pass filter breakpoint frequency */
static const int end_frame_extra = 2; /* allows deltas slightly after frame length */

static const int half_width  = 8;
static const int buf_extra   = (half_width * 2) + end_frame_extra;
static const int phase_bits  = 5;
static const int phase_count = 1 << phase_bits;
static const int delta_bits  = 15;
static const int delta_unit  = 1 << delta_bits;

/** Sample buffer that resamples to output rate and accumulates samples
until they're read out */
struct blip_t
{
	fixed_t factor;
	fixed_t offset;
	int avail;
	int size;
	int integrator;
};

typedef int buf_t;

/* probably not totally portable */
#define SAMPLES( m ) ((buf_t*) ((m) + 1))

/* Arithmetic (sign-preserving) right shift */
#define ARITH_SHIFT( n, shift ) ((n) >> (shift))

static const int max_sample = +32767;
static const int min_sample = -32768;

#define CLAMP( n ) \
	{\
		if ( (short) n != n )\
			n = ARITH_SHIFT( n, 16 ) ^ max_sample;\
}

blip_t* blip_new( int size )
{
	blip_t* m = (blip_t*) malloc( sizeof (blip_t) + (size + buf_extra) * sizeof (buf_t) );
	if ( m )
	{
		m->size = size;
		m->factor = fixed_t(1.0 / blip_max_ratio);
		blip_clear( m );
	}
	return m;
}

void blip_delete( blip_t* m )
{
	if ( m ) free( m );
}

void blip_set_rates( blip_t* m, double clock_rate, double sample_rate )
{
	m->factor = fixed_t(sample_rate / clock_rate);
	/* At this point, factor is most likely rounded up, but could still
	have been rounded down in the floating-point calculation. */
}

void blip_clear( blip_t* m )
{
	/* We could set offset to 0, factor/2, or factor-1. 0 is suitable if
	factor is rounded up. factor-1 is suitable if factor is rounded down.
	Since we don't know rounding direction, factor/2 accommodates either,
	with the slight loss of showing an error in half the time. Since for
	a 64-bit factor this is years, the halving isn't a problem. */

	m->offset     = fixed_t::FromRawValue(m->factor.GetRawValue() >> 1);
	m->avail      = 0;
	m->integrator = 0;
	memset( SAMPLES( m ), 0, (m->size + buf_extra) * sizeof (buf_t) );
}

int blip_clocks_needed( const blip_t* m, int samples )
{
	fixed_t needed = fixed_t(samples);
	if ( needed.GetRawValue() < m->offset.GetRawValue() )
	{
		return 0;
	}
	
	int raw_needed = needed.GetRawValue() - m->offset.GetRawValue() + m->factor.GetRawValue() - 1;
	return raw_needed / m->factor.GetRawValue();
}

void blip_end_frame( blip_t* m, unsigned t )
{
	int off = (int)t * m->factor.GetRawValue() + m->offset.GetRawValue();
	m->avail += (off >> time_bits);
	m->offset = fixed_t::FromRawValue(off & ((1 << time_bits) - 1));
}

int blip_samples_avail( const blip_t* m )
{
	return m->avail;
}

static void remove_samples( blip_t* m, int count )
{
	buf_t* buf = SAMPLES( m );
	int remain = m->avail + buf_extra - count;
	m->avail -= count;
	
	memmove( &buf [0], &buf [count], remain * sizeof (buf_t) );
	memset( &buf [remain], 0, count * sizeof (buf_t) );
}

int blip_read_samples( blip_t* m, short out [], int count, int stereo )
{
	if ( count > m->avail )
	{
		count = m->avail;
	}
	if ( count )
	{
		int const step = stereo ? 2 : 1;
		buf_t* in = SAMPLES( m );
		int sum = m->integrator;
		for (int i = 0; i < count; i++)
		{
			/* Eliminate fraction */
			int s = ARITH_SHIFT( sum, delta_bits );
			
			sum += in[i];
			
			CLAMP( s );
			out[i * step] = (short)s;
			/* High-pass filter */
			sum -= s << (delta_bits - bass_shift);
		}
		m->integrator = sum;
		remove_samples( m, count );
	}
	return count;
}

/* Sinc_Generator( 0.9, 0.55, 4.5 ) */
static short const bl_step [phase_count + 1] [half_width] =
{
{   43, -115,  350, -488, 1136, -914, 5861,21022},
{   44, -118,  348, -473, 1076, -799, 5274,21001},
{   45, -121,  344, -454, 1011, -677, 4706,20936},
{   46, -122,  336, -431,  942, -549, 4156,20829},
{   47, -123,  327, -404,  868, -418, 3629,20679},
{   47, -122,  316, -375,  792, -285, 3124,20488},
{   47, -120,  303, -344,  714, -151, 2644,20256},
{   46, -117,  289, -310,  634,  -17, 2188,19985},
{   46, -114,  273, -275,  553,  117, 1758,19675},
{   44, -108,  255, -237,  471,  247, 1356,19327},
{   43, -103,  237, -199,  390,  373,  981,18944},
{   42,  -98,  218, -160,  310,  495,  633,18527},
{   40,  -91,  198, -121,  231,  611,  314,18078},
{   38,  -84,  178,  -81,  153,  722,   22,17599},
{   36,  -76,  157,  -43,   80,  824, -241,17092},
{   34,  -68,  135,   -3,    8,  919, -476,16558},
{   32,  -61,  115,   34,  -60, 1006, -683,16001},
{   29,  -52,   94,   70, -123, 1083, -862,15422},
{   27,  -44,   73,  106, -184, 1152,-1015,14824},
{   25,  -36,   53,  139, -239, 1211,-1142,14210},
{   22,  -27,   34,  170, -290, 1261,-1244,13582},
{   20,  -20,   16,  199, -335, 1301,-1322,12942},
{   18,  -12,   -3,  226, -375, 1331,-1376,12293},
{   15,   -4,  -19,  250, -410, 1351,-1408,11638},
{   13,    3,  -35,  272, -439, 1361,-1419,10979},
{   11,    9,  -49,  292, -464, 1362,-1410,10319},
{    9,   16,  -63,  309, -483, 1354,-1383, 9660},
{    7,   22,  -75,  322, -496, 1337,-1339, 9005},
{    6,   26,  -85,  333, -504, 1312,-1280, 8355},
{    4,   31,  -94,  341, -507, 1278,-1205, 7713},
{    3,   35, -102,  347, -506, 1238,-1119, 7082},
{    1,   40, -110,  350, -499, 1190,-1021, 6464},
{    0,   43, -115,  350, -488, 1136, -914, 5861}
};

void blip_add_delta( blip_t* m, unsigned time, int delta )
{
	int raw_res = (int)time * m->factor.GetRawValue() + m->offset.GetRawValue();
    int sample_pos = (raw_res >> frac_bits);
	buf_t* out = SAMPLES( m ) + m->avail + sample_pos;
	
	int const p_shift = frac_bits - phase_bits;
	int phase = (raw_res >> p_shift) & (phase_count - 1);

	short const* in = bl_step [phase];
	short const* rev = bl_step [phase_count - phase];
	
	int interp = (raw_res >> (p_shift - delta_bits)) & (delta_unit - 1);
	int delta2 = (delta * interp) >> delta_bits;
	delta -= delta2;

	out[0]  += in[0]  * delta + in[8]   * delta2;
	out[1]  += in[1]  * delta + in[9]   * delta2;
	out[2]  += in[2]  * delta + in[10]  * delta2;
	out[3]  += in[3]  * delta + in[11]  * delta2;
	out[4]  += in[4]  * delta + in[12]  * delta2;
	out[5]  += in[5]  * delta + in[13]  * delta2;
	out[6]  += in[6]  * delta + in[14]  * delta2;
	out[7]  += in[7]  * delta + in[15]  * delta2;
	
	out[8] 	+= rev[7] * delta + rev[15] * delta2;
	out[9] 	+= rev[6] * delta + rev[14] * delta2;
	out[10] += rev[5] * delta + rev[13] * delta2;
	out[11] += rev[4] * delta + rev[12] * delta2;
	out[12] += rev[3] * delta + rev[11] * delta2;
	out[13] += rev[2] * delta + rev[10] * delta2;
	out[14] += rev[1] * delta + rev[9]	* delta2;
	out[15] += rev[0] * delta + rev[8]	* delta2;
}

void blip_add_delta_fast( blip_t* m, unsigned time, int delta )
{
	int raw_res = (int)time * m->factor.GetRawValue() + m->offset.GetRawValue();
	buf_t* out = SAMPLES( m ) + m->avail + (raw_res >> frac_bits);
	
	int interp = (raw_res >> (frac_bits - delta_bits)) & (delta_unit - 1);
	int delta2 = delta * interp;
	
	out [7] += delta * delta_unit - delta2;
	out [8] += delta2;
}
