/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// server.h

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../game/g_public.h"
#include "../game/bg_public.h"
#include "qvmStructs.h"

//=============================================================================

#define	PERS_SCORE				0		// !!! MUST NOT CHANGE, SERVER AND
// GAME BOTH REFERENCE !!!

#define	MAX_ENT_CLUSTERS	16


#define	MAX_1V1_ARENAS  12

typedef struct {
    vec3_t spawn1;
    vec3_t spawn2;
    int player1num;
    int player2num;
    int population;
    qboolean finished;
} arena;


typedef struct svEntity_s {
    struct worldSector_s *worldSector;
    struct svEntity_s *nextEntityInWorldSector;

    entityState_t baseline; // for delta compression of initial sighting
    int numClusters; // if -1, use headnode instead
    int clusternums[MAX_ENT_CLUSTERS];
    int lastCluster; // if all the clusters don't fit in clusternums
    int areanum, areanum2;
    int snapshotCounter; // used to prevent double adding from portal views
} svEntity_t;

typedef enum {
    SS_DEAD, // no map loaded
    SS_LOADING, // spawning level entities
    SS_GAME // actively running
} serverState_t;

typedef struct {
    serverState_t state;
    qboolean restarting; // if true, send configstring changes during SS_LOADING
    int serverId; // changes each server start
    int restartedServerId; // serverId before a map_restart
    int checksumFeed; // the feed key that we use to compute the pure checksum strings
    // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
    // the serverId associated with the current checksumFeed (always <= serverId)
    int checksumFeedServerId;
    int snapshotCounter; // incremented for each snapshot built
    int timeResidual; // <= 1000 / sv_frame->value
    int nextFrameTime; // when time > nextFrameTime, process world
    struct cmodel_s *models[MAX_MODELS];
    char *configstrings[MAX_CONFIGSTRINGS];
    svEntity_t svEntities[MAX_GENTITIES];

    char *entityParsePoint; // used during game VM init

    // the game virtual machine will update these on init and changes
    sharedEntity_t *gentities;
    int gentitySize;
    int num_entities; // current number, <= MAX_GENTITIES

    playerState_t *gameClients;
    int gameClientSize; // will be > sizeof(playerState_t) due to game private data

    int restartTime;
    int time;

    // For the anti ghost
    unsigned int stopAntiBlockTime;
    qboolean checkGhostMode;

    // For the turnpike blocker
    int currplayers;

    // For custom entities
    int lastEntityNum;
    int lastIndexNum;

    // for the arenas
    arena Arenas[MAX_1V1_ARENAS];
    vec3_t defaultSpawns[20];
    int doTeleportTime;
    int doSmite;
    qboolean doneTp;
    qboolean doneSmite;
    int finishedArenas;
} server_t;


typedef struct {
    int areabytes;
    byte areabits[MAX_MAP_AREA_BYTES]; // portalarea visibility bits
    playerState_t ps;
    int num_entities;
    int first_entity; // into the circular sv_packet_entities[]
    // the entities MUST be in increasing state number
    // order, otherwise the delta compression will fail
    int messageSent; // time the message was transmitted
    int messageAcked; // time the message was acked
    int messageSize; // used to rate drop packets
} clientSnapshot_t;

typedef enum {
    CS_FREE, // can be reused for a new connection
    CS_ZOMBIE, // client has been disconnected, but don't reuse
    // connection for a couple seconds
    CS_CONNECTED, // has been assigned to a client_t, but no gamestate yet
    CS_PRIMED, // gamestate has been sent, but client hasn't sent a usercmd
    CS_ACTIVE // client is fully in game
} clientState_t;

typedef struct netchan_buffer_s {
    msg_t msg;
    byte msgBuffer[MAX_MSGLEN];
    struct netchan_buffer_s *next;
} netchan_buffer_t;


