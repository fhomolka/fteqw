
#if defined(Q2CLIENT) || defined(Q2SERVER)
struct sfx_s;
struct server_static_s;
struct server_s;
struct usercmd_s;
struct q2entity_state_s;
struct q2gamecode_s
{
	struct
	{
		unsigned int (*GatherSounds)(vec3_t *positions, unsigned int *entnums, sfx_t **sounds, unsigned int max);
		void (*ParseTEnt)(void);
		void (*AddEntities)(void);
		void (*ParseBaseline)(void);
		void (*ClearParticleState)(void);
		void (*ParsePlayerUpdate) (void);
		void (*ParseFrame) (int extrabits);
		void (*ParseMuzzleFlash)(void);
		void (*ParseMuzzleFlash2)(void);
		void (*ParseInventory)(int seat);
		int (*RegisterTEntModels)(void);
		void (*WriteDemoBaselines)(sizebuf_t *buf);
		//cl_parse.c//
		void (*WriteDemoMessage) (sizebuf_t *msg, int payloadoffset);
		void (*ParseServerMessage)(void);
		//cl_pred.c//
		void (*PredictMovement)(int seat);
		//view.c//
		void (*DrawNameTags)(vec3_t org, vec3_t diff, vec3_t screenspace, 
							//HACK(fhomolka): This is absolutely stupid, but gets it running
							float (*TraceLine) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int *ent),
							int (*DrawTextField)(int x, int y, int w, int h, const char *text, unsigned int defaultmask, unsigned int fieldflags, struct font_s *font, vec2_t fontscale));
	} cl;
/*
	struct
	{
		void			(*VideoRestarted)		(void);
		int				(*Redraw)				(double time);
		qboolean		(*ConsoleCommand)		(void);
		qboolean		(*KeyPressed)			(int key, int unicode, int down);
		unsigned int	(*GatherLoopingSounds)	(vec3_t *positions, unsigned int *entnums, struct sfx_s **sounds, unsigned int max);
	} cg;

	struct
	{
		qboolean (*IsRunning)(void);
		qboolean (*ConsoleCommand)(void);
		void (*Start) (void);
		qboolean (*OpenMenu)(void);
		void (*Reset)(void);
	} ui;
*/
//server stuff
	struct
	{
		//
		// server.h //
		//
		//svq2_game.c
		qboolean	(*InitGame)					(struct server_static_s *server_state_static, struct server_s *server_state);
		qboolean	(*InitGameProgs)			(void);
		void		(*ShutdownGameProgs)		(void);
		void 		(*PF_Configstring) 			(int i, const char *val); //for engine cheats.
		//svq2_ents.c
		void 		(*BuildClientFrame) 		(struct client_s *client);
		void		(*WriteFrameToClient)		(struct client_s *client, sizebuf_t *msg);
//#ifdef Q2SERVER
		void		(*WriteDeltaEntity)			(struct q2entity_state_s *from, struct q2entity_state_s *to, sizebuf_t *msg, qboolean force, qboolean newentity);
		void 		(*BuildBaselines)			(void);
//#endif

		void 		(*ExecuteClientMessage)		(struct client_s *cl);
		//
		// sv_init.c //
		//
		void		(*Ents_Shutdown)			(void);
		//
		// sv_main.c //
		//
		void 		(*ClearEvents)				(void);

// renderer.c //
		void		(*ReloadWorld)				(void);
// sv_phys.c //
		void 		(*RunFrame)					(void);
		void 		(*ClearWorld)				(world_t *w, qboolean relink);
		// pr_cmds.c //
		qboolean 	(*HasGameExport)			(void);

		void 		(*ClientCommand) 			(struct q2edict_s *ent);
		struct q2edict_s* (*UpdateClientNum)	(int num);
		void 		(*SpawnEntities)			(char *mapname, char *entities, char *spawnpoint);
		void 		(*ClientDisconnect)			(struct q2edict_s *ent);
		qboolean 	(*ClientConnect)			(struct q2edict_s *ent, char *userinfo);
		void 		(*ClientUserinfoChanged)	(struct q2edict_s *ent, char *userinfo);
		void 		(*ClientThink)				(q2edict_t *ed, usercmd_t *cmd);
		void 		(*ClientBegin)				(struct q2edict_s *ent);
		int 		(*CalcPing)					(client_t *cl, qboolean forcecalc);
		void 		(*ClientWritePing)			(client_t *cl);
		void 		(*ServerCommand)			(void);
		//sv_main//
		void (*ClientProtocolExtensionsChanged)	(client_t *client);
		//savegame.c//
		void 		(*WriteLevel)				(char *filename);
		void 		(*WriteGame)				(char *filename, qboolean autosave);
		void 		(*ReadLevel) 				(char *filename);
		void 		(*ReadGame)					(char *filename);
		struct q2edict_s* (*Q2EDICT_NUM)		(int i); //HACK(fhomolka): yeah yeah
		int 		(*Q2NUM_FOR_EDICT)			(struct q2edict_s *ent); //HACK(fhomolka): yeah yeah
	} sv;

};

extern struct q2gamecode_s *q2;
#endif
