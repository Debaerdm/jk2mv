// sv_ccmds_misc.h -- Miscellaneous console commands
// Extracted from sv_ccmds.cpp as part of aggressive server refactoring

#ifndef SV_CCMDS_MISC_H
#define SV_CCMDS_MISC_H

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {
	char	*p;
	char	text[1024];

	if( !com_dedicated->integer ) {
		Com_Printf( "Server is not dedicated.\n" );
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	strcpy (text, "Server: ");
	p = Cmd_Args();

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(NULL, "chat \"%s\n\"", text);
}

static const char * const forceToggleNamePrints[] =
{
	"HEAL",//FP_HEAL
	"JUMP",//FP_LEVITATION
	"SPEED",//FP_SPEED
	"PUSH",//FP_PUSH
	"PULL",//FP_PULL
	"MINDTRICK",//FP_TELEPTAHY
	"GRIP",//FP_GRIP
	"LIGHTNING",//FP_LIGHTNING
	"DARK RAGE",//FP_RAGE
	"PROTECT",//FP_PROTECT
	"ABSORB",//FP_ABSORB
	"TEAM HEAL",//FP_TEAM_HEAL
	"TEAM REPLENISH",//FP_TEAM_FORCE
	"DRAIN",//FP_DRAIN
	"SEEING",//FP_SEE
	"SABER OFFENSE",//FP_SABERATTACK
	"SABER DEFENSE",//FP_SABERDEFEND
	"SABER THROW",//FP_SABERTHROW
	NULL
};

/*
==================
SV_ForceToggle_f
==================
*/
void SV_ForceToggle_f(void)
{
	int i = 0;
	int fpDisabled = Cvar_VariableValue("g_forcePowerDisable");
	int targetPower = 0;
	const char *powerDisabled = "Enabled";

	if ( Cmd_Argc () < 2 )
	{ //no argument supplied, spit out a list of force powers and their numbers
		while (i < NUM_FORCE_POWERS)
		{
			if (fpDisabled & (1 << i))
			{
				powerDisabled = "Disabled";
			}
			else
			{
				powerDisabled = "Enabled";
			}

			Com_Printf("%i - %s - Status: %s\n", i, forceToggleNamePrints[i], powerDisabled);
			i++;
		}

		Com_Printf("Example usage: forcetoggle 3\n(toggles PUSH)\n");
		return;
	}

	targetPower = atoi(Cmd_Argv(1));

	if (targetPower < 0 || targetPower >= NUM_FORCE_POWERS)
	{
		Com_Printf("Specified a power that does not exist.\nExample usage: forcetoggle 3\n(toggles PUSH)\n");
		return;
	}

	if (fpDisabled & (1 << targetPower))
	{
		powerDisabled = "enabled";
		fpDisabled &= ~(1 << targetPower);
	}
	else
	{
		powerDisabled = "disabled";
		fpDisabled |= (1 << targetPower);
	}

	Cvar_Set("g_forcePowerDisable", va("%i", fpDisabled));

	Com_Printf("%s has been %s.\n", forceToggleNamePrints[targetPower], powerDisabled);
}

/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}

/*
=================
SV_KillServer_f
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}

/*
=================
SV_WhitelistIP_f
=================
*/
static void SV_WhitelistIP_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf ("Usage: whitelistip <ip>...\n");
		return;
	}

	for ( int i = 1; i < Cmd_Argc(); i++ ) {
		netadr_t	adr;

		if ( NET_StringToAdr( Cmd_Argv(i), &adr ) ) {
			SVC_WhitelistAdr( adr );
		} else {
			Com_Printf("Incorrect IP address: %s\n", Cmd_Argv(i));
		}
	}
}

#endif // SV_CCMDS_MISC_H
