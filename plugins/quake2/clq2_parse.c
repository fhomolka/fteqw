#include "q2common.h"
#include <zlib.h>

#ifdef Q2CLIENT
static void CLQ2_ParseServerData (void)
{
	char	*str;
	int		i;
	int svcnt;
//	int cflag;

	memset(&cls.netchan.netprim, 0, sizeof(cls.netchan.netprim));
	cls.netchan.netprim.coordtype = COORDTYPE_FIXED_13_3;
	cls.netchan.netprim.anglesize = 1;
	cls.fteprotocolextensions = 0;
	cls.fteprotocolextensions2 = 0;
	cls.ezprotocolextensions1 = 0;
	cls.demohadkeyframe = true;	//assume that it did, so this stuff all gets recorded.

	Con_DPrintf ("Serverdata packet %s.\n", cls.demoplayback?"read":"received");
//
// wipe the client_state_t struct
//
	//TODO(fhomolka): plug interface

	SCR_SetLoadingStage(LS_CLIENT);
	SCR_BeginLoadingPlaque();

//	CL_ClearState ();
	cls.state = ca_onserver;

// parse protocol version number
	i = Imsgfuncs->ReadLong ();

	if (i == PROTOCOL_VERSION_FTE1)
	{
		cls.fteprotocolextensions = i = Imsgfuncs->ReadLong();
//		if (i & PEXT_FLOATCOORDS)
//			i -= PEXT_FLOATCOORDS;
		if (i & PEXT_SOUNDDBL)
			i -= PEXT_SOUNDDBL;
		if (i & PEXT_MODELDBL)
			i -= PEXT_MODELDBL;
		if (i & PEXT_SPLITSCREEN)
			i -= PEXT_SPLITSCREEN;
		if (i)
			plugfuncs->EndGame ("Unsupported q2 protocol extensions: %x", i);
		i = Imsgfuncs->ReadLong ();

		if (cls.fteprotocolextensions & PEXT_FLOATCOORDS)
			cls.netchan.netprim.coordtype = COORDTYPE_FLOAT_32;
	}
	cls.protocol_q2 = i;

	if (i == PROTOCOL_VERSION_R1Q2)
		Con_DPrintf("Using R1Q2 protocol\n");
	else if (i == PROTOCOL_VERSION_Q2PRO)
		Con_DPrintf("Using Q2PRO protocol\n");
	else if (i > PROTOCOL_VERSION_Q2 || i < (cls.demoplayback?PROTOCOL_VERSION_Q2_DEMO_MIN:PROTOCOL_VERSION_Q2_MIN))
		plugfuncs->EndGame ("Q2 Server returned version %i, not %i", i, PROTOCOL_VERSION_Q2);

	svcnt = Imsgfuncs->ReadLong ();
	/*cl.attractloop =*/ Imsgfuncs->ReadByte ();

	// game directory
	str = Imsgfuncs->ReadString ();
	// set gamedir
	if (!*str)
		COM_Gamedir("baseq2", NULL);
	else
		COM_Gamedir(str, NULL);

	cvarfuncs->GetNVFDG("timescale", "1", 0, NULL, "Q2Admin hacks");
	//Cvar_Get("timescale", "1", 0, "Q2Admin hacks");	//Q2Admin will kick players who have a timescale set to something other than 1
													//FTE doesn't actually have a timescale cvar, so create one to 'fool' q2admin.
													//I can't really blame q2admin for rejecting engines that don't have this cvar, as it could have been renamed via a hex-edit.

	//CL_ClearState (true);
	CLQ2_ClearState ();
	cl.minpitch = -89;
	cl.maxpitch = 89;
	cl.servercount = svcnt;
	//Cam_AutoTrack_Update(NULL);
	
/*
#ifdef QUAKEHUD
	Stats_NewMap();
#endif
*/

	// parse player entity number
	cl.playerview[0].playernum = Imsgfuncs->ReadShort ();
	cl.playerview[0].viewentity = cl.playerview[0].playernum+1;
	cl.playerview[0].spectator = false;
	cl.splitclients = 1;

	cl.numq2visibleweapons = 1;	//give it a default.
	cl.q2visibleweapons[0] = "weapon.md2";
	cl.q2svnetrate = 10;

	// get the full level name
	str = Imsgfuncs->ReadString ();
	Q_strncpyz (cl.levelname, str, sizeof(cl.levelname));


	if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2)
	{
		unsigned short r1q2ver;
		qboolean isenhanced = Imsgfuncs->ReadByte();
		if (isenhanced)
			plugfuncs->EndGame ("R1Q2 server is running an unsupported mod");
		r1q2ver = Imsgfuncs->ReadShort();	//protocol version... limit... yeah, buggy.
		if (r1q2ver > 1905)
			plugfuncs->EndGame ("R1Q2 server version %i not supported", r1q2ver);

		if (r1q2ver >= 1903)
		{
			Imsgfuncs->ReadByte();	//'used to be advanced deltas'
			Imsgfuncs->ReadByte(); //strafejump hack
		}
		if (r1q2ver >= 1904)
			cls.netchan.netprim.flags |= NPQ2_R1Q2_UCMD;
		if (r1q2ver >= 1905)
			cls.netchan.netprim.flags |= NPQ2_SOLID32;
	}
	else if (cls.protocol_q2 == PROTOCOL_VERSION_Q2PRO)
	{
		unsigned short q2prover = Imsgfuncs->ReadShort();	//q2pro protocol version
		if (q2prover < 1011 || q2prover > 1021)
			plugfuncs->EndGame ("Q2PRO server version %i not supported", q2prover);
		Imsgfuncs->ReadByte();	//server state (ie: demo playback vs actual game)
		Imsgfuncs->ReadByte(); //strafejump hack
		Imsgfuncs->ReadByte(); //q2pro qw-mode. kinda silly for us tbh.
		if (q2prover >= 1014)
			cls.netchan.netprim.flags |= NPQ2_SOLID32;
		if (q2prover >= 1018)
			cls.netchan.netprim.flags |= NPQ2_ANG16;
		if (q2prover >= 1015)
			Imsgfuncs->ReadByte();	//some kind of waterjump hack enable
	}

	cls.netchan.message.prim = cls.netchan.netprim;
	//TODO(fhomolka): plug interface
	MSG_ChangePrimitives(cls.netchan.netprim);

	if (cl.playerview[0].playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		//TODO(fhomolka): plug interface
		SCR_EndLoadingPlaque();
		CL_MakeActive("Quake2");
		if (!FCheckExists(str) && !FCheckExists(va("video/%s", str)))
		{
			int i;
			char basename[64], *t;
			char *exts[] = {".ogv", ".roq", ".cin"};
			//NOTE(fhomolka): Yeah, i dunno why this is in Imodelfuncs
			Imodelfuncs->StripExtension(COMQ2_SkipPath(str), basename, sizeof(basename));
			for(i = 0; i < countof(exts); i++)
			{
				t = va("video/%s%s", basename, exts[i]);
				if (FCheckExists(t))
				{
					str = t;
					break;
				}
			}
		}
		//TODO(fhomolka):
		/*
		if (!Media_PlayFilm(str, false))
		{
			NYI("Playing films");
			CL_SendClientCommand(true, "nextserver %i", cl.servercount);
		}
		*/
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Con_TPrintf ("\n\n^Ue01d^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01e^Ue01f\n\n");
		Con_Printf ("%c%s\n", 2, str);

		//TODO(fhomolka): Media_StopFilm(true);

		// need to prep refresh at next oportunity
		//cl.refresh_prepped = false;
	}

	Cvar_ForceCallback(Cvar_FindVar("r_particlesdesc"));

	Surf_PreNewMap();
	CL_CheckServerInfo();
}
#endif

