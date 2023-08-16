#include "q2game.h"
#include "q2common.h"
#include "clq2defs.h"

plugworldfuncs_t	*Iworldfuncs;
plugmsgfuncs_t		*Imsgfuncs;
plugmodfuncs_t 		*Imodelfuncs;
plugfsfuncs_t 	*Ifilesystemfuncs;

plugclientfuncs_t	*Iclientfuncs;
plugaudiofuncs_t	*Iaudiofuncs;

//server_static_t	svs;				// persistant server info
//server_t		sv;					// local server


/*
============
COM_SkipPath
============
*/
char *COMQ2_SkipPath (const char *pathname)
{
	const char	*last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/' || *pathname == '\\')
			last = pathname+1;
		pathname++;
	}
	return (char *)last;
}


void SVQ2_ReloadWorld(void)
{
	int i;
	q2edict_t *q2ent;

	WorldQ2_ClearWorld_Nodes (&sv.world, false);
	q2ent = ge->edicts;
	for (i=0 ; i<ge->num_edicts ; i++, q2ent = (q2edict_t *)((char *)q2ent + ge->edict_size))
	{
		if (!q2ent)
			continue;
		if (!q2ent->inuse)
			continue;

		if (q2ent->area.prev)
		{
			q2ent->area.prev = q2ent->area.next = NULL;
			WorldQ2_LinkEdict (&sv.world, q2ent);	// relink ents so touch functions continue to work.
		}
	}
}

//TODO(Fix): move to plugin!
#ifdef Q2SERVER
void SVQ2_ClearEvents(void)
{
	q2edict_t	*ent;
	int		i;

	for (i=0 ; i<ge->num_edicts ; i++, ent++)
	{
		ent = Q2EDICT_NUM(i);
		// events only last for a single message
		ent->s.event = 0;
	}
}
#endif


void SVQ2_RunFrame(void);

qboolean InitGame(struct server_static_s *server_state_static, struct server_s *server_state)
{
	//TODO(fhomolka): maintain sv and svs internally in the plugin
	return (sv.world.worldmodel->fromgame == fg_quake2 || 
			sv.world.worldmodel->fromgame == fg_quake3) && 
			sv.world.worldmodel->funcs.AreasConnected && 
			!*pr_ssqc_progs.string && 
			SVQ2_InitGameProgs();	//these are the rules for running a q2 server
}

void CL_WriteDemoMessage (sizebuf_t *msg, int payloadoffset);
void CLQ2_ParseServerMessage (void);
void CLQ2_PredictMovement (int seat);

qboolean SVQ2_HasGameExport(void)
{
	if(ge) return true;
	return false;
}

void ClientCommand(q2edict_t *ent)
{
	ge->ClientCommand(host_client->q2edict);
}

q2edict_t *UpdateClientNum(int num)
{
	q2edict_t* q2ent;
	q2ent = Q2EDICT_NUM(num);
	q2ent->s.number = num;
	return q2ent;
}

void SpawnEntities (char *mapname, char *entities, char *spawnpoint)
{
	ge->SpawnEntities(mapname, entities, spawnpoint);
}

void ClientDisconnect(q2edict_t *ent)
{
	ge->ClientDisconnect(ent);
}

qboolean ClientConnect (q2edict_t *ent, char *userinfo)
{
	return ge->ClientConnect(ent, userinfo);
}


void ClientUserinfoChanged (q2edict_t *ent, char *userinfo)
{
	ge->ClientUserinfoChanged(ent, userinfo);
}

q2edict_t *EdictFromNum(int i)
{
	q2edict_t *ent = Q2EDICT_NUM(i);
	return ent;
}

int NumFromEdict(q2edict_t *ent)
{
	int n = Q2NUM_FOR_EDICT(ent);
	return n;
}

int SVQ2_CalcPing(client_t *cl, qboolean forcecalc)
{
	float		ping;
	int			i;
	int			count;

	q2client_frame_t *frame;
	ping = 0;
	count = 0;
	for (frame = cl->frameunion.q2frames, i=0 ; i<Q2UPDATE_BACKUP ; i++, frame++)
	{
		if (frame->ping_time > 0)
		{
			ping += frame->ping_time;
			count++;
		}
	}
	if (!count)
		return 9999;
	ping /= count;

	return ping;
}
void SVQ2_ClientWritePing(client_t *client)
{
	client->q2edict->client->ping = SV_CalcPing (client, false);
}

