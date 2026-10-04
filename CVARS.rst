.. Keep this file in sync with wiki entries

======================
New and Modified Cvars
======================

:Name: com_busyWait
:Values: "0", "1"
:Default: "0"
:Description:
   Enable / Disable old "busy" game loop using 100% CPU time.

..

:Name: com_debugMessage
:Values: "0", "1"
:Default: "0"
:Description:
   Print warnings about field overflows in network messages. Useful
   for debugging modules.

..

:Name: com_speeds
:Values: "0", "1", "3"
:Default: "0"
:Description:
   Print the time spent in each part of every frame, in milliseconds with
   microsecond resolution: all, server (sv), events (ev), client (cl), game
   module (gm), renderer frontend (rf) and backend (bk).
   | 3: Also print the time spent processing each server packet.

..

:Name: com_timestamps
:Values: "0", "1"
:Default: "1"
:Description:
   Print timestamps in qconsole.log and system console.

..

:Name: fs_forcegame
:Values: Foldername
:Default: "" (Not set)
:Description:
   Overrides the active folder, allowing a server/client to store configs and
   other data in a specific folder independent of the active mod (``fs_game``).
   All new configs, screenshots, demos, etc. stored by the game end up in the
   specified folder. This folder may also be "base".

   Load order:

   | ``base``
   | ``fs_basegame cvar``
   | ``fs_game cvar``
   | ``fs_forcegame cvar``

..

:Name: fs_assetspathjka
:Values: Foldername
:Default: "" (Not set on portable); autodetected for non-portable
:Description:
   Sets the path to load JKA assets from.

..

:Name: fs_basejka
:Values: Foldername
:Default: "basejka" (if fs_assetspathjka is empty); "base" (if fs_assetspathjka is set)
:Description:
   Name of the folder containing the JKA assets within fs_assetspathjka. If
   fs_assetspathjka is not set the game tries to load assets from the specified
   folder in fs_basepath and fs_homepath.

..

:Name: fs_loadjka
:Values: "0", "1"
:Default: "1"
:Description:
   Enables loading of JKA assets when fs_assetspathjka point to a valid JKA
   folder.

-----------
Client-Side
-----------

:Name: cl_autoDemo
:Values: "0", "1"
:Default: "0"
:Description:
   When enabled, starts recording a demo automatically on joining a
   server. Current and single last demo are stored
   in ``demos/LastDemo`` directory.

..

:Name: cl_autoDemoFormat
:Values: Format String
:Default: "%t_%m"
:Description:
   Filename format for demos saved with ``saveDemo`` command. Valid
   format tokens are:

   | %d: local date
   | %m: map
   | %n: custom name supplied as an argument to ``saveDemo`` command
   | %p: player name
   | %t: sequence of server timestamps when ``saveDemo`` was executed
   | %%: % character

..

:Name: cl_aviFrameRate
:Values: Integer from 1 to 1000
:Default: "30"
:Description:
   Frame rate for recording with ``video`` command.

..

:Name: cl_aviMotionJpeg
:Values: "0", "1"
:Default: "1"
:Description:
   Record AVI using Motion JPEG video compression format. Much smaller
   file size for little quality loss.

..

:Name: cl_aviMotionJpegQuality
:Values: Integer from 0 to 100
:Default: "90"
:Description:
   JPEG quality used by AVI Motion JPEG compression. Lower values result
   in worse quality and smaller file size.

..

:Name: cl_drawRecording
:Values: "0", "1", "2"
:Default: "1"
:Description:
   | 0: don't draw any demo recording indicator
   | 1: draw filename and demo size near the top of the screen
   | 2: draw red dot in the bottom left corner

..

:Name: cl_demoTimeline
:Values: "0", "1"
:Default: "0"
:Description:
   During demo playback, show the playback speed, the elapsed time, the
   progress through the demo file and the camera mode at the bottom of the
   screen.

..