typedef struct clientMod_s {
    int powerups[MAX_POWERUPS]; // For infinite ammo
    int lastEventSequence; // For infinite ammo

    int delayedSound; // Snapshot where sound event should be sended

    int locationLocked; // If 1 the location command will be locked

    int perPlayerHealth; // This player have custom config for health
    int limitHealth; // Up to what health
    int whenmovingHealth; // Health when a player is moving?
    int timeoutHealth; // Health every X milliseconds
    int stepHealth; // How many health will be added
    int lastAutoHealth; // Last time autohealth this player
    int turnOffWhenFinish; // Turn off the autohealth if limit < health and turnOffUsed is 1
    int turnOffUsed;

    int frozen; // Whether the player is frozen

    vec3_t savedPosition; // Saved client last position coordinates
    vec3_t savedPositionAngle; // Saved client last position angle

    qboolean ghost; // Whether the player has cg_ghost 1 (Jump Mode)
    qboolean ready; // Whether the player has activated his timer with /ready (Jump Mode)
    qboolean noFreeSave; // Whether the player wants to disable the free-saving feature (Jump Mode)

    int infiniteStamina; // Enable infinite stamina on a player
    int infiniteWallJumps; // Enable infinite walljumps on a player
    int hidePlayers; // Make all other players invisible for a client

    int lastWeaponAfterScope; // For disable scope is necesary save last weapon used for do switch.

    char authcl[32]; // Change for custom auths.
} clientMod_t;

typedef struct client_s {
    clientState_t state;
    char userinfo[MAX_INFO_STRING]; // name, etc
    char userinfobuffer[MAX_INFO_STRING]; //used for buffering of user info

    char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
    int reliableSequence; // last added reliable message, not necesarily sent or acknowledged yet
    int reliableAcknowledge; // last acknowledged reliable message
    int reliableSent; // last sent reliable message, not necesarily acknowledged yet
    int messageAcknowledge;

    int gamestateMessageNum; // netchan->outgoingSequence of gamestate
    int challenge;

    usercmd_t lastUsercmd;
    int lastMessageNum; // for delta compression
    int lastClientCommand; // reliable client message sequence
    char lastClientCommandString[MAX_STRING_CHARS];
    sharedEntity_t *gentity; // SV_GentityNum(clientnum)
    char name[MAX_NAME_LENGTH]; // extracted from userinfo, high bits masked

    // downloading
    char downloadName[MAX_QPATH]; // if not empty string, we are downloading
    fileHandle_t download; // file being downloaded
    int downloadSize; // total bytes (can't use EOF because of paks)
    int downloadCount; // bytes sent
    int downloadClientBlock; // last block we sent to the client, awaiting ack
    int downloadCurrentBlock; // current block number
    int downloadXmitBlock; // last block we xmited
    unsigned char *downloadBlocks[MAX_DOWNLOAD_WINDOW]; // the buffers for the download blocks
    int downloadBlockSize[MAX_DOWNLOAD_WINDOW];
    qboolean downloadEOF; // We have sent the EOF block
    int downloadSendTime; // time we last got an ack from the client

    int deltaMessage; // frame last client usercmd message
    int nextReliableTime; // svs.time when another reliable command will be allowed
    int nextReliableUserTime; // svs.time when another userinfo change will be allowed
    int lastPacketTime; // svs.time when packet was last received
    int lastConnectTime; // svs.time when connection started
    int nextSnapshotTime; // send another snapshot when svs.time >= nextSnapshotTime
    qboolean rateDelayed; // true if nextSnapshotTime was set based on rate instead of snapshotMsec
    int timeoutCount; // must timeout a few frames in a row so debugging doesn't break
    clientSnapshot_t frames[PACKET_BACKUP]; // updates can be delta'd from here
    int ping;
    int rate; // bytes / second
    int snapshotMsec; // requests a snapshot every snapshotMsec unless rate choked
    int pureAuthentic;
    qboolean gotCP; // TTimo - additional flag to distinguish between a bad pure checksum, and no cp command at all
    netchan_t netchan;
    // TTimo
    // queuing outgoing fragmented messages to send them properly, without udp packet bursts
    // in case large fragmented messages are stacking up
    // buffer them into this queue, and hand them out to netchan as needed
    netchan_buffer_t *netchan_start_queue;
    netchan_buffer_t **netchan_end_queue;

    qboolean demo_recording; // are we currently recording this client?
    fileHandle_t demo_file; // the file we are writing the demo to
    qboolean demo_waiting; // are we still waiting for the first non-delta frame?
    int demo_backoff; // how many packets (-1 actually) between non-delta frames?
    int demo_deltas; // how many delta frames did we let through so far?

    int oldServerTime;
    qboolean csUpdated[MAX_CONFIGSTRINGS + 1];
    int numcmds; // number of client commands so far (in this time period), for sv_floodprotect

    char colourName[MAX_NAME_LENGTH];

    // Medkit vars for gunsmod
    qboolean hasmedkit;
    int lastmedkittime;

    // Stuff used for custom chat
    qboolean muted;
    qboolean isuser;
    qboolean isadmin;
    qboolean isowner;
    qboolean isauthed;
    qboolean isbot;
    int chatcolour;

    // for anticamp
    float xlast;
    float ylast;
    float zlast;
    int timechecked;
    int campcounter;

    // I'll clean this up eventually lol
    int lastmedtime;
    int lastsoundtime;
    qboolean particlefx;

    // Attaching to other players
    qboolean hasattached;
    qboolean isattached;
    int attachedto;
    int stopattach;

    // for the levelsystem (parts of this may also be used by the 1v1 server)
    int experience;
    int level;
    int kills;
    int clientgamenum;
    qboolean customname;
    char lastcustomname[128];
    char defaultconfigstr[256];

    // For the spectator feature
    int cidCurrSpecd;
    int cidLastSpecd;
    char spectators[256];
    int cid;
    int lastLocationTime;
    char *location;
    char nameConfigString[256];
    char lastName[MAX_NAME_LENGTH];

    // For battleroyale
    char *lastPistol;
    char *lastSecondary;
    char *lastRifle;
    char *lastSniper;

    // 1v1 arena
    int arena;
    int skill;
    int playernum;
    int primary;
    int secondary;
    int pistol;
    int sniper;
    int preference;
    qboolean didplay;
    qboolean weaponGiven;
    qboolean didWin;
    qboolean didLose;
    int arenaKills;
    int arenaDeaths;

    // Variables of TitanMod
    clientMod_t cm;
} client_t;

