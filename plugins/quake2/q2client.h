#ifndef Q2_CLIENT_H
#define Q2_CLIENT_H

#ifdef Q2CLIENT
unsigned int CLQ2_GatherSounds(vec3_t *positions, unsigned int *entnums, sfx_t **sounds, unsigned int max);
void CLQ2_ParseTEnt (void);
void CLQ2_AddEntities (void);
void CLQ2_ParseBaseline (void);
void CLQ2_ClearParticleState(void);
void CLR1Q2_ParsePlayerUpdate(void);
void CLQ2_ParseFrame (int extrabits);
void CLQ2_ParseMuzzleFlash (void);
void CLQ2_ParseMuzzleFlash2 (void);
void CLQ2_ParseInventory (int seat);
int CLQ2_RegisterTEntModels (void);
void CLQ2_WriteDemoBaselines(sizebuf_t *buf);
#endif

#endif