#include "q2common.h"

typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	int			hitcontentsmask;
	wedict_t		*passedict;
#ifdef Q2SERVER
	q2edict_t	*q2passedict;
#endif
	int			hullnum;
	qboolean	capsule;
} moveclip_t;

#ifdef Q2SERVER

void VARGS WorldQ2_UnlinkEdict(world_t *w, q2edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

void VARGS WorldQ2_LinkEdict(world_t *w, q2edict_t *ent)
{
	areanode_t	*node;

	if (ent->area.prev)
		WorldQ2_UnlinkEdict (w, ent);	// unlink from old position
		
	if (ent == ge->edicts)
		return;		// don't add the world

	if (!ent->inuse)
		return;

	// set the size
	VectorSubtract (ent->maxs, ent->mins, ent->size);
	
	// encode the size into the entity_state for client prediction
	if (ent->solid == Q2SOLID_BBOX && !(ent->svflags & SVF_DEADMONSTER))
	{	// assume that x/y are equal and symetric
		ent->s.solid = COM_EncodeSize(ent->mins, ent->maxs);
		/*
		i = ent->maxs[0]/8;
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (-ent->mins[2])/8;
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (ent->maxs[2]+32)/8;
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		//fixme: 32bit?
		ent->s.solid = (k<<10) | (j<<5) | i;*/
	}
	else if (ent->solid == Q2SOLID_BSP)
	{
		ent->s.solid = ES_SOLID_BSP;		// a solid_bbox will never create this value
	}
	else
		ent->s.solid = 0;

	// set the abs box
	if (ent->solid == Q2SOLID_BSP && 
	(ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2]) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v =fabs( ent->mins[i]);
			if (v > max)
				max = v;
			v =fabs( ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			ent->absmin[i] = ent->s.origin[i] - max;
			ent->absmax[i] = ent->s.origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->s.origin, ent->mins, ent->absmin);	
		VectorAdd (ent->s.origin, ent->maxs, ent->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->absmin[0] -= 1;
	ent->absmin[1] -= 1;
	ent->absmin[2] -= 1;
	ent->absmax[0] += 1;
	ent->absmax[1] += 1;
	ent->absmax[2] += 1;

// link to PVS leafs
	{
		pvscache_t cache;
		w->worldmodel->funcs.FindTouchedLeafs(w->worldmodel, &cache, ent->absmin, ent->absmax);

		//evilness: copy into the q2 state (we don't have anywhere else to store it, and there's a chance that the gamecode will care).
		ent->num_clusters = cache.num_leafs;
		if (ent->num_clusters > (int)countof(ent->clusternums))
			ent->num_clusters = (int)countof(ent->clusternums);
		memcpy(ent->clusternums, cache.leafnums, min(sizeof(ent->clusternums), sizeof(cache.leafnums)));
		ent->headnode = cache.headnode;
		ent->areanum = cache.areanum;
		ent->areanum2 = cache.areanum2;
	}

	// if first time, make sure old_origin is valid
	if (!ent->linkcount)
	{
		VectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->linkcount++;

	if (ent->solid == Q2SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = w->areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

	// link it in	
	InsertLinkBefore (&ent->area, &node->edicts);
}

static areanode_t *WorldQ2_CreateAreaNode (world_t *w, int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &w->areanodes[w->numareanodes];
	w->numareanodes++;

	ClearLink (&anode->edicts);

	VectorSubtract (maxs, mins, size);

	if (depth == w->areanodedepth || (size[0] < 512 && size[1] < 512))
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = WorldQ2_CreateAreaNode (w, depth+1, mins2, maxs2);
	anode->children[1] = WorldQ2_CreateAreaNode (w, depth+1, mins1, maxs1);

	return anode;
}

void WorldQ2_ClearWorld_Nodes (world_t *w, qboolean relink)
{
	int maxdepth;
	vec3_t mins, maxs;
	if (w->worldmodel)
	{
		VectorCopy(w->worldmodel->mins, mins);
		VectorCopy(w->worldmodel->maxs, maxs);
	}
	else
	{
		VectorSet(mins, -4096, -4096, -4096);
		VectorSet(maxs, 4096, 4096, 4096);
	}


#if 0 // !defined(USEAREAGRID) //NOTE(fhomolka): Did this ever run? Check world.h:234
	memset (&w->portallist, 0, sizeof(w->portallist));
	ClearLink (&w->portallist.edicts);
	w->portallist.axis = -1;
#endif

	maxdepth = 8;

	if (!w->areanodes || w->areanodedepth != maxdepth)
	{
		Z_Free(w->areanodes);
		w->areanodedepth = maxdepth;
		w->areanodes = Z_Malloc(sizeof(*w->areanodes) * (pow(2, w->areanodedepth+1)-1));
	}
	else
		memset (w->areanodes, 0, sizeof(*w->areanodes)*w->numareanodes);
	w->numareanodes = 0;
	WorldQ2_CreateAreaNode (w, 0, mins, maxs);
#if 0
	int i;
	wedict_t *ent;
	if (relink)
	{
		for (i=0 ; i<w->num_edicts ; i++)
		{
			ent = WEDICT_NUM_PB(w->progs, i);
			if (!ent)
				continue;
			ent->area.prev = ent->area.next = NULL;
			if (ED_ISFREE(ent))
				continue;
			WorldQ2_LinkEdict (w, ent);	// relink ents so touch functions continue to work.
		}
	}
#endif
}




#endif

#ifdef Q2SERVER
const float	*area_mins, *area_maxs;
q2edict_t	**area_q2list;
int		area_count, area_maxcount;
int		area_type;
static void WorldQ2_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	q2edict_t		*check;

	// touch linked edicts
	start = &node->edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		if (!l)
		{
			int i;
			WorldQ2_ClearWorld_Nodes(&sv.world, false);
			check = ge->edicts;
			for (i = 0; i < ge->num_edicts; i++, check = (q2edict_t	*)((char *)check + ge->edict_size))
				memset(&check->area, 0, sizeof(check->area));
			Con_Printf ("SV_AreaEdicts: Bad links\n");
			return;
		}
		next = l->next;
		check = Q2EDICT_FROM_AREA(l);

		if (check->solid == Q2SOLID_NOT)
			continue;		// deactivated

		/*q2 still has solid/trigger lists, emulate that here*/
		if ((check->solid == Q2SOLID_TRIGGER) != (area_type == AREA_TRIGGER))
			continue;

		if (check->absmin[0] > area_maxs[0]
		|| check->absmin[1] > area_maxs[1]
		|| check->absmin[2] > area_maxs[2]
		|| check->absmax[0] < area_mins[0]
		|| check->absmax[1] < area_mins[1]
		|| check->absmax[2] < area_mins[2])
			continue;		// not touching

		if (area_count == area_maxcount)
		{
			Con_Printf ("SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_q2list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return;		// terminal node

	// recurse down both sides
	if ( area_maxs[node->axis] > node->dist )
		WorldQ2_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		WorldQ2_AreaEdicts_r ( node->children[1] );
}

int VARGS WorldQ2_AreaEdicts (world_t *w, const vec3_t mins, const vec3_t maxs, q2edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_q2list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	WorldQ2_AreaEdicts_r (w->areanodes);

	return area_count;
}
#endif

#ifdef Q2SERVER
static model_t *WorldQ2_ModelForEntity (world_t *w, q2edict_t *ent)
{
	model_t	*model;

// decide which clipping hull to use, based on the size
	if (ent->solid == Q2SOLID_BSP)
	{	// explicit hulls in the BSP model
		model = w->Get_CModel(w, ent->s.modelindex);

		if (!model)
			SV_Error ("Q2SOLID_BSP with a non bsp model");

		if (model->loadstate == MLS_LOADED)
			return model;
	}

	// create a temp hull from bounding box sizes

	return CM_TempBoxModel (ent->mins, ent->maxs);
}
#endif

#ifdef Q2SERVER
void WorldQ2_ClipMoveToEntities (world_t *w, moveclip_t *clip )
{
	int			i, num;
	q2edict_t		*touchlist[MAX_Q2EDICTS], *touch;
	trace_t		trace;
	model_t		*model;
	float		*angles;

	num = WorldQ2_AreaEdicts (w, clip->boxmins, clip->boxmaxs, touchlist
		, MAX_Q2EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		touch = touchlist[i];
		if (touch->solid == Q2SOLID_NOT)
			continue;
		if (touch == clip->q2passedict)
			continue;
		if (clip->trace.allsolid)
			return;
		if (clip->q2passedict)
		{
		 	if (touch->owner == clip->q2passedict)
				continue;	// don't clip against own missiles
			if (clip->q2passedict->owner == touch)
				continue;	// don't clip against owner
		}

		if (touch->svflags & SVF_DEADMONSTER)
		if ( !(clip->hitcontentsmask & Q2CONTENTS_DEADMONSTER))
				continue;

		// might intersect, so do an exact clip
		model = WorldQ2_ModelForEntity (w, touch);
		angles = touch->s.angles;
		if (touch->solid != Q2SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if (touch->svflags & SVF_MONSTER)
			Iworldfuncs->TransformedTrace(model, 0, NULL, clip->start, clip->end, clip->mins2, clip->maxs2, false, &trace, touch->s.origin, angles, clip->hitcontentsmask);
		else
			Iworldfuncs->TransformedTrace(model, 0, NULL, clip->start, clip->end, clip->mins, clip->maxs, false, &trace, touch->s.origin, angles, clip->hitcontentsmask);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = (edict_t *)touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
			{
				clip->trace = trace;
			}
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}
#undef ped
}
#endif

#ifdef Q2SERVER

static void WorldQ2_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;
	
	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

trace_t WorldQ2_Move (world_t *w, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hitcontentsmask, q2edict_t *passedict)
{
	moveclip_t	clip;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

// clip to world
	w->worldmodel->funcs.NativeTrace(w->worldmodel, 0, NULLFRAMESTATE, NULL, start, end, mins, maxs, false, hitcontentsmask, &clip.trace);
	clip.trace.ent = ge->edicts;

	if (clip.trace.fraction == 0)
		return clip.trace;

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = MOVE_NORMAL;
	clip.hitcontentsmask = hitcontentsmask;
	clip.passedict = NULL;
	clip.q2passedict = passedict;

	VectorCopy (mins, clip.mins2);
	VectorCopy (maxs, clip.maxs2);
	
// create the bounding box of the entire move
//FIXME: should we use clip.trace.endpos here?	
	WorldQ2_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
	WorldQ2_ClipMoveToEntities(w, &clip);

	return clip.trace;
}
#endif 