//=============================================================================


// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

#define	AUTHORIZE_TIMEOUT	5000

typedef struct {
    netadr_t adr;
    int challenge;
    int challengePing;
    int time; // time the last packet was sent to the autherize server
    int pingTime; // time the challenge response was sent to client
    int firstTime; // time the adr was first used, for authorize timeout checks
    qboolean wasrefused;
    qboolean connected;
} challenge_t;

typedef struct {
    netadr_t adr;
    int time;
} receipt_t;

// MAX_INFO_RECEIPTS is the maximum number of getstatus+getinfo responses that we send
// in a two second time period.
#define MAX_INFO_RECEIPTS	48

#define	MAX_MASTERS	8				// max recipients for heartbeat packets


// this structure will be cleared only when the game dll changes
typedef struct {
    qboolean initialized; // sv_init has completed

    int time; // will be strictly increasing across level changes

    int snapFlagServerBit; // ^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()

    client_t *clients; // [sv_maxclients->integer];
    int numSnapshotEntities; // sv_maxclients->integer*PACKET_BACKUP*MAX_PACKET_ENTITIES
    int nextSnapshotEntities; // next snapshotEntities to use
    entityState_t *snapshotEntities; // [numSnapshotEntities]
    int nextHeartbeatTime;
    challenge_t challenges[MAX_CHALLENGES]; // to prevent invalid IPs from connecting
    receipt_t infoReceipts[MAX_INFO_RECEIPTS];
    netadr_t redirectAddress; // for rcon return messages

    netadr_t authorizeAddress; // for rcon return messages
} serverStatic_t;


// The value below is how many extra characters we reserve for every instance of '$' in a
// ut_radio, say, or similar client command.  Some jump maps have very long $location's.
// On these maps, it may be possible to crash the server if a carefully-crafted
// client command is sent.  The constant below may require further tweaking.  For example,
// a text of "$location" would have a total computed length of 25, because "$location" has
// 9 characters, and we increment that by 16 for the '$'.
#define STRLEN_INCREMENT_PER_DOLLAR_VAR 16

// Don't allow more than this many dollared-strings (e.g. $location) in a client command
// such as ut_radio and say.  Keep this value low for safety, in case some things like
// $location expand to very large strings in some maps.  There is really no reason to have
// more than 6 dollar vars (such as $weapon or $location) in things you tell other people.
#define MAX_DOLLAR_VARS 6

// When a radio text (as in "ut_radio 1 1 text") is sent, weird things start to happen
// when the text gets to be greater than 118 in length.  When the text is really large the
// server will crash.  There is an in-between gray zone above 118, but I don't really want
// to go there.  This is the maximum length of radio text that can be sent, taking into
// account increments due to presence of '$'.
#define MAX_RADIO_STRLEN 118

// Don't allow more than this text length in a command such as say.  I pulled this
// value out of my ass because I don't really know exactly when problems start to happen.
// This value takes into account increments due to the presence of '$'.
#define MAX_SAY_STRLEN 256

