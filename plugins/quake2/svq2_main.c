#include "q2common.h"

void SVQ2_ClientProtocolExtensionsChanged(client_t *client)
{
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	client->frameunion.q2frames = client->frameunion.q2frames;	//don't touch these.
	if (client->frameunion.q2frames)
		Z_Free(client->frameunion.q2frames);

	client->frameunion.q2frames = Z_Malloc(sizeof(q2client_frame_t)*Q2UPDATE_BACKUP);
}