#ifdef Q2CLIENT

void CLQ2_ParseServerMessage (void);

static void CLQ2_ParseZPacket(void)
{
#ifndef AVAIL_ZLIB
	plugfuncs->EndGame ("CLQ2_ParseZPacket: zlib not supported in this build");
#else
	z_stream s;
	char *indata, *outdata;	//we're hacking stuff onto the end of the current buffer, to avoid issues if something errors out and doesn't leave net_message in a clean state
	unsigned short clen = Imsgfuncs->ReadShort();
	unsigned short ulen = Imsgfuncs->ReadShort();
	sizebuf_t restoremsg;
	if (clen > net_message.cursize-MSG_GetReadCount())
		plugfuncs->EndGame ("CLQ2_ParseZPacket: svcr1q2_zpacket truncated");
	if (ulen > net_message.maxsize-net_message.cursize)
		plugfuncs->EndGame ("CLQ2_ParseZPacket: svcr1q2_zpacket overflow");
	indata = net_message.data + Imsgfuncs->ReadCount();
	outdata = net_message.data + net_message.cursize;
	Imsgfuncs->ReadSkip(clen);
	restoremsg = net_message;
	net_message.currentbit = net_message.cursize<<3;
	net_message.cursize += ulen;

	memset(&s, 0, sizeof(s));
	s.next_in = indata;
	s.avail_in = clen;
	s.total_in = 0;
	s.next_out = outdata;
	s.avail_out = ulen;
	s.total_out = 0;
	if (inflateInit2(&s, -15) != Z_OK)
		plugfuncs->EndGame ("CLQ2_ParseZPacket: unable to initialise zlib");
	if (inflate(&s, Z_FINISH) != Z_STREAM_END)
		plugfuncs->EndGame ("CLQ2_ParseZPacket: stream truncated");
	if (inflateEnd(&s) != Z_OK)
		plugfuncs->EndGame ("CLQ2_ParseZPacket: stream truncated");
	if (s.total_out != ulen || s.total_in != clen)
		plugfuncs->EndGame ("CLQ2_ParseZPacket: stream truncated");

	CLQ2_ParseServerMessage();
	net_message = restoremsg;
	msg_badread = false;
#endif
}
static void CLR1Q2_ParseSetting(void)
{
	int setting = Imsgfuncs->ReadLong();
	int value = Imsgfuncs->ReadLong();

	if (setting == R1Q2_SVSET_FPS)
	{
		cl.q2svnetrate = value;
		if (cl.validsequence)
			Con_Printf("warning: fps rate changed mid-game\n");	//fixme: we need to clean up lerping stuff. if its now lower, we might have a whole load of things waiting ages for a timeout.
	}
}


