#include "q2common.h"
#include "clq2defs.h"

extern usercmd_t cl_pendingcmd[MAX_SPLITS];

#ifdef Q2CLIENT

#define	MAX_PARSE_ENTITIES	1024
extern entity_state_t	clq2_parse_entities[MAX_PARSE_ENTITIES];

char *Get_Q2ConfigString(int i);

#ifdef Q2BSPS
void VARGS Q2_Pmove (q2pmove_t *pmove);
#define	Q2PMF_DUCKED			1
#define	Q2PMF_JUMP_HELD		2
#define	Q2PMF_ON_GROUND		4
#define	Q2PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	Q2PMF_TIME_LAND		16	// pm_time is time before rejump
#define	Q2PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define Q2PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)
#endif //Q2BSPS

vec3_t cl_predicted_origins[MAX_SPLITS][UPDATE_BACKUP];

/*
===================
CL_CheckPredictionError
===================
*/
#ifdef Q2BSPS
void CLQ2_CheckPredictionError (void)
{
	int		frame;
	int		delta[3];
	int		i;
	int		len;
	int		seat;
	q2player_state_t *ps;
	playerview_t *pv;
	cvar_t *cl_nopred;

	for (seat = 0; seat < cl.splitclients; seat++)
	{
		ps = &cl.q2frame.playerstate[seat];
		pv = &cl.playerview[seat];

		 cl_nopred = cvarfuncs->GetNVFDG("cl_nopred", NULL, 0, NULL, NULL);

		if (cl_nopred->value || (ps->pmove.pm_flags & Q2PMF_NO_PREDICTION))
			continue;

		// calculate the last usercmd_t we sent that the server has processed
		frame = cls.netchan.incoming_acknowledged;
		frame &= (UPDATE_MASK);

		// compare what the server returned with what we had predicted it to be
		VectorSubtract (ps->pmove.origin, cl_predicted_origins[seat][frame], delta);

		// save the prediction error for interpolation
		len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);
		if (len > 640)	// 80 world units
		{	// a teleport or something
			VectorClear (pv->prediction_error);
		}
		else
		{
//			if (/*cl_showmiss->value && */(delta[0] || delta[1] || delta[2]) )
//				Con_Printf ("prediction miss on %i: %i\n", cl.q2frame.serverframe,
//				delta[0] + delta[1] + delta[2]);

			VectorCopy (ps->pmove.origin, cl_predicted_origins[seat][frame]);

			// save for error itnerpolation
			for (i=0 ; i<3 ; i++)
				pv->prediction_error[i] = delta[i]*0.125;
		}
	}
}


/*
====================
CL_ClipMoveToEntities

====================
*/
int predignoreentitynum;
void CLQ2_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	int			i;
	trace_t		trace;
	float		*angles;
	entity_state_t	*ent;
	int			num;
	model_t		*cmodel;
	vec3_t		bmins, bmaxs;

	for (i=0 ; i<cl.q2frame.num_entities ; i++)
	{
		num = (cl.q2frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &clq2_parse_entities[num];

		if (ent->solidsize == ES_SOLID_NOT)
			continue;

		if (ent->number == predignoreentitynum)
			continue;

		if (ent->solidsize == ES_SOLID_BSP)
		{	// special value for bmodel
			cmodel = cl.model_precache[ent->modelindex];
			if (!cmodel)
				continue;
			angles = ent->angles;
		}
		else
		{	// encoded bbox
			COM_DecodeSize(ent->solidsize, bmins, bmaxs);
			cmodel = CM_TempBoxModel (bmins, bmaxs);
			angles = vec3_origin;	// boxes don't rotate
		}

		if (tr->allsolid)
			return;

		World_TransformedTrace (cmodel, 0, 0, start, end, mins, maxs, false, &trace, ent->origin, angles, MASK_PLAYERSOLID);

		if (trace.allsolid || trace.startsolid || trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
			*tr = trace;
		}
	}
}


/*
================
CL_PMTrace
================
*/
q2trace_t	VARGS CLQ2_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	q2trace_t	q2t;
	trace_t		t;

	// check against world
	cl.worldmodel->funcs.NativeTrace(cl.worldmodel, 0, NULLFRAMESTATE, NULL, start, end, mins, maxs, false, MASK_PLAYERSOLID, &t);
	if (t.fraction < 1.0)
		t.ent = (struct edict_s *)1;

	// check all other solid models
	CLQ2_ClipMoveToEntities (start, mins, maxs, end, &t);

	q2t.allsolid = t.allsolid;
	q2t.contents = t.contents;
	VectorCopy(t.endpos, q2t.endpos);
	q2t.ent = t.ent;
	q2t.fraction = t.fraction;
	q2t.plane = t.plane;
	q2t.startsolid = t.startsolid;
	q2t.surface = t.surface;

	return q2t;
}