//=============================================================================

extern serverStatic_t svs; // persistant server info across maps
extern server_t sv; // cleared each map
extern vm_t *gvm; // game virtual machine

#define	MAX_MASTER_SERVERS	5

extern cvar_t *sv_fps;
extern cvar_t *sv_timeout;
extern cvar_t *sv_zombietime;
extern cvar_t *sv_rconPassword;
extern cvar_t *sv_rconRecoveryPassword;
extern cvar_t *sv_rconAllowedSpamIP;
extern cvar_t *sv_privatePassword;
extern cvar_t *sv_allowDownload;
extern cvar_t *sv_maxclients;

extern cvar_t *sv_privateClients;
extern cvar_t *sv_hostname;
extern cvar_t *sv_master[MAX_MASTER_SERVERS];
extern cvar_t *sv_reconnectlimit;
extern cvar_t *sv_showloss;
extern cvar_t *sv_padPackets;
extern cvar_t *sv_killserver;
extern cvar_t *sv_mapname;
extern cvar_t *sv_mapChecksum;
extern cvar_t *sv_serverid;
extern cvar_t *sv_minRate;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_minPing;
extern cvar_t *sv_maxPing;
extern cvar_t *sv_gametype;
extern cvar_t *sv_pure;
extern cvar_t *sv_newpurelist;
extern cvar_t *sv_floodProtect;
extern cvar_t *sv_lanForceRate;
extern cvar_t *sv_strictAuth;
extern cvar_t *sv_clientsPerIp;

extern cvar_t *sv_demonotice;
extern cvar_t *sv_sayprefix;
extern cvar_t *sv_tellprefix;
extern cvar_t *sv_demofolder;

extern cvar_t *mod_infiniteStamina;
extern cvar_t *mod_infiniteWallJumps;
extern cvar_t *mod_nofallDamage;

extern cvar_t *mod_colourNames;

extern cvar_t *mod_playerCount;
extern cvar_t *mod_botsCount;
extern cvar_t *mod_mapName;
extern cvar_t *mod_mapColour;
extern cvar_t *mod_hideCmds;
extern cvar_t *mod_infiniteAmmo;
extern cvar_t *mod_forceGear;
extern cvar_t *mod_checkClientGuid;
extern cvar_t *mod_disconnectMsg;
extern cvar_t *mod_badRconMessage;

extern cvar_t *mod_allowTell;
extern cvar_t *mod_allowRadio;
extern cvar_t *mod_allowWeapDrop;
extern cvar_t *mod_allowItemDrop;
extern cvar_t *mod_allowFlagDrop;
extern cvar_t *mod_allowSuicide;
extern cvar_t *mod_allowVote;
extern cvar_t *mod_allowTeamSelection;
extern cvar_t *mod_allowWeapLink;

extern cvar_t *mod_minKillHealth;
extern cvar_t *mod_minTeamChangeHealth;

extern cvar_t *mod_limitHealth;
extern cvar_t *mod_timeoutHealth;
extern cvar_t *mod_enableHealth;
extern cvar_t *mod_addAmountOfHealth;
extern cvar_t *mod_whenMoveHealth;

extern cvar_t *mod_allowPosSaving;
extern cvar_t *mod_persistentPositions;
extern cvar_t *mod_freeSaving;
extern cvar_t *mod_enableJumpCmds;
extern cvar_t *mod_enableHelpCmd;
extern cvar_t *mod_loadSpeedCmd;
extern cvar_t *mod_ghostRadius;

extern cvar_t *mod_slickSurfaces;
extern cvar_t *mod_gameType;
extern cvar_t *mod_ghostPlayers;
extern cvar_t *mod_noWeaponRecoil;
extern cvar_t *mod_noWeaponCycle;
extern cvar_t *mod_specChatGlobal;
extern cvar_t *mod_cleanMapPrefixes;

extern cvar_t *mod_disableScope;
extern cvar_t *mod_fastTeamChange;

extern cvar_t *mod_auth;
extern cvar_t *mod_defaultauth;

extern cvar_t *mod_hideServer;
extern cvar_t *mod_enableWeaponsCvars;

extern cvar_t *mod_gunsmod;
extern cvar_t *mod_customchat;
extern cvar_t *mod_infiniteAirjumps;
extern cvar_t *mod_punishCampers;