static void CLQ2_ParseStartSoundPacket(void)
{
	vec3_t  pos_v;
	float	*pos;
	int 	channel, ent;
	int 	sound_num;
	float 	volume;
	float 	attenuation;
	int		flags;
	float	ofs;
	sfx_t	*sfx;

	flags = Imsgfuncs->ReadByte ();

	if ((flags & Q2SND_LARGEIDX) && (cls.fteprotocolextensions & PEXT_SOUNDDBL))
		sound_num = Imsgfuncs->ReadShort();
	else
		sound_num = Imsgfuncs->ReadByte ();

	if (flags & Q2SND_VOLUME)
		volume = Imsgfuncs->ReadByte () / 255.0;
	else
		volume = Q2DEFAULT_SOUND_PACKET_VOLUME;

	if (flags & Q2SND_ATTENUATION)
		attenuation = Imsgfuncs->ReadByte () / 64.0;
	else
		attenuation = Q2DEFAULT_SOUND_PACKET_ATTENUATION;

	if (flags & Q2SND_OFFSET)
		ofs = Imsgfuncs->ReadByte () / 1000.0;
	else
		ofs = 0;

	if (flags & Q2SND_ENT)
	{	// entity reletive
		channel = Imsgfuncs->ReadShort();
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			plugfuncs->EndGame ("CLQ2_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & Q2SND_POS)
	{	// positioned in space
		if ((flags & Q2SND_LARGEPOS) && (cls.fteprotocolextensions & PEXT_FLOATCOORDS))
		{
			pos_v[0] = Imsgfuncs->ReadFloat();
			pos_v[1] = Imsgfuncs->ReadFloat();
			pos_v[2] = Imsgfuncs->ReadFloat();
		}
		else
		{
			pos_v[0] = Imsgfuncs->ReadCoord();
			pos_v[1] = Imsgfuncs->ReadCoord();
			pos_v[2] = Imsgfuncs->ReadCoord();
		}

		pos = pos_v;
	}
	else	// use entity number
	{
		CLQ2_GetNumberedEntityInfo(ent, pos_v, NULL);
		pos = pos_v;
//		pos = NULL;
	}

	if (!cl.sound_precache[sound_num])
		return;

	sfx = cl.sound_precache[sound_num];
	if (sfx->name[0] == '*')
	{	//a 'sexed' sound
		if (ent > 0 && ent <= MAX_CLIENTS)
		{
			char *model = Iworldfuncs->GetIBufKey(&cl.players[ent-1].userinfo, "skin");
			char *skin;
			skin = strchr(model, '/');
			if (skin)
				*skin = '\0';
			if (*model)
				sfx = Iaudiofuncs->PrecacheSound(va("players/%s/%s", model, cl.sound_precache[sound_num]->name+1));
		}
		//fall back to male if it failed to load.
		//note: threaded loading can still make it silent the first time we hear it.
		if (sfx->loadstate == SLS_FAILED)
			sfx = Iaudiofuncs->PrecacheSound(va("players/male/%s", cl.sound_precache[sound_num]->name+1));
	}
	Iaudiofuncs->StartSound (ent, channel, sfx, pos, NULL, volume, attenuation, ofs, 0, 0);
}




void CL_WriteDemoMessage (sizebuf_t *msg, int payloadoffset);


#define SHOWNETEOM(x) if(cl_shownet.value>=2)Con_Printf ("%3i:%s\n", Imsgfuncs->ReadCount(), x);
#define SHOWNET(x) if(cl_shownet.value>=2)Con_Printf ("%3i:%s\n", Imsgfuncs->ReadCount()-1, x);
#define SHOWNET2(x, y) if(cl_shownet.value>=2)Con_Printf ("%3i:%3i:%s\n", Imsgfuncs->ReadCount()-1, y, x);

void CLQ2_ParseServerMessage (void)
{
	int				cmd;
	char			*s;
	int				i;
	unsigned int	seat;
//	int				j;
	int startpos = Imsgfuncs->ReadCount();

	cl.last_servermessage = realtime;
	CL_ClearProjectiles ();

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf ("%i ",net_message.cursize);
	else if (cl_shownet.value >= 2)
		Con_Printf ("------------------\n");


	CL_ParseClientdata ();

//
// parse the message
//
	while (1)
	{
		if (msg_badread)
		{
			plugfuncs->EndGame ("CLQ2_ParseServerMessage: Bad server message");
			break;
		}

		cmd = Imsgfuncs->ReadByte ();

		seat = 0;
		if (cmd == svcq2_playerinfo && (cls.fteprotocolextensions & PEXT_SPLITSCREEN))
		{	//playerinfo should not normally be seen here.
			//so we can just 'borrow' it for seat numbers for targetted svcs.
			SHOWNET(va("%i", cmd));
			seat = Imsgfuncs->ReadByte ();
			if (seat >= MAX_SPLITS)
				plugfuncs->EndGame ("CLQ2_ParseServerMessage: Unsupported seat (%i)", seat);
			cmd = Imsgfuncs->ReadByte ();
		}

		if (cmd == -1)
		{
			SHOWNETEOM("END OF MESSAGE");
			break;
		}

		SHOWNET(va("%i", cmd));

	// other commands
		switch (cmd)
		{
		default:
isilegible:
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2 || cls.protocol_q2 == PROTOCOL_VERSION_Q2PRO)
			{
				switch(cmd & 0x1f)
				{
				case svcq2_frame:			//20 (the bastard to implement.)
					CLQ2_ParseFrame(cmd>>5);
					break;
				default:
					plugfuncs->EndGame ("CLQ2_ParseServerMessage: Illegible server message (%i)", cmd);
					return;
				}
				break;
			}
			plugfuncs->EndGame ("CLQ2_ParseServerMessage: Illegible server message (%i)", cmd);
			return;

	//known to game
		case svcq2_muzzleflash:
			CLQ2_ParseMuzzleFlash();
			break;
		case svcq2_muzzleflash2:
			CLQ2_ParseMuzzleFlash2();
			return;
		case svcq2_temp_entity:
			CLQ2_ParseTEnt();
			break;
		case svcq2_layout:
			s = MSG_ReadString ();
			Q_strncpyz (cl.q2layout[seat], s, sizeof(cl.q2layout[seat]));
			break;
		case svcq2_inventory:
			CLQ2_ParseInventory(seat);
			break;

	// the rest are private to the client and server
		case svcq2_nop:			//6
			plugfuncs->EndGame ("CL_ParseServerMessage: svcq2_nop not implemented");
			return;
		case svcq2_disconnect:
			if (cls.state == ca_connected)
				plugfuncs->EndGame ("Server disconnected\n"
					"Server version may not be compatible");
			else
				plugfuncs->EndGame ("Server disconnected");
			return;
		case svcq2_reconnect:	//8. this is actually kinda weird to have
			Con_TPrintf ("reconnecting...\n");
#if 1
			CL_Disconnect("Reconnect request");
			CL_BeginServerReconnect();
			return;
#else
			CL_SendClientCommand(true, "new");
			break;
#endif
		case svcq2_sound:		//9			// <see code>
			CLQ2_ParseStartSoundPacket();
			break;
		case svcq2_print:		//10			// [qbyte] id [string] null terminated string
			i = Imsgfuncs->ReadByte ();
			s = Imsgfuncs->ReadString ();

			//TODO(fhomolka): CL_ParsePrint(s, i);
			break;
		case svcq2_stufftext:	//11			// [string] stuffed into client's console buffer, should be \n terminated
			s = Imsgfuncs->ReadString ();
			Con_DPrintf ("stufftext: %s\n", s);
			if (!strncmp(s, "precache", 8))	//big major hack. Q2 uses a command that q1 has as a cvar.
			{	//call the q2 precache function.
				//TODO(fhomolka): CLQ2_Precache_f();
			}
			else
				Cbuf_AddText (s, RESTRICT_SERVER);	//don't let the local user cheat
			break;
		case svcq2_serverdata:	//12			// [long] protocol ...
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CLQ2_ParseServerData ();
			break;
		case svcq2_configstring:	//13		// [short] [string]
			//TODO(fhomolka): CLQ2_ParseConfigString();
			break;
		case svcq2_spawnbaseline://14
			CLQ2_ParseBaseline();
			break;
		case svcq2_centerprint:	//15		// [string] to put in center of the screen
			s = MSG_ReadString();

#ifdef PLUGINS
			if (Plug_CenterPrintMessage(s, seat))
#endif
				SCR_CenterPrint (seat, s, false);
			break;
		case svcq2_download:		//16		// [short] size [size bytes]
			//TODO CL_ParseDownload(false);
			break;
		case svcq2_playerinfo:	//17			// variable
			plugfuncs->EndGame ("CL_ParseServerMessage: svcq2_playerinfo not as part of svcq2_frame");
			return;
		case svcq2_packetentities://18			// [...]
			plugfuncs->EndGame ("CL_ParseServerMessage: svcq2_packetentities not as part of svcq2_frame");
			return;
		case svcq2_deltapacketentities://19	// [...]
			plugfuncs->EndGame ("CL_ParseServerMessage: svcq2_deltapacketentities not as part of svcq2_frame");
			return;
		case svcq2_frame:			//20 (the bastard to implement.)
			CLQ2_ParseFrame(0);
			break;

		case svcr1q2_zpacket:	//r1q2, just try to ignore it.
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2 || cls.protocol_q2 == PROTOCOL_VERSION_Q2PRO)
				CLQ2_ParseZPacket();
			else
				goto isilegible;
			break;
		case svcr1q2_zdownload:
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2 || cls.protocol_q2 == PROTOCOL_VERSION_Q2PRO)
				{
					//TODO CL_ParseDownload(true);
				}
			else
				goto isilegible;
			break;
		case svcr1q2_playerupdate:
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2)
				CLR1Q2_ParsePlayerUpdate();
			else
				goto isilegible;
			break;
		case svcr1q2_setting:
			if (cls.protocol_q2 == PROTOCOL_VERSION_R1Q2)
				CLR1Q2_ParseSetting();
			else
				goto isilegible;
			break;
		}
	}
	CL_SetSolidEntities ();

//TODO(fhomolka):
/*
	if (cls.demohadkeyframe)
		CL_WriteDemoMessage(&net_message, startpos);	//FIXME: incomplete frames might be awkward
*/
}
#endif 
