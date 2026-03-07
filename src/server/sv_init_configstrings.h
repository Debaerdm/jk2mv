// sv_init_configstrings.h -- Configstring management
// Extracted from sv_init.cpp as part of aggressive server refactoring

#ifndef SV_INIT_CONFIGSTRINGS_H
#define SV_INIT_CONFIGSTRINGS_H

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	int		len, i;
	int		maxChunkSize = MAX_STRING_CHARS - 24;
	client_t	*client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( (void *)sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {

		// send the data to all relevent clients
		for (i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++) {
			if ( client->state < CS_PRIMED ) {
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && client->gentity && (client->gentity->r.svFlags & SVF_NOSERVERINFO) ) {
				continue;
			}

			len = (int)strlen( val );
			if( len >= maxChunkSize ) {
				int		sent = 0;
				int		remaining = len;
				char	*cmd;
				char	buf[MAX_STRING_CHARS];

				while (remaining > 0 ) {
					if ( sent == 0 ) {
						cmd = "bcs0";
					}
					else if( remaining < maxChunkSize ) {
						cmd = "bcs2";
					}
					else {
						cmd = "bcs1";
					}
					Q_strncpyz( buf, &val[sent], maxChunkSize );

					SV_SendServerCommand( client, "%s %i \"%s\"\n", cmd, index, buf );

					sent += (maxChunkSize - 1);
					remaining -= (maxChunkSize - 1);
				}
			} else {
				// standard cs, just send it
				SV_SendServerCommand( client, "cs %i \"%s\"\n", index, val );
			}
		}
	}
}

/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = 0;
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}

/*
================
SV_AddConfigstring

================
*/
int SV_AddConfigstring (const char *name, int start, int max)
{
	int		i;

	if (!name || !name[0])
	{
		return 0;
	}

	if (name[0] == '/' || name[0] == '\\')
	{
#ifdef DEBUG
		Com_DPrintf( "WARNING: Leading slash on '%s'\n", name);
#endif
		name++;

		if (!name[0])
		{
			return 0;
		}
	}

	for (i=1 ; i < max ; i++)
	{
		if (sv.configstrings[start+i][0] == 0)
		{	// Didn't find it
			SV_SetConfigstring ((start+i), name);
			break;
		}
		else if (!Q_stricmp(sv.configstrings[start+i], name))
		{
			return i;
		}
	}

	return 0;
}

#endif // SV_INIT_CONFIGSTRINGS_H