extern cvar_t *mod_levelsystem;
extern cvar_t *sv_TurnpikeBlocker;
extern cvar_t *sv_ghostOnRoundstart;

extern cvar_t *mod_announceNoscopes;

extern cvar_t *sv_ent_dump;
extern cvar_t *sv_ent_dump_path;
extern cvar_t *sv_ent_load;
extern cvar_t *sv_ent_load_path;

extern cvar_t *mod_jumpSpecAnnouncer;

extern cvar_t *mod_turnpikeTeleporter;

extern cvar_t *mod_battleroyale;

extern cvar_t *mod_1v1arena;

extern cvar_t *mod_fastSr8;

extern cvar_t *mod_zombiemod;

#ifdef USE_AUTH
extern cvar_t *sv_authServerIP;
extern cvar_t *sv_auth_engine;
#endif

//===========================================================


//
// qvm_offsets.c
//
void *QVM_baseWeapon(weapon_t weapon);

void *QVM_bullets(weapon_t weapon);

void *QVM_clips(weapon_t weapon);

void *QVM_damages(weapon_t weapon, ariesHitLocation_t location);

void *QVM_bleed(weapon_t weapon, ariesHitLocation_t location);

void *QVM_knockback(weapon_t weapon);

void *QVM_reloadSpeed(weapon_t weapon);

void *QVM_fireTime(weapon_t weapon, int mode);

void *QVM_noRecoil(weapon_t weapon, int mode);

void *QVM_WPflags(weapon_t weapon);


//
// sv_weapon.c
//
weapon_t SV_Char2Weapon(char *weapon);

utItemID_t SV_Char2Item(char *item);

int overrideQVMData(void);

void SV_GiveBulletsAW(playerState_t *ps, int bulletsCount, int weapon);

void SV_GiveClipsAW(playerState_t *ps, int clipsCount, int weapon);

void SV_SetBulletsAW(playerState_t *ps, int bulletsCount, int weapon);

void SV_SetClipsAW(playerState_t *ps, int clipsCount, int weapon);

void SV_GiveWeaponCB(playerState_t *ps, weapon_t wp, int bullets, int clips);

void SV_GiveWeapon(playerState_t *ps, weapon_t wp);

void SV_RemoveWeapon(playerState_t *ps, weapon_t wp);

void SV_WeaponMod(int cnum);

void utPSRemoveItem(playerState_t *ps, utItemID_t itemid);

void utPSGiveItem(playerState_t *ps, utItemID_t itemid);

int utPSFirstMath(playerState_t *ps, utItemID_t itemid);

int SV_FirstMatchFor(playerState_t *ps, weapon_t weapon);


//
// sv_main.c
//

void QDECL SV_LogPrintf(const char *fmt, ...);

int SV_GetClientTeam(int cid);

qboolean SV_IsClientGhost(client_t *cl);

qboolean SV_IsClientInPosition(int cid, float x, float y, float z, float xPlus, float yPlus, float zPlus);

void SV_SetClientPosition(int cid, float x, float y, float z);

void SV_LoadPositionFromFile(client_t *cl, char *mapname);

void SV_SavePositionToFile(client_t *cl, char *mapname);

void SV_FinalMessage(char *message);

void QDECL SV_SendServerCommand(client_t *cl, const char *fmt, ...);

void SV_AddOperatorCommands(void);

void SV_RemoveOperatorCommands(void);

void SV_MasterHeartbeat(void);

void SV_MasterShutdown(void);


//
// sv_init.c
//

void SV_SendCustomConfigString(client_t *client, char *cs, int index);

void SV_SetConfigstring(int index, const char *val);

void SV_GetConfigstring(int index, char *buffer, int bufferSize);

void SV_UpdateConfigstrings(client_t *client);

void SV_SetUserinfo(int index, const char *val);

void SV_GetUserinfo(int index, char *buffer, int bufferSize);

void SV_ChangeMaxClients(void);

void SV_SpawnServer(char *server, qboolean killBots);


//
// sv_client.c
//

void MOD_ChangeLocation(client_t *cl, int changeto, int lock);

void MOD_SendCustomLocation(client_t *cl, char *csstring, int index);

void MOD_ResquestPk3DownloadByClientGameState(client_t *client, char *todownload);

void addToClientsList(client_t *speccedClient, client_t *cl);

void removeFromClientsList(client_t *client);

//void forceLocationUpdate (client_t *speccedClient); // FIXME

// events
void EV_PlayerSpawn(int cnum);

