#ifndef S_APU_H__
#define S_APU_H__

#ifdef __cplusplus
extern "C" {
#endif

void APUSoundInstall(void);
void APU4015Reg(void);
void APUSoundWrite(Uint address, Uint value);

#ifdef __cplusplus
}
#endif

#endif /* S_APU_H__ */
