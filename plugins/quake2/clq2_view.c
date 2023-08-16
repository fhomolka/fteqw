#include "q2common.h"

void RQ2_DrawNameTags(vec3_t org, vec3_t diff, vec3_t screenspace, 
	//HACK(fhomolka): This is absolutely stupid, but gets it running
	float (*TraceLine) (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int *ent),
	int (*DrawTextField)(int x, int y, int w, int h, const char *text, unsigned int defaultmask, unsigned int fieldflags, struct font_s *font, vec2_t fontscale))
{
	int i = 0;
	if(!ge) return;
	
	struct q2edict_s *e;

	int best = 0;
	float bestscore = 0, score = 0;
	for (i = 1; i < ge->num_edicts; i++)
	{
		e = &ge->edicts[i];
		if (!e->inuse)
			continue;
		VectorInterpolate(e->mins, 0.5, e->maxs, org);
		VectorAdd(org, e->s.origin, org);
		VectorSubtract(org, r_refdef.vieworg, diff);
		if (DotProduct(diff, diff) < 16*16)
			continue;	//ignore stuff too close(like the player themselves)
		VectorNormalize(diff);
		score = DotProduct(diff, vpn);// r_refdef.viewaxis[0]);
		if (score > bestscore)
		{
			int hitent;
			vec3_t imp;
			if (TraceLine(r_refdef.vieworg, org, imp, NULL, &hitent)>=1 || hitent == i)
			{
				best = i;
				bestscore = score;
			}
		}
	}
	if (best)
	{
		e = &ge->edicts[best];
		VectorInterpolate(e->mins, 0.5, e->maxs, org);
		VectorAdd(org, e->s.origin, org);
		if (Matrix4x4_CM_Project(org, screenspace, r_refdef.viewangles, r_refdef.vieworg, r_refdef.fov_x, r_refdef.fov_y))
		{
			char *entstr = va("entity %i {\n\tmodelindex %i\n\torigin \"%f %f %f\"\n}\n", e->s.number, e->s.modelindex, e->s.origin[0], e->s.origin[1], e->s.origin[2]);
			if (entstr)
			{
				vec2_t scale = {8,8};
				int x = screenspace[0]*r_refdef.vrect.width+r_refdef.vrect.x;
				int y = (1-screenspace[1])*r_refdef.vrect.height+r_refdef.vrect.y;
				DrawTextField(x, y, vid.width - x, vid.height - y, entstr, CON_WHITEMASK, CPRINT_TALIGN|CPRINT_LALIGN, font_console, scale);
			}
		}
	}

}