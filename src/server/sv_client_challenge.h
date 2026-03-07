// sv_client_challenge.h -- Challenge system for connection authentication
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_CHALLENGE_H
#define SV_CLIENT_CHALLENGE_H

/*
=================
SV_GetChallenge

A "getchallenge" OOB command has been received
Returns a challenge number that can be used
in a subsequent connectResponse command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
void SV_GetChallenge( netadr_t from ) {
	int		i;
	int		oldest;
	int		oldestTime;
	int		oldestClientTime;
	int		clientChallenge;
	challenge_t	*challenge;
	qboolean wasfound = qfalse;

	oldest = 0;
	oldestClientTime = oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	challenge = &svs.challenges[0];
	clientChallenge = atoi(Cmd_Argv(1));

	for (i = 0; i < MAX_CHALLENGES; i++, challenge++) {
		if (!challenge->connected && NET_CompareAdr(from, challenge->adr)) {
			wasfound = qtrue;

			if (challenge->time < oldestClientTime)
				oldestClientTime = challenge->time;
		}

		if (wasfound && i >= MAX_CHALLENGES_MULTI) {
			i = MAX_CHALLENGES;
			break;
		}

		if ( challenge->time < oldestTime ) {
			oldestTime = challenge->time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// this is the first time this client has asked for a challenge
		challenge = &svs.challenges[oldest];
		challenge->clientChallenge = clientChallenge;
		challenge->adr = from;
		challenge->connected = qfalse;
	}

	// always generate a new challenge number, so the client cannot circumvent sv_maxping
	challenge->challenge = ((rand() << 16) ^ rand()) ^ svs.time;
	challenge->wasrefused = qfalse;
	challenge->time = svs.time;
	challenge->pingTime = svs.time;
	NET_OutOfBandPrint(NS_SERVER, challenge->adr, "challengeResponse %i %i", challenge->challenge, clientChallenge);
}

#endif // SV_CLIENT_CHALLENGE_H
