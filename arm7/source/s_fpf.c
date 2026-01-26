#include <stdint.h>
#include "s_fpf.h"

static inline int32 sat32(int64 x)
{
    if (x > INT32_MAX) return INT32_MAX;
    if (x < INT32_MIN) return INT32_MIN;
    return (int32)x;
}

static inline int32 highpass(HPF *f, int32 input, int shift)
{
    int64 acc;

    acc  = (int64)input - f->prev;
    acc += ((int64)f->out * ((1LL << shift) - 1)) >> shift;

    f->prev = input;
    f->out  = sat32(acc);

    return f->out;
}

static inline int32 lowpass(LPF *f, int32 input, int shift)
{
    int64 acc;

    acc  = f->out;
    acc += ((int64)input - f->out) >> shift;

    f->out = sat32(acc);
    return f->out;
}

static inline int32 bandpass(BPF *f, int32 input, int hpf_shift, int lpf_shift)
{
    int32 x = highpass(&f->hpf, input, hpf_shift);
    return lowpass(&f->lpf, x, lpf_shift);
}

void MidsEq_Init(MidsEq *eq)
{
    eq->hpf1 = (HPF){0};
    eq->hpf2 = (HPF){0};
    eq->lpf  = (LPF){0};
    eq->bpf  = (BPF){0};
}

int32 MidsEq_Process(MidsEq *eq, int32 x)
{
    int64 acc;

    x = highpass(&eq->hpf1, x, 7);

    int32 mids = bandpass(&eq->bpf, x, 5, 4);

    acc = (int64)x - (mids >> 1);
    x   = sat32(acc);

    int32 highs = highpass(&eq->hpf2, x, 4);

    acc = (int64)x + (highs >> 2);
    x   = sat32(acc);

    x = lowpass(&eq->lpf, x, 2);
    return x;
}