void SVQ2_ClientThink(q2edict_t *ed, usercmd_t *cmd);

void SVQ2_ClientBegin(q2edict_t* ent)
{
	ge->ClientBegin(ent);
}

void SVQ2_ServerCommand(void)
{
	ge->ServerCommand();
}

void SVQ2_WriteLevel(char *filename)
{
	ge->WriteLevel(filename);
}

void SVQ2_WriteGame(char *filename, qboolean autosave)
{
	ge->WriteGame(filename, autosave);
}

void ReadLevel (char *filename)
{
	ge->ReadLevel(filename);
}

void ReadGame (char *filename)
{
	ge->ReadGame(filename);
}

static struct q2gamecode_s q2funcs =
{

#ifdef HAVE_CLIENT
	{
		CLQ2_GatherSounds,
		CLQ2_ParseTEnt,
		CLQ2_AddEntities,
		CLQ2_ParseBaseline,
		CLQ2_ClearParticleState,
		CLR1Q2_ParsePlayerUpdate,
		CLQ2_ParseFrame,
		CLQ2_ParseMuzzleFlash,
		CLQ2_ParseMuzzleFlash2,
		CLQ2_ParseInventory,
		CLQ2_RegisterTEntModels,
		CLQ2_WriteDemoBaselines,
		CL_WriteDemoMessage,
		CLQ2_ParseServerMessage,
		CLQ2_PredictMovement,
		RQ2_DrawNameTags,
	},
/*
	{
		CG_Restart,
		CG_Refresh,
		CG_ConsoleCommand,
		CG_KeyPressed,
		CG_GatherLoopingSounds,
	},

	{
		UI_IsRunning,
		UI_ConsoleCommand,
		UI_Start,
		UI_OpenMenu,
		UI_Reset,
	},
*/
#else
{NULL},//{NULL},{NULL},
#endif

#ifdef HAVE_SERVER
	{
		InitGame,
		SVQ2_InitGameProgs,
		SVQ2_ShutdownGameProgs,
		PFQ2_Configstring,
		SVQ2_BuildClientFrame,
		SVQ2_WriteFrameToClient,
		MSGQ2_WriteDeltaEntity,
		SVQ2_BuildBaselines,
		SVQ2_ExecuteClientMessage,
		SVQ2_Ents_Shutdown,
		SVQ2_ClearEvents,
		SVQ2_ReloadWorld,
		SVQ2_RunFrame,
		WorldQ2_ClearWorld_Nodes,
		SVQ2_HasGameExport,
		ClientCommand,
		UpdateClientNum,
		SpawnEntities,
		ClientDisconnect,
		ClientConnect,
		ClientUserinfoChanged,
		SVQ2_ClientThink,
		SVQ2_ClientBegin,
		SVQ2_CalcPing,
		SVQ2_ClientWritePing,
		SVQ2_ServerCommand,
		SVQ2_WriteLevel,
		SVQ2_WriteGame,
		ReadLevel,
		ReadGame,
		EdictFromNum,
		NumFromEdict,
	},
#else
	{NULL},
#endif
};

#ifdef STATIC_Q2
#define Plug_Init Plug_Q2_Init
#endif

qboolean Plug_Init(void)
{
	if(plugfuncs->ExportInterface("Quake2Plugin", &q2funcs, sizeof(q2funcs)))
	{
		Con_Printf("Engine is already using a q2-derived gamecode plugin.\n");
		return false;
	}

	Iworldfuncs = plugfuncs->GetEngineInterface(plugworldfuncs_name, sizeof(*Iworldfuncs));
	Imsgfuncs = plugfuncs->GetEngineInterface(plugmsgfuncs_name, sizeof(*Imsgfuncs));
	Imodelfuncs = plugfuncs->GetEngineInterface(plugmodfuncs_name, sizeof(*Imodelfuncs));
	Ifilesystemfuncs = plugfuncs->GetEngineInterface(plugfsfuncs_name, sizeof(*Ifilesystemfuncs));

#ifdef HAVE_CLIENT
	Iclientfuncs = plugfuncs->GetEngineInterface(plugclientfuncs_name, sizeof(*Iclientfuncs));
	Iaudiofuncs = plugfuncs->GetEngineInterface(plugaudiofuncs_name, sizeof(*Iaudiofuncs));
#endif

	return true;
}