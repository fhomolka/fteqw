#include "q2common.h"
#ifdef Q2SERVER
/*
void SVQ2_BaseLines_f (void)
{
	int		start;
	q2entity_state_t	nullstate;
	q2entity_state_t	*base;

	extern q2entity_state_t	sv_baselines[Q2MAX_EDICTS];

	Con_DPrintf ("Baselines() from %s\n", host_client->name);

	if (host_client->state != cs_connected)
	{
		Con_Printf ("baselines not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Con_Printf ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data

	while ( host_client->netchan.message.cursize <  host_client->netchan.message.maxsize/2
		&& start < Q2MAX_EDICTS)
	{
		base = &sv_baselines[start];
		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte (&host_client->netchan.message, svcq2_spawnbaseline);
			MSGQ2_WriteDeltaEntity (&nullstate, base, &host_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command

	if (start == Q2MAX_EDICTS)
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&host_client->netchan.message, svcq2_stufftext);
		MSG_WriteString (&host_client->netchan.message, va("cmd baselines %i %i\n",svs.spawncount, start) );
	}
}

void SVQ2_NextServer_f (void)
{
	if (!*sv.modelname && atoi(Cmd_Argv(1)) == svs.spawncount)
	{
		cvar_t *nsv = Cvar_FindVar("nextserver");
		if (!nsv || !*nsv->string)
			return;

		svs.spawncount++;	// make sure another doesn't sneak in

		Cbuf_AddText(nsv->string, RESTRICT_LOCAL);
		Cbuf_AddText("\n", RESTRICT_LOCAL);
		Cvar_Set(nsv, "");
	}
}
*/
#endif

//#ifdef Q2SERVER
void SVQ2_ClientThink(q2edict_t *ed, usercmd_t *cmd)
{
	q2usercmd_t q2;
	q2.msec = cmd->msec;
	q2.buttons = cmd->buttons;
	VectorCopy(cmd->angles, q2.angles);
	q2.forwardmove = cmd->forwardmove;
	q2.sidemove = cmd->sidemove;
	q2.upmove = cmd->upmove;
	q2.impulse = cmd->impulse;
	q2.lightlevel = cmd->lightlevel;
	ge->ClientThink (ed, &q2);
}
void SVQ2_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
	usercmd_t	oldest, oldcmd, newcmd;
	q2client_frame_t	*frame;
	int		move_issued = 0; //only allow one move command
	int		checksumIndex;
	qbyte	checksum, calculatedChecksum;
	int		seq_hash;
	int lastframe;
	client_t *split;

	if (!ge)
	{
		Con_Printf("Q2 client without Q2 server\n");
		SV_DropClient(cl);
	}

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped

	// calc ping time
	if (cl->netchan.outgoing_sequence - cl->netchan.incoming_acknowledged > Q2UPDATE_MASK)
	{
		cl->delay -= 0.001;
		if (cl->delay < 0)
			cl->delay = 0;
	}
	else
	{
		frame = &cl->frameunion.q2frames[cl->netchan.incoming_acknowledged & Q2UPDATE_MASK];
		if (frame->senttime != -1)
		{
			cvar_t *sv_minping = cvarfuncs->GetNVFDG("sv_minping", NULL, 0, NULL, NULL);
			int ping_time = (int)(realtime*1000) - frame->senttime;	//no more phenomanally low pings please
			if (ping_time > sv_minping->value+1)
			{
				cl->delay -= 0.001;
				if (cl->delay < 0)
					cl->delay = 0;
			}
			if (ping_time < sv_minping->value)
			{
				cl->delay += 0.001;
				if (cl->delay > 1)
					cl->delay = 1;
			}
			frame->senttime = -1;
			frame->ping_time = ping_time;
		}
	}

	// save time for ping calculations
