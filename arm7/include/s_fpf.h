#pragma once
#include "audiosys.h"

typedef struct {
    int32 out;
} LPF;

typedef struct {
    int32 prev;
    int32 out;
} HPF;

typedef struct {
    HPF hpf;
    LPF lpf;
} BPF;

typedef struct {
    HPF hpf1;
    HPF hpf2;
    LPF lpf;
    BPF bpf;
} MidsEq;

void MidsEq_Init(MidsEq *eq);
int32 MidsEq_Process(MidsEq *eq, int32 sample);