int		VARGS CLQ2_PMpointcontents (vec3_t point)
{
	int			i;
	entity_state_t	*ent;
	int			num;
	model_t		*cmodel;
	int			contents;
	vec3_t		axis[3], relpos;

	contents = cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, point);

	for (i=0 ; i<cl.q2frame.num_entities ; i++)
	{
		num = (cl.q2frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &clq2_parse_entities[num];

		if (ent->solidsize != ES_SOLID_BSP) // special value for bmodel
			continue;

		cmodel = cl.model_precache[ent->modelindex];
		if (!cmodel)
			continue;

		AngleVectors (ent->angles, axis[0], axis[1], axis[2]);
		VectorNegate(axis[1], axis[1]);
		VectorSubtract(point, ent->origin, relpos);
		contents |= cmodel->funcs.PointContents(cmodel, axis, relpos);
	}

	return contents;
}

#endif //Q2BSPS

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
static void CLQ2_UserCmdToQ2(q2usercmd_t *out, const usercmd_t *cmd)
{
	out->msec = cmd->msec;
	out->buttons = cmd->buttons;
	VectorCopy(cmd->angles, out->angles);
	out->forwardmove = cmd->forwardmove;
	out->sidemove = cmd->sidemove;
	out->upmove = cmd->upmove;
	out->impulse = cmd->impulse;
	out->lightlevel = cmd->lightlevel;
}
void CLQ2_PredictMovement (int seat)	//q2 doesn't support split clients.
{
#ifdef Q2BSPS
	int			ack, current;
	int			frame;
	int			oldframe;
	q2pmove_t	pm;
	int			step;
	int			oldz;
#endif
	int			i;
	q2player_state_t *ps = &cl.q2frame.playerstate[seat];
	playerview_t *pv = &cl.playerview[seat];
	cvar_t *cl_nopred;

	if (cls.state != ca_active)
		return;

//	if (cl_paused->value)
//		return;
	
#ifdef Q2BSPS
	cl_nopred = cvarfuncs->GetNVFDG("cl_nopred", NULL, 0, NULL, NULL);
	if (cl_nopred->value || cls.demoplayback || (ps->pmove.pm_flags & Q2PMF_NO_PREDICTION))
#endif
	{	// just set angles
		for (i=0 ; i<3 ; i++)
		{
			pv->predicted_angles[i] = pv->viewangles[i] + SHORT2ANGLE(ps->pmove.delta_angles[i]);
		}
		return;
	}
#ifdef Q2BSPS
	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if (current - ack >= UPDATE_MASK)
	{
//		if (cl_showmiss->value)
//			Con_Printf ("exceeded CMD_BACKUP\n");
		return;
	}

	// copy current state to pmove
	memset (&pm, 0, sizeof(pm));
	pm.trace = CLQ2_PMTrace;
	pm.pointcontents = CLQ2_PMpointcontents;

	//pm_airaccelerate = atof(Get_Q2ConfigString(Q2CS_AIRACCEL));

	pm.s = ps->pmove;

//	SCR_DebugGraph (current - ack - 1, 0);

	frame = 0;

	predignoreentitynum = cl.q2frame.clientnum[seat]+1;//cl.playerview[seat].playernum+1;

	// run frames
	while (++ack < current)
	{
		frame = ack & (UPDATE_MASK);
		CLQ2_UserCmdToQ2(&pm.cmd, &cl.outframes[frame].cmd[seat]);
		Q2_Pmove (&pm);

		// save for debug checking
		VectorCopy (pm.s.origin, cl_predicted_origins[seat][frame]);
	}

	if (cl_pendingcmd[seat].msec)
	{
		CLQ2_UserCmdToQ2(&pm.cmd, &cl_pendingcmd[seat]);
		Q2_Pmove (&pm);
	}

	oldframe = (ack-1) & (UPDATE_MASK);
	oldz = cl_predicted_origins[seat][oldframe][2];
	step = pm.s.origin[2] - oldz;
	if (step > 63 && step < 160 && (pm.s.pm_flags & Q2PMF_ON_GROUND) )
	{
		pv->predicted_step = step * 0.125;
		pv->predicted_step_time = realtime;// - host_frametime;// * 0.5;
	}

	pv->onground = !!(pm.s.pm_flags & Q2PMF_ON_GROUND);


	// copy results out for rendering
	pv->predicted_origin[0] = pm.s.origin[0]*0.125;
	pv->predicted_origin[1] = pm.s.origin[1]*0.125;
	pv->predicted_origin[2] = pm.s.origin[2]*0.125;

	VectorScale (pm.s.velocity, 0.125, pv->simvel);
	VectorCopy (pm.viewangles, pv->predicted_angles);
#endif
}

/*
=================
CL_NudgePosition

If pmove.origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
void CL_NudgePosition (void)
{
	vec3_t	base;
	int		x, y;

	if (cl.worldmodel->funcs.PointContents (cl.worldmodel, NULL, pmove.origin) == FTECONTENTS_EMPTY)
		return;

	VectorCopy (pmove.origin, base);
	for (x=-1 ; x<=1 ; x++)
	{
		for (y=-1 ; y<=1 ; y++)
		{
			pmove.origin[0] = base[0] + x * 1.0/8;
			pmove.origin[1] = base[1] + y * 1.0/8;
			if (cl.worldmodel->funcs.PointContents (cl.worldmodel, NULL, pmove.origin) == FTECONTENTS_EMPTY)
				return;
		}
	}
	Con_DPrintf ("CL_NudgePosition: stuck\n");
}


#endif 