//	cl->frameunion.q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].senttime = realtime*1000;
//	cl->frameunion.q2frames[cl->netchan.outgoing_sequence & Q2UPDATE_MASK].ping_time = -1;

	host_client = cl;
	sv_player = host_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
 	cl->localtime = sv.time;
	cl->delta_sequence = -1;	// no delta unless requested
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SVQ2_ExecuteClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		safeswitch ((enum clcq2_ops_e)c)
		{
		case clcq2_nop:
			break;

		case clcq2_move:
			if (move_issued >= MAX_SPLITS)
				return;		// someone is trying to cheat...

			for (checksumIndex = 0, split = cl; split && checksumIndex < move_issued; checksumIndex++)
				split = split->controlled;

			if (move_issued)
			{
				checksumIndex = -1;
				checksum = 0;
			}
			else
			{
				checksumIndex = MSG_GetReadCount();
				checksum = (qbyte)MSG_ReadByte ();


				lastframe = MSG_ReadLong();
				if (lastframe != split->delta_sequence)
				{
					split->delta_sequence = lastframe;
				}
			}

			MSGQ2_ReadDeltaUsercmd (&nullcmd, &oldest);
			MSGQ2_ReadDeltaUsercmd (&oldest, &oldcmd);
			MSGQ2_ReadDeltaUsercmd (&oldcmd, &newcmd);

			if ( split && split->state == cs_spawned )
			{
				if (checksumIndex != -1)
				{
					// if the checksum fails, ignore the rest of the packet
					calculatedChecksum = Q2COM_BlockSequenceCRCByte(
						net_message.data + checksumIndex + 1,
						MSG_GetReadCount() - checksumIndex - 1,
						seq_hash);

					if (calculatedChecksum != checksum)
					{
						Con_DPrintf ("Failed command checksum for %s(%d) (%d != %d)\n",
							cl->name, cl->netchan.incoming_sequence, checksum, calculatedChecksum);
						return;
					}
				}

				if (split->penalties & BAN_CRIPPLED)
				{
					split->lastcmd.forwardmove = 0;	//hmmm.... does this work well enough?
					oldest.forwardmove = 0;
					newcmd.forwardmove = 0;

					split->lastcmd.sidemove = 0;
					oldest.sidemove = 0;
					newcmd.sidemove = 0;

					split->lastcmd.upmove = 0;
					oldest.upmove = 0;
					newcmd.upmove = 0;
				}

				split->q2edict->client->ping = SV_CalcPing (split, false);
				if (!sv.paused)
				{
					if (net_drop < 20)
					{
						while (net_drop > 2)
						{
							SVQ2_ClientThink (split->q2edict, &split->lastcmd);
							net_drop--;
						}
						if (net_drop > 1)
							SVQ2_ClientThink (split->q2edict, &oldest);
						if (net_drop > 0)
							SVQ2_ClientThink (split->q2edict, &oldcmd);
					}
					SVQ2_ClientThink (split->q2edict, &newcmd);
				}

				split->lastcmd = newcmd;
			}
			move_issued++;
			break;

		case clcq2_userinfo:
			//FIXME: allows the client to set * keys mid-game.
			s = MSG_ReadString();
			InfoBuf_FromString(&cl->userinfo, s, false);
			SV_ExtractFromUserinfo(cl, true);	//let the server routines know
			ge->ClientUserinfoChanged (cl->q2edict, s);	//tell the gamecode
			break;

		case clcq2_stringcmd_seat:
			c = MSG_ReadByte();
			host_client = cl;
			while (c --> 0 && host_client->controlled)
				host_client = host_client->controlled;
			sv_player = host_client->edict;
			//fall through
		case clcq2_stringcmd:
			s = MSG_ReadString ();
			SV_ExecuteUserCommand (s, false);

			host_client = cl;
			sv_player = cl->edict;

			if (cl->state < cs_connected)
				return;	// disconnect command
			break;

#ifdef VOICECHAT
		case clcq2_voicechat:
			SV_VoiceReadPacket();
			break;
#endif

		case clcq2_bad:
		case clcr1q2_setting:
		case clcr1q2_multimoves:
		safedefault:
			Con_Printf ("SVQ2_ReadClientMessage: unknown command char %i\n", c);
			SV_DropClient (cl);
			return;
		}
	}
}
//#endif