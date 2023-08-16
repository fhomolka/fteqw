#ifndef Q2_WORLD_H
#define Q2_WORLD_H 

#ifdef Q2SERVER
typedef struct q2edict_s q2edict_t;

void VARGS WorldQ2_LinkEdict(world_t *w, q2edict_t *ent);
void VARGS WorldQ2_UnlinkEdict(world_t *w, q2edict_t *ent);
int VARGS WorldQ2_AreaEdicts (world_t *w, const vec3_t mins, const vec3_t maxs, q2edict_t **list,
	int maxcount, int areatype);
trace_t WorldQ2_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hitcontentsmask, q2edict_t *passedict);
void WorldQ2_ClearWorld_Nodes (world_t *w, qboolean relink);
#endif

#endif