:Name: cl_freecamSpeed
:Values: Float
:Default: "400"
:Description:
   Speed of the demo free camera (``demo_freecam``), in units per second.

..

:Name: cl_demoHideHud
:Values: "0", "1"
:Default: "0"
:Description:
   Hide the HUD, scoreboard, chat and console notify lines during demo
   playback, for clean shots and videos. Has no effect in a live game.

..

:Name: cl_fovAspectFix
:Values: "0", "1"
:Default: "0"
:Description:
   Widescreen field of view. Mods based on the original code keep the
   horizontal fov on any screen, so 16:9 and 21:9 screens see less at the
   top and bottom than 4:3. With 1, ``cg_fov`` counts as a 4:3 value and
   wide screens see the same height plus more on the sides ("Hor+"), zoom
   scopes included. It does nothing on screens up to 4:3 or when the server
   fixes the fov (dmflags 16).

   | 0: the fov is left to the mod: the original view, unless you turned on
     the mod's own ``cg_fovAspectAdjust``.
   | 1, with a mod that has ``cg_fovAspectAdjust`` (MVSDK mods, including
     jk2mv's own base modules): the mod widens the view as with
     ``cg_fovAspectAdjust 1`` and places the dynamic crosshair, the rocket
     lock box and the saber clash flares for that wider view.
     ``cg_fovAspectAdjust`` and the config keep your own value.
   | 1, with other mods (the original 1.02 to 1.04 modules, also used on pure
     servers without jk2mv's modules): the engine widens the view, whatever
     ``cg_fovAspectAdjust`` holds. What the mod draws over points of the
     world away from the screen center (dynamic crosshair, rocket lock box,
     saber clash flares) then drifts off target; with
     ``cg_dynamicCrosshair 0`` the crosshair stays fixed in the center.

..

:Name: cl_perfOverlay
:Values: "0", "1", "2"
:Default: "0"
:Description:
   | 1: show the frame rate, the 1% low frame rate and the frame time in
     the top right corner
   | 2: also draw a frame time graph (green under 16.7 ms, yellow under
     33.3 ms, red above)

..

:Name: con_height
:Values: Decimal > 0
:Default: "0.5"
:Description:
   Fraction of a screen which should be occupied by in-game console.

..

:Name: con_scale
:Values: Decimal > 0
:Default: "1"
:Description:
   Scale console font relative to it's original size.

..

:Name: con_timestamps
:Values: "0", "1"
:Default: "0"
:Description:
   Draw local timestamps in console and condump output.

..

:Name: in_mouse
:Values: "0", "1", "2", "3"
:Default: "1"
:Description:
   Mouse input mode

   | 0: Mouse disabled.
   | 1: Same as in retail client - high latency, follows desktop mouse speed and acceleration settings.
   | 2: Raw, no acceleration, lowest latency.
   | 3: Raw, lowest latency but with desktop mouse speed and acceleration settings.

..

:Name: mv_allowDownload
:Values: "0", "1"
:Default: "1"
:Description:
   Enable / Disable Downloads. If you turn this off, the download
   popup will not appear and you will not be asked wether a file
   should be downloaded.

..

:Name: mv_apienabled
:Values: "0", "1", "2", "3"
:Default: Max supported MVAPI level
:Description:
   Max MVAPI level modules can use. 0 disables MVAPI SysCalls
   completely.

..

:Name: mv_consoleShiftRequirement
:Values: "0", "1", 2
:Default: "1"
:Description:
   | 0: shift is not required to open/close the console.
   | 1: shift is required to open the console but not to close it.
   | 2: shift is required to both, open and to close the console.

..

:Name: mv_nameShadows
:Values: "0", "1", 2
:Default: "2"
:Description:
   | 0: no name shadows at all.
   | 1: name shadows enabled on every version.
   | 2: name shadows enabled in 1.02 mode.

..

:Name: mv_menuOverride
:Values: "0", "1"
:Default: "0"
:Description:
   Allow loading custom UI modules in the main menu. Beware! This
   gives full control over downloaded content to the mod, there will
   be no download popup. Use only for testing.

..

:Name: mv_slowrefresh
:Values: Integer >= 0
:Default: "3"
:Description:
   Number of requests on a serverlist refresh sent per second to
   servers in the list. Some providers filter packets on a high number
   of requests to a lot of different IP addresses in a short
   time. (e.g. two major ISPs in Germany: "Kabel Deutschland", "Kabel
   BW").

..

:Name: r_consoleFont
:Values: "0", "1", "2"
:Default: "1"
:Description:
   Font used in console, timer, message input field and other places:

   | 0: Original charsgrid_med
   | 1: Code New Roman
   | 2: M+ 1M

..

:Name: r_dynamicGlow
:Values: "0", "1", "2"
:Default: "0"
:Description:
   Enable / Disable dynamic glow effect.
   | 2: Debug view, shows only the glow buffer.

..

:Name: r_dynamicGlowPasses
:Values: Integer >= 1
:Default: "5"
:Description:
   Number of blur passes applied to the glow.

..

:Name: r_dynamicGlowDelta
:Values: Float
:Default: "0.8"
:Description:
   Distance between the blur samples of each pass.

..

:Name: r_dynamicGlowIntensity
:Values: Float
:Default: "1.13"
:Description:
   Brightness of the glow.

..

:Name: r_dynamicGlowSoft
:Values: "0", "1"
:Default: "1"
:Description:
   Use soft blending for the glow.

..

:Name: r_dynamicGlowWidth / r_dynamicGlowHeight
:Values: Integer
:Default: "320" / "240"
:Description:
   Resolution of the glow buffer. Requires vid_restart.

   | 0: automatic, about a quarter of the screen resolution, with more blur
     passes at high resolutions so glows look the same size but stay sharp.

..

:Name: r_dynamicGlowFinish
:Values: "0", "1"
:Default: "0"
:Description:
   Wait for the GPU in the middle of the glow pass, as older versions always
   did. Only useful as a workaround for drivers that draw glows incorrectly.

..

:Name: r_colorGrade
:Values: "", "cinematic", "vivid", "cold", "warm", "noir"
:Default: "" (Not set)
:Description:
   Color mood applied by the post-process gamma pass (``r_gammamethod 2``) at
   no runtime cost. ``r_saturation``, ``r_contrast`` and ``r_vibrance``
   adjust it further, or work on their own. With ``r_fbo 1`` it is applied
   to the 3D view only, so the HUD, menus and console keep their colors.

..

:Name: r_colorGradeSplit
:Values: "0", "1"
:Default: "0"
:Description:
   Before/after comparison of the color settings: the left half of the screen
   keeps the original colors. With ``r_fbo 1`` the 3D view is split, and
   only it shows the divider.

..

:Name: r_contrast / r_saturation / r_vibrance
:Values: Float (0.5 - 1.5 / 0.0 - 2.0 / -1.0 - 1.0)
:Default: "1" / "1" / "0"
:Description:
   Contrast, saturation and vibrance (a saturation boost that spares already
   vivid colors) of the post-process color grading, see ``r_colorGrade``.

..

:Name: r_maxFrameLatency
:Values: "0" - "3"
:Default: "0"
:Description:
   How many frames the CPU may prepare ahead of the GPU. Drivers usually
   queue up to 3, which adds input lag. 1 waits each frame until the GPU is
   done (lowest lag, can cost frame rate), 2 lets one frame overlap.
   | 0: left to the driver.
   Needs OpenGL 3.2 or ARB_sync.

..

:Name: r_gpuTimers
:Values: "0", "1"
:Default: "0"
:Description:
   Measure the GPU time of each frame, and of the dynamic glow and the
   post-process passes (gamma, ``r_fbo`` effects), with GL timestamps read a
   few frames later so nothing waits for the GPU. ``cl_perfOverlay`` shows
   them and ``benchmark`` adds GPU time percentiles and a gpu_usec column.
   Needs OpenGL 3.3 or ARB_timer_query.

..

:Name: r_dlightPriority
:Values: "0", "1"
:Default: "0"
:Description:
   Surfaces can take 32 dynamic lights per scene. With more (big fights:
   sabers, shots, explosions), 0 keeps the first 32 added, whatever they
   are; 1 collects up to 256 and keeps the 32 that matter most for the view:
   lights entirely behind the viewer are dropped (not with a mirror or a
   portal in sight), the others rank by brightness and distance to the
   viewer or to a portal's camera. A light kept in the previous frame stays
   until another one matters clearly more, so the choice doesn't flicker.
   No difference with 32 lights or fewer. The enhanced, ultra, competitive
   and movie presets set 1.

..

:Name: r_dlightMode
:Values: "0", "1"
:Default: "0"
:Description:
   | 0: classic dynamic lights (saber, blaster and explosion lights), projected
     along the vertical axis, which smears them on walls and shows the
     triangles of large surfaces.
   | 1: per-pixel dynamic lights: round, smooth spots of about the classic
     size, shaded by the surface direction (both sides of two-sided
     surfaces, like grass, face the light). Needs ARB vertex and fragment
     programs, otherwise the classic lights are used. ``r_dlightBacks 0``
     leaves the sides facing away from the light dark.

..

:Name: r_fbo
:Values: "0", "1"
:Default: "0"
:Description:
   Render each frame into an offscreen buffer (framebuffer object) and show
   it through the post-process gamma pass. On its own it looks the same as 0,
   and the dynamic glow gets faster (no screen copies, finer blur);
   ``r_hdr`` and ``r_bloom`` need it. Requires ``r_gammamethod 2`` and
   vid_restart. With MSAA the offscreen buffer is multisampled.
   ``r_measureOverdraw`` is turned off with it.

..

:Name: r_hdr
:Values: "0", "1"
:Default: "0"
:Description:
   With ``r_fbo``, render the 3D view in 16-bit floating point, so additive
   light (sabers, blaster bolts, explosions, lights) keeps adding up past
   full brightness. At the end of the view the highlights roll off smoothly
   towards white instead of clipping; the HUD and menus are not affected.
   Requires vid_restart.

..

:Name: r_exposure
:Values: Float (0.25 - 4.0)
:Default: "1"
:Description:
   Brightness of the 3D view before the highlight roll-off of ``r_hdr``.

..

:Name: r_bloom
:Values: "0", "1", "2"
:Default: "0"
:Description:
   With ``r_fbo``, bright parts of the 3D view glow softly into their
   surroundings; with ``r_hdr`` the glow keeps their color. Requires
   vid_restart.

   | 2: debugging, blooms the whole frame including the HUD, at threshold 0.

..

:Name: r_bloomIntensity / r_bloomThreshold
:Values: Float (0.0 - 4.0 / 0.0 - 1.0)
:Default: "0.5" / "0.75"
:Description:
   Strength of the bloom, and the brightness (as a fraction of white) above
   which parts of the view bloom.

..

:Name: r_environmentMapping
:Values: "0", "1"
:Default: "1"
:Description:
   Disable environment mapping for better performance on low-end
   machines.

..

:Name: r_ext_multisample
:Values: "0", "2", "4", "8", "16"
:Default: "0"
:Description:
   Multisample anti-aliasing. May not work on all machines.

..

:Name: r_ext_alphaToCoverage
:Values: "0", "1"
:Default: "0"
:Description:
   With multisampling (``r_ext_multisample``), antialias the cut-out edges of
   foliage, fences and grates using alpha to coverage. Their textures get a
   copy with sharpened alpha, so leaves and bars stay solid at any distance
   and only their edges are smoothed. Requires vid_restart.

..

:Name: r_ext_texture_filter_anisotropic
:Values: "0", "2", "4", "8", "16"
:Default: "2"
:Description:
   Anisotropic filtering level. Higher values increase image quality
   with little performance loss.

..

:Name: r_fontSharpness
:Values: Decimal >= 0
:Default: "1"
:Description:
   Relative font sharpness (doesn't affect console font).

   | 0: Always use original low-res fonts
   | 1: Best quality (in fau's opinion)

..

:Name: r_gammamethod
:Values: "0", "1", "2"
:Default: "2"
:Description:
   Method for applying gamma correction. Keep in mind that using
   non-functional gamma method disables not only ``r_gamma``, but also
   ``r_overbrightbits``.

   | 0: Pre-processing. Causes washed out colors. Use as last resort.
   | 1: Hardware gamma. Works only in fullscreen.
   | 2: Post-processing. Works in both fullscreen and windowed.

..

:Name: r_highdpi
:Values: "0", "1"
:Default: "1"
:Description:
   Enable / Disable high DPI rendering when desktop scaling is
   enabled.

..

:Name: r_openglMipMaps
:Values: "0", "1"
:Default: "1"
:Description:
   Enable / Disable OpenGL mipmap generation. Disable to restore
   original downsampling algorithms.

..

:Name: r_saberGlow
:Values: "0", "1"
:Default: "1"
:Description:
   Enable / Disable dynamic glow on saber shaders. Turn off
   if it breaks your custom saber model.

..

:Name: r_textureLODBias
:Values: Decimal
:Default: "0"
:Description:
   Adjust OpenGL texture Level of Detail bias. Useful for some low
   quality video drivers. Small negative values (eg "-0.2") can help
   with distant textures appearing blurry.

..

:Name: s_muteWhenMinimized
:Values: "0", "1"
:Default: "1"
:Description:
   Mute all sounds when client window is minimized.

..

:Name: s_muteWhenUnfocused
:Values: "0", "1"
:Default: "1"
:Description:
   Mute all sounds when client window is unfocused.

..

:Name: r_printMissingModels
:Values: "0", "1"
:Default: "0"
:Description:
   Print a warning when a model fails to load.

..

:Name: con_opacity
:Values: 1 >= Decimal >= 0
:Description:
   Opacity of the in-game console.

..

:Name: con_skipNotifyKeyword
:Values: String
:Default: "" (Not set)
:Description:
   Keyword used by modules to print messags into the console that
   should not appear as notifications. JKA uses the builtin keyword
   ``[skipnotify]`` and some mods seem to have adopted this. To increase
   compatibility with those mods this cvar can be used.

-----------
Server-Side
-----------

:Name: mv_apiConnectionless
:Valid: "0", "1"
:Default: "1"
:Description:
   Controls if game module may use MVAPI 1 to receive and send
   connectionless packets with arbitrary source and destination. When
   disabled SysCalls always return qtrue as if error occured.

..

:Name: mv_serverversion
:Valid: "auto", "1.04", "1.03", "1.02"
:Default: "1.04"
:Description:
   Decides which gameversion the server will run on. "auto" will host
   a 1.04 server if assets5.pk3 is found, 1.03 if assets2.pk3
   is available and if only assets0.pk3 and assets1.pk3 can be found
   it will host a 1.02 server. *Make sure you have only mods
   compatible with the hosted gameversion in your base/mod directory.
   The dedicated server expects you to know what you are doing.*

..

:Name: mv_httpdownloads
:Valid: "0", "1"
:Default: "0"
:Description:
   Switches http downloads on and off.

..

:Name: mv_httpserverport
:Valid: 0-65535 (TCP Port), Any URL (http://...)
:Default: "0"
:Description:
   If a number is provided it decides on which TCP port the builtin
   HTTP-Server will listen on. If set to zero it will automatically
   choose a port between 18200 and 18215, trying every single one till
   it finds an unused port. Make sure that this port is opened in your
   Firewall / NAT. Since JK2MV 1.1 external HTTP Servers are
   supported. The URL should point to the GameData directory of your
   file server. Note that clients also need at least JK2MV 1.1 in case
   you are using a URL. Older JK2MV versions will not detect the
   availability of HTTP Downloads in this case.

..

:Name: mv_fixnamecrash
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks the use of chars from the extended ASCII table which can
   cause a crash if used correctly.

..

:Name: mv_fixforcecrash
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks the use of malformed forceconfig strings which can cause a
   crash if used correctly.

..

:Name: mv_fixgalaking
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks the use of "galak_mech" as a playermodel on the serverside
   so legacy clients will not crash. Only useful in 1.02 mode.

..

:Name: mv_fixbrokenmodels
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks the use of "kyle/fpls" and "morgan" as a playermodel. These
   models have invisible parts and thus are some kind of ghosting.
   Only useful in 1.02 mode.

..

:Name: mv_fixturretcrash
:Valid: "0", "1"
:Default: "1"
:Description:
   Removes all blaster missiles from the game before hitting the
   engine limit to prevent players from crashing a server with the
   turret/sentry.

..

:Name: mv_blockchargejump
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks a hack which can be used to jump higher then normally
   possible.

..

:Name: mv_blockspeedhack
:Valid: "0", "1"
:Default: "1"
:Description:
   Blocks the speedhack which can be used to run faster.

..

:Name: mv_fixsaberstealing
:Valid: "0", "1"
:Default: "1"
:Description:
   Prevents spectators from stealing saber.

..

:Name: mv_fixplayerghosting
:Valid: "0", "1"
:Default: "1"
:Description:
   Prevents "player ghosting" bug, where players can freely walk
   through affected player.

..

:Name: mv_resetServerTime
:Valid: "0", "1", "2"
:Default: "1"
:Description:
   Reset internal server time on map restart. Helps to avoid high
   server time bugs. Breaks queue in duel gametype on basejk. May
   cause issues with other mods.

   | 0: Never (compatible)
   | 1: Always except in Duel gametype
   | 2: Always

..

:Name: sv_autoWhitelist
:Values: "0", "1"
:Default: "1"
:Description:
   Automatically add IPs of players to a whitelist. Whitelisted IPs
   are can still access the server while it's under a DOS attack and
   they are stored in ipwhitelist.dat file. Collecting IP addresses
   without consent may be against European Union's General Data
   Protection Regulation.

..

:Name: sv_enforceSnaps
:Values: "0", "1"
:Default: "0"
:Description:
   Ignore the client preference for "snaps" and try to send a snapshot per
   server frame (sv_fps) if sv_maxSnaps and the client rate permit it.

..

:Name: sv_floodProtect
:Values: Integer >= 0
:Default: "3"
:Description:
   | 0: Disable flood protection.
   | 1: Original flood protection - 1 client command per second.
   | 2+: Relaxed flood protection - Allow sv_floodProtect commands
     at once (burst), after this 1 command per second (rate).

..

:Name: sv_hibernateFps
:Values: Integer >= 0
:Default: "4"
:Description:
   The fps to use while the server is in hibernation mode, in which it uses
   less CPU power. The server hibernates when no human player is connected
   (bots do not count), at the earliest 10 seconds after a map load.
   The value zero disables hibernation mode. Set it to 0 for load tests with
   bots.

..

:Name: sv_maxOOBRate
:Valid: 1-1000
:Default: "20"
:Description:
   Max out-of-bound requests handled per second. Increasing rate
   improves server responsiveness at the cost of higher CPU usage.

..

:Name: sv_maxRate
:Valid: "0", Integer >= 1000
:Default: "90000"
:Description:
   Maximum rate for each client. The client rate limits the maximum amount of
   snapshots sent to a client.

..

:Name: sv_maxSnaps
:Valid: Integer > 0
:Default: "30"
:Description:
   Maximum amount of snapshots each client should receive. This can also be
   limited by the client rate.

..

:Name: sv_minRate
:Valid: Integer >= 1000
:Default: "1000"
:Description:
   Minimum rate for each client. The client rate limits the maximum amount of
   snapshots sent to a client.

..

:Name: sv_minSnaps
:Valid: Integer > 0
:Default: "1"
:Description:
   Minimum amount of snapshots each client should receive. This can also be
   limited by the client rate.

..

:Name: sv_pingFix
:Values: "0", "1"
:Default: "1"
:Description:
   Enable more accurate and bug-free ping calculation.

..

:Name: sv_dynamicSnapshots
:Values: "0", "1"
:Default: "1"
:Description:
   Try to send partial snapshots if a snapshot message would otherwise overflow.
   This should help to avoid clients from dropping due to
   ``CL_ParseServerMessage: read past end of server message`` when maps or mods
   cause a lot of commands to be sent to a client in a short interval on a busy
   server.

..

:Name: sv_snapshotEntityBudget
:Valid: 64-1024
:Default: "128"
:Description:
   Size of the ring that keeps the entities of recent snapshots, so that a
   client can be sent only what changed since the last snapshot it received.
   While snapshots average no more entities than this value, the ring keeps
   the last 32 snapshots of every client slot, all a client may delta from,
   even when every slot builds a snapshot each server frame as bots do. A
   client whose last received snapshot has left the ring gets a full
   snapshot instead, several times larger, and that happens when the server
   is busiest: big fights, many bots, high ``sv_fps``. With 31 bots on
   ffa_bespin at ``sv_fps`` 40, 64 (the size of earlier versions) kept at
   worst the last 0.6 s of snapshots and 128 kept 1.4 s, the most a client
   at 20 snapshots per second can delta from. A listen server gets the same
   ring (earlier versions kept 4 snapshots per slot there), since its bots
   and local client build a snapshot every client frame. Latched: takes
   effect on the next map load or ``map_restart``. The ring takes
   sv_maxclients x 32 x value x 296 bytes, rounded up to a power of two
   (exact for 8, 16 or 32 slots; 17 to 31 slots cost as much as 32):

   | 64: 592 KB per slot, 18.5 MB for 32 slots
   | 128: 1.2 MB per slot, 37 MB for 32 slots, 9.25 MB for 8
   | 256: 2.3 MB per slot, 74 MB for 32 slots
   | 512: 4.6 MB per slot, 148 MB for 32 slots
   | 1024: 9.3 MB per slot, 296 MB for 32 slots

   When a ring that large can't be allocated (32-bit builds), a smaller one
   is used and a warning is printed.

==================
Undocumented Cvars
==================

* com_maxfpsMinimized
* com_maxfpsUnfocused
* in_nograb
* mv_coloredTextShadows
* net_dropsim (dev cvar)
* net_enabled
* r_allowsoftwaregl
* r_convertModelBones
* r_loadSkinsJKA
* r_noborder
* r_centerWindow
* s_sdlBits
* s_sdlSpeed
* s_sdlChannels
* s_sdlDevSamps
* s_sdlMixSamps

=============
Other Changes
=============

* Demo tools, only active during demo playback:

  - ``demo_pause``, ``demo_speed <x>``, ``demo_faster``, ``demo_slower`` and
    ``demo_step [ms]`` (advance a paused demo). Slow motion now plays at the
    requested speed instead of getting stuck at 1 ms of game time per frame.
  - ``demo_freecam``: fly freely through the demo with the movement keys and
    the mouse; sound and effects follow the camera. It starts as the game's
    own view; once it moves or turns, or the recorded player does, it backs
    off a little behind the recorder's head and shows the recorder's own
    body. The recorder's own sounds stay at full volume.
  - Camera paths: ``cam_add`` records a key (position, angles, fov, speed)
    at the current demo time, ``cam_play`` follows the smooth path through the
    keys, with smooth turns (speed keys make slow motion ramps), ``cam_del [index]``,
    ``cam_clear``, ``cam_list``, ``cam_save <name>`` and ``cam_load <name>``
    (demos/<name>.cam).
  - ``photomode`` pauses the demo, frees the camera and hides the HUD; again
    restores everything. During ``cam_play``, ``photomode`` and
    ``demo_freecam`` start from the path's current view.

* ``video_mp4 [name]`` records the demo being played to videos/<name>.mp4
  (H.264 with BT.709 colors, AAC sound from the software mixer) through
  ffmpeg, which must be in the PATH, next to the game on Windows, or in the
  game's directory, /usr/local/bin or /opt/homebrew/bin on Linux and macOS;
  ``stopvideo`` ends it (adding the sound to a long recording takes a
  while: the screen counts the seconds, and the window stays responsive).
  Names are up to 42 letters, digits, '_', '-' and '.'. When ffmpeg
  fails, its messages are in videos/<name>.mp4.log. The frame rate comes
  from ``cl_aviFrameRate``, the quality from ``cl_mp4Crf`` (default 18, lower
  is better, 0 - 51) and ``cl_mp4Preset`` (x264 preset, default "medium").
* ``video`` and ``video_mp4``: a sound starts on the video frame where it was
  played, whatever the real frame rate (it used to come 0.1 to 0.2 s late).
* ``cl_aviMotionBlur N`` (2 - 32, default 0): ``video`` and ``video_mp4`` run
  N game frames per video frame and blend them, for real motion blur (the
  window shows the frames as they come). Needs ``r_fbo 1``; recording takes
  N times longer. At most 1000 game frames per second: 16 frames at 60 fps,
  none above 500 fps. ``cl_aviFrameRate`` and ``cl_aviMotionBlur`` are read
  when a recording starts.
* ``screenshot_png [name | silent]`` takes a lossless PNG screenshot.
* ``testscene <map> [x y z [yaw [pitch]]] [dlight | dlights] [spin]`` draws a
  map from a fixed camera while disconnected, in place of the main menu and
  without the game modules, to try renderer settings (the renderer smoke
  test uses it); ``testscene off`` or Escape brings the menu back.
  ``dlight`` adds a dynamic light in front of the camera; ``dlights`` adds it
  after 40 others behind the camera, past the classic limit of 32 lights
  (``r_dlightPriority``). ``spin`` turns the camera, and ``video`` and
  ``video_mp4`` work there too, to try the video settings; the recording
  stops with the scene.
* ``benchmark <demo> [runs] [warmup] [tag]`` plays a demo in timedemo mode
  several times without the 1000 fps cap and prints the average fps, frame
  time percentiles and the 1% low per run and pooled; it writes every frame
  time to benchmarks/<demo>_<tag>.csv and the summary to a .txt file.
  ``benchmark stop`` aborts it. The first run starts right away and the
  commands after ``benchmark`` in a script keep running meanwhile (a
  ``quit`` there cuts it short); to quit or go on once the results are
  written, set ``nextdemo`` first, as in ``set nextdemo quit; benchmark mydemo``.
* ``r_speeds 8`` prints the draw calls, texture binds and state changes of
  each frame.
* New command ``preset classic|enhanced|ultra|competitive|movie`` sets a group
  of visual cvars in one go and restarts the renderer if needed, right away:
  the commands after it in a script or bind run with the preset applied, and
  each preset of a chain restarts the renderer in turn. Every preset
  sets the whole group (what it doesn't change goes back to the default), so
  ``preset classic`` is the original look. It works in autoexec.cfg too.
  ``preset`` alone lists them.
* ``r_fullscreen 2`` is a borderless fullscreen window at the desktop
  resolution, without a display mode change. Alt+Enter toggles between
  windowed and the last fullscreen mode, remembered across sessions in
  ``r_fullscreenLast``.
* ``r_finish 0`` now also applies to menu and loading frames.
* cl_avidemo replaced by cl_aviFrameRate
* cl_conspeed renamed to con_speed
