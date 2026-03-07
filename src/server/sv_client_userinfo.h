// sv_client_userinfo.h -- Userinfo validation with crash/exploit fixes
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_USERINFO_H
#define SV_CLIENT_USERINFO_H

#define INFO_CHANGE_MIN_INTERVAL	5000
#define INFO_CHANGE_MAX_COUNT		4

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C friendly form.
=================
*/
void SV_UserinfoChanged( client_t *cl ) {
	const char	*ip;
	char		*val;
	int			i;

	// name for C code
	Q_strncpyz( cl->name, Info_ValueForKey (cl->userinfo, "name"), sizeof(cl->name) );

	// rate command

	// if the client is on the same subnet as the server and we aren't running an
	// internet public server, assume they don't need a rate choke
	cl->rate = atoi( Info_ValueForKey(cl->userinfo, "rate") );
	if ( Sys_IsLANAddress( cl->netchan.remoteAddress ) && com_dedicated->integer != 2 && cl->rate < 99999 ) {
		cl->rate = 99999;	// lans should not rate limit
	}

	val = Info_ValueForKey (cl->userinfo, "handicap");
	if (strlen(val)) {
		i = atoi(val);
		if (i<=0 || i>100 || strlen(val) > 4) {
			Info_SetValueForKey( cl->userinfo, "handicap", "100" );
		}
	}

	SV_ClientUpdateSnaps( cl );

	if (mv_fixnamecrash->integer && !(sv.fixes & MVFIX_NAMECRASH)) {
		char name[61], cleanedName[61]; // 60 because some mods increased this
		Q_strncpyz(name, Info_ValueForKey(cl->userinfo, "name"), sizeof(name));
		int count = 0;

		for (int i = 0; i < (int)strlen(name); i++) {
			char ch = name[i];

			if (isascii(ch) ||
				ch == '\x0A' || // underscore cursor (console only)
				ch == '\x0B' || // block cursor (console only)
				ch == '\xB7' || // section sign (§)
				ch == '\xB4' || // accute accent (´)
				ch == '\xC4' || // A umlaut (Ä)
				ch == '\xD6' || // O umlaut (Ö)
				ch == '\xDC' || // U umlaut (Ü)
				ch == '\xDF' || // sharp S (ß)
				ch == '\xE4' || // a umlaut (ä)
				ch == '\xF6' || // o umlaut (ö)
				ch == '\xFC')   // u umlaut (ü)
			{
				cleanedName[count++] = ch;
			}
		}

		cleanedName[count] = 0;
		Info_SetValueForKey(cl->userinfo, "name", cleanedName);
	}

	// forcecrash fix
	if (mv_fixforcecrash->integer && !(sv.fixes & MVFIX_FORCECRASH)) {
		char forcePowers[30];
		Q_strncpyz(forcePowers, Info_ValueForKey(cl->userinfo, "forcepowers"), sizeof(forcePowers));

		int len = (int)strlen(forcePowers);
		bool badForce = false;
		if (len >= 22 && len <= 24) {
			byte seps = 0;

			for (int i = 0; i < len; i++) {
				if (forcePowers[i] != '-' && (forcePowers[i] < '0' || forcePowers[i] > '9')) {
					badForce = true;
					break;
				}

				if (forcePowers[i] == '-' && (i < 1 || i > 5)) {
					badForce = true;
					break;
				}

				if (i && forcePowers[i - 1] == '-' && forcePowers[i] == '-') {
					badForce = true;
					break;
				}

				if (forcePowers[i] == '-') {
					seps++;
				}
			}

			if (seps != 2) {
				badForce = true;
			}
		} else {
			badForce = true;
		}

		if (badForce) {
			Q_strncpyz(forcePowers, "7-1-030000000000003332", sizeof(forcePowers));
		}

		if ( !Info_SetValueForKey(cl->userinfo, "forcepowers", forcePowers) )
		{
			{
				SV_DropClient( cl, "userinfo string length exceeded");
				return;
			}
		}
	}

	// serverside galaking fix
	if (mv_fixgalaking->integer && !(sv.fixes & MVFIX_GALAKING)) {
		char model[80];

		Q_strncpyz(model, Info_ValueForKey(cl->userinfo, "model"), sizeof(model));
		if (!Q_stricmp(model, "galak_mech") || !Q_stricmpn(model, "galak_mech/", 11))
		{
			if ( !Info_SetValueForKey(cl->userinfo, "model", "galak/default") )
			{
				SV_DropClient( cl, "userinfo string length exceeded");
				return;
			}
		}

		Q_strncpyz(model, Info_ValueForKey(cl->userinfo, "team_model"), sizeof(model));
		if (!Q_stricmp(model, "galak_mech") || !Q_stricmpn(model, "galak_mech/", 11))
		{
			if ( !Info_SetValueForKey(cl->userinfo, "team_model", "galak/default") )
			{
				SV_DropClient( cl, "userinfo string length exceeded");
				return;
			}
		}
	}

	// serverside broken models fix (head only model)
	if (mv_fixbrokenmodels->integer && !(sv.fixes & MVFIX_BROKENMODEL)) {
		char model[80];

		Q_strncpyz(model, Info_ValueForKey(cl->userinfo, "model"), sizeof(model));
		if (!Q_stricmpn(model, "kyle/fpls", 9) || !Q_stricmp(model, "morgan") || (!Q_stricmpn(model, "morgan/", 7) && (Q_stricmp(model, "morgan/default_mp") && Q_stricmp(model, "morgan/red") && Q_stricmp(model, "morgan/blue"))))
		{
			if ( !Info_SetValueForKey(cl->userinfo, "model", "kyle/default") )
			{
				SV_DropClient( cl, "userinfo string length exceeded");
				return;
			}
		}
	}
	
	// TTimo
	// maintain the IP information
	// the banning code relies on this being consistently present
	if( NET_IsLocalAddress(cl->netchan.remoteAddress) )
		ip = "localhost";
	else
		ip = NET_AdrToString( cl->netchan.remoteAddress );

	if ( !Info_SetValueForKey(cl->userinfo, "ip", ip) )
		SV_DropClient( cl, "userinfo string length exceeded" );
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f( client_t *cl ) {
	char info[MAX_INFO_STRING];
	char *arg = Cmd_Argv(1);

	// Stop random empty /userinfo calls without hurting anything
	if (!arg || !*arg) {
		return;
	}

	if (cl->lastUserInfoChange > svs.time) {
		cl->lastUserInfoCount++;

		if (cl->lastUserInfoCount >= INFO_CHANGE_MAX_COUNT) {
			Q_strncpyz( cl->userinfoPostponed, arg, sizeof(cl->userinfoPostponed) );
			SV_SendServerCommand(cl, "print \"Warning: Too many info changes, last info postponed\n\"\n");
			return;
		}
	} else {
		cl->userinfoPostponed[0] = 0;
		cl->lastUserInfoCount = 0;
		cl->lastUserInfoChange = svs.time + INFO_CHANGE_MIN_INTERVAL;
	}

	Q_strncpyz( cl->userinfo, arg, sizeof(cl->userinfo) );
	SV_UserinfoChanged( cl );

	// call prog code to allow overrides
	VM_Call( gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients );

	// get the name out of the game and set it in the engine
	SV_GetConfigstring(CS_PLAYERS + (cl - svs.clients), info, sizeof(info));
	Info_SetValueForKey(cl->userinfo, "name", Info_ValueForKey(info, "n"));
	Q_strncpyz(cl->name, Info_ValueForKey(info, "n"), sizeof(cl->name));
}

#endif // SV_CLIENT_USERINFO_H
