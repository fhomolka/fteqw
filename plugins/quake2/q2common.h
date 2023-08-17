#ifndef FTEPLUGIN
#define FTEPLUGIN
#endif

#ifndef Q2COMMON_H
#define Q2COMMON_H

#include "../plugins/plugin.h"
/*
typedef struct q2csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} q2csurface_t;
*/
typedef struct q2cplane_s
{
	vec3_t	normal;
	float	dist;
	qbyte	type;			// for fast side tests
	qbyte	signbits;		// signx + (signy<<1) + (signz<<1)
	qbyte	pad[2];
} q2cplane_t;

typedef struct q2trace_s
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	const q2csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	void	*ent;		// not set by CM_*() functions
} q2trace_t;


#include "q2client.h"
#include "q2game.h"
#include "q2world.h"

typedef struct	//merge?
{
	int					areabytes;
	qbyte				areabits[MAX_Q2MAP_AREAS/8];		// portalarea visibility bits
	q2player_state_t	ps[MAX_SPLITS];		//yuck
	int					clientnum[MAX_SPLITS];
	int					num_entities;
	int					first_entity;		// into the circular sv_packet_entities[]
	int					senttime;			// for ping calculations
	float				ping_time;
} q2client_frame_t; 

#ifdef HAVE_SERVER
//svq2_game.c
qboolean SVQ2_InitGameProgs(void);
void SVQ2_ShutdownGameProgs(void);
void PFQ2_Configstring(int i, const char *val); //for engine cheats.
//svq2_ents.c
void SVQ2_BuildClientFrame(struct client_s *client);
void SVQ2_WriteFrameToClient(struct client_s *client, sizebuf_t *msg);
void MSGQ2_WriteDeltaEntity(struct q2entity_state_s *from, struct q2entity_state_s *to, sizebuf_t *msg, qboolean force, qboolean newentity);
void SVQ2_BuildBaselines(void);

void SVQ2_ExecuteClientMessage(struct client_s *cl);
// sv_init.c
void SVQ2_Ents_Shutdown(void);
// sv_main.c
void SVQ2_ClearEvents(void);

void SVQ2_ReloadWorld(void);
#endif

char *COMQ2_SkipPath(const char *pathname);

//#ifdef HAVE_CLIENT
extern plugclientfuncs_t	*Iclientfuncs;
extern plugaudiofuncs_t		*Iaudiofuncs;
//#endif

extern plugworldfuncs_t	*Iworldfuncs;
extern plugmsgfuncs_t	*Imsgfuncs;
extern plugfsfuncs_t 	*Ifilesystemfuncs;

#include "com_mesh.h"
extern plugmodfuncs_t *Imodelfuncs;

extern server_static_t	svs;				// persistant server info
extern server_t			sv;					// local server

#define FCheckExists(filename) Ifilesystemfuncs->LocateFile(filename,FSLF_IFFOUND, NULL)

#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)
#define Q2NUM_FOR_EDICT(ent) (((char *)ent - (char *)ge->edicts) / ge->edict_size)

#define NYI(errmsg) plugfuncs->Error("NYI at %s!: %s", __func__, errmsg)

void RQ2_DrawNameTags(vec3_t org, vec3_t diff, vec3_t screenspace, 
	//HACK(fhomolka): This is absolutely stupid, but gets it running
	float (*TraceLine) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int *ent),
	int (*DrawTextField)(int x, int y, int w, int h, const char *text, unsigned int defaultmask, unsigned int fieldflags, struct font_s *font, vec2_t fontscale));

void SVQ2_ClientProtocolExtensionsChanged(client_t *client);
void CLQ2_GetNumberedEntityInfo (int num, float *org, float *ang);


#endif //Q2COMMON_H