void EV_1v1PlayerSpawn(int cnum);

void EV_ClientUserInfoChanged(int cnum);

void EV_ClientConnect(int cnum);

void EV_ClientDisconnect(int cnum);

void EV_ClientBegin(int cnum);

void EV_ClientKill(int cnum, int target);

int SV_ClientIsMoving(client_t *cl);

void MOD_PlaySoundFile(client_t *cl, char *file);

void MOD_SetExternalEvent(client_t *cl, entity_event_t event, int eventarg);

void MOD_AddHealth(client_t *cl, int value);

void MOD_SetHealth(client_t *cl, int value);

void SV_GetChallenge(netadr_t from);

void SV_DirectConnect(netadr_t from);

void SV_AuthorizeIpPacket(netadr_t from);

void SV_ExecuteClientMessage(client_t *cl, msg_t *msg);

void SV_UserinfoChanged(client_t *cl);

void SV_ClientEnterWorld(client_t *client, usercmd_t *cmd);

void SV_DropClient(client_t *drop, const char *reason);

#ifdef USE_AUTH
void SV_Auth_DropClient(client_t *drop, const char *reason, const char *message);
#endif

void SV_ExecuteClientCommand(client_t *cl, const char *s, qboolean clientOK);

void SV_GhostThink(client_t *cl);

void SV_ClientThink(client_t *cl, usercmd_t *cmd);

void SV_WriteDownloadToClient(client_t *cl, msg_t *msg);

char *SV_CleanName(char *name);

//
// sv_ccmds.c
//
void SV_Heartbeat_f(void);

void SVD_WriteDemoFile(const client_t *, const msg_t *);


//
// sv_snapshot.c
//
void SV_AddServerCommand(client_t *client, const char *cmd);

void SV_UpdateServerCommandsToClient(client_t *client, msg_t *msg);

void SV_WriteFrameToClient(client_t *client, msg_t *msg);

void SV_SendMessageToClient(msg_t *msg, client_t *client);

void SV_SendClientMessages(void);

void SV_SendClientSnapshot(client_t *client);

void SV_CheckClientUserinfoTimer(void);

void SV_UpdateUserinfo_f(client_t *cl);


//
// sv_game.c
//
urtVersion getVersion(void);

int SV_NumForGentity(sharedEntity_t *ent);

sharedEntity_t *SV_GentityNum(int num);

playerState_t *SV_GameClientNum(int num);

svEntity_t *SV_SvEntityForGentity(sharedEntity_t *gEnt);

sharedEntity_t *SV_GEntityForSvEntity(svEntity_t *svEnt);

void SV_InitGameProgs(void);

void SV_ShutdownGameProgs(void);

void SV_RestartGameProgs(void);

qboolean SV_inPVS(const vec3_t p1, const vec3_t p2);


//
// sv_bot.c
//
void SV_BotFrame(int time);

int SV_BotAllocateClient(void);

void SV_BotFreeClient(int clientNum);

void SV_BotInitCvars(void);

int SV_BotLibSetup(void);

int SV_BotLibShutdown(void);

int SV_BotGetSnapshotEntity(int client, int ent);

int SV_BotGetConsoleMessage(int client, char *buf, int size);

int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points);

void BotImport_DebugPolygonDelete(int id);


//
// high level object sorting to reduce interaction tests
//

void SV_ClearWorld(void);

// called after the world model has been loaded, before linking any entities

void SV_UnlinkEntity(sharedEntity_t *ent);

// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void SV_LinkEntity(sharedEntity_t *ent);

// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

clipHandle_t SV_ClipHandleForEntity(const sharedEntity_t *ent);

void SV_SectorList_f(void);

int SV_AreaEntities(const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount);

// fills in a table of entity numbers with entities that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// The world entity is never returned in this list.

int SV_PointContents(const vec3_t p, int passEntityNum);

// returns the CONTENTS_* value from the world and all entities at the given point.

void SV_Trace(trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int passEntityNum,
              int contentmask, int capsule);

// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passEntityNum is explicitly excluded from clipping checks (normally ENTITYNUM_NONE)

void SV_ClipToEntity(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                     int entityNum, int contentmask, int capsule);

// clip to a specific entity


//
// sv_net_chan.c
//
void SV_Netchan_Transmit(client_t *client, msg_t *msg);

void SV_Netchan_TransmitNextFragment(client_t *client);

qboolean SV_Netchan_Process(client_t *client, msg_t *msg);
