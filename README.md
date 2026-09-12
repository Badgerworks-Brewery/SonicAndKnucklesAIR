# Sonic and Knuckles Oxygen enhanced

Source code incl. dependencies for "Sonic 3 - Angel Island Revisited", a fan-made remaster of Sonic 3 & Knuckles, modded to run the pc collection version of the game.

S3AIR Project homepage: https://sonic3air.org/


### Disclaimer

Sonic 3 A.I.R. is a non-profit fan game project. It is not affiliated in any way with SEGA or Sonic Team, the original creators of Sonic 3 and Sonic & Knuckles.

Sonic the Hedgehog is a trademark of SEGA. All copyrights regarding Sonic the Hedgehog, including characters, names, terms, art, and music belong to SEGA. All registered trademarks belong to SEGA and Sonic Team.

The developers of Sonic 3 A.I.R. have no intent to infringe said copyrights and registered trademarks.
No financial gain is made from this project.

Any commercial use of this project without SEGA's explicit consent is strictly prohibited.


## Repository overview

This repository is split into several different projects:
* The larger external dependencies (namely SDL2, Ogg/Vorbis, zlib) inside the "framework" directory. These are copies of the respective open source projects, with a few custom changes applied where needed - see the "how-to-build.txt" files in there for details.
* The librmx libraries that together with the external dependencies build a foundation for my own projects (S3AIR and my private stuff). This primarily consists of rmxbase, a collection of helper and utility classes, and rmxmedia, which is providing a basic game framework built on top of SDL2 & OpenGL.
* Lemonscript language library, with compiler and runtime environment for script execution.
* Oxygen Engine, the backbone game engine powering Sonic 3 A.I.R. This includes input, graphics, audio handling, and all the other game application stuff, as well as a simulation environment built around lemonscript that - as far as needed for the purposes of S3AIR - emulates aspects of Sega Genesis hardware. Note that Oxygen can be built as its own application (OxygenApp) that lacks the S3AIR C++ code.
* S3AIR-specific C++ code, scripts and data in the "Oxygen/sonic3air" directory. Yes, that's what it's named.


## Sonic & Knuckles Collection (PC, 1997) support

Besides the original Genesis/Mega Drive ROM, the game can also be run from
`SONIC3K.EXE`, the executable of the official 1997 Windows PC port
("Sonic & Knuckles Collection"). On first launch, when prompted for a ROM,
point the file selection dialog at your own legitimately-owned `SONIC3K.EXE`
instead of a `.bin` ROM file.

Technical background: that executable embeds the same Genesis-address-mapped
game data used by the Mega Drive ROM, copied verbatim at matching file byte
offsets (i.e. Genesis address `X` sits at file offset `X`, verified against a
retail copy). This is configured via the `S3K_OldPC` entry in
`Oxygen/sonic3air/oxygenproject.json` (`RomType: "PC"`, `PCDataOffset`,
`PCDataSize`).

**Current status: partial, not yet fully playable.** The executable never
executes 68k code itself -- the PC port replaced the original boot code and
several small pointer/lookup tables that were interleaved with it with native
x86 code, so those specific address ranges can't be read from the PC
executable at all (confirmed by inspection: real x86 opcodes sit where 68k
data should be). Booting straight from `SONIC3K.EXE` alone crashes almost
immediately (`Kosinski.decompress` reads a garbage pointer during the title
screen boot sequence).

An optional patch-overlay mechanism addresses this for anyone who also owns
a legitimate Genesis ROM: set `PatchSourceRomName` and `PatchRanges` on a
`RomInfo` entry, and those byte ranges are copied from that ROM on top of the
extracted PC data after loading (see `ResourcesCache::applyPCPatchRanges`).
Verified with a byte-perfect `Sonic_Knuckles_wSonic3.bin` supplied by a user:
patching `0x000000`-`0x1ffff0` (the low range replaced by native code) gets
past the original crash, through the main menu, and into the data select
screen -- further than the PC executable manages alone. A second, still
unidentified mismatch remains above that range (`VDP_copyToVRAM` overflow
loading a save-slot preview sprite), so no zone has been reached yet this
way. Neither the patch source ROM nor any extracted game data is bundled
with this repository -- users must supply their own legitimately-owned
files for both the PC executable and, optionally, the patch source ROM.

Limitations: audio in the retail PC executable uses General MIDI/PCM rather
than the Genesis FM synth; S3AIR's own remastered soundtrack is used instead
regardless of ROM source, so this doesn't currently affect playback.


## How to build

For information on how to build for different platforms, find the readme files in the respective subdirectories of "Oxygen/sonic3air/build":
* Windows: "_vstudio"
* Mac:     "_xcode"
* Linux:   "_cmake"
* Android: "_android"
* Web:     "_emscripten"
* Switch:  "_make" (unmaintained)

Additional platform ports:
* Vita: See https://github.com/v-atamanenko/sonic3air


## External dependencies

External libraries and code used in this project:
* SDL2 - in "framework/external/sdl"
* libogg & libvorbis - in "framework/external/ogg-vorbis"
* zlib incl. minizip - in "framework/external/zlib"
* libcurl - in "framework/external/curl"
* Dear ImGui - in "framework/external/imgui"
* jsoncpp - in "librmx/source/rmxbase/jsoncpp"
* GLEW - in "librmx/source/rmxmedia/glew"
* Sound chip emulation related code from Genesis Plus GX - in "Oxygen/oxygenengine/source/oxygen/simulation/sound"
* Discord Game SDK - in "Oxygen/sonic3air/source/external/discord_game_sdk"
* xBRZ upscaler shader code - in "Oxygen/oxygenengine/data/shader" and once more in "Oxygen/sonic3air/data/shader"
* Hqx upscaler shader code & data files - in "Oxygen/oxygenengine/data/shader" and once more in "Oxygen/sonic3air/data/shader"


## Contributors

Thanks to all contributors!

Source code contributions by:
* Sappharad
* Heyjoeway
* Carjem Generations
* Ultracoolguy
* gl33ntwine
* Rinnegatamante
* MDashK
* CodenameGamma
* LelJader

Remastered soundtrack by:
* SpinnRG

Game scripts & other contributions by:
* Vinegar
* Thorn
* Legobouwer
* GFX32
* Dynamic Lemons
* AmberChromatic
* iCloudius
* D.A. Garden
* Alieneer
* 3Pills
* Elsie The Pict
* nabbup
* mrgrassman14
* Vague Rant
* PaperTriangle
* Crappy Productions
* AtomicRey

Additional thanks:
* All contributors of the Sonic 3 / Sonic & Knuckles Disassembly (https://github.com/sonicretro/skdisasm), which has proven itself a valuable source of information on S3&K code


## Want to contribute?

Here's the bad news: This repository isn't meant for direct distribution. If you have your own changes that you want to share with the world, create a **fork** of this repo, instead of making pull requests. I'd very much appreciate that as I really want to avoid having too much overhead with managing / reviewing code changes made by others. I'd rather use the time to continue with implementing my own ideas into the project.

Plus there's a second reason, and that's an important one for me as well: It's about code ownership and software licenses. It's much easier to use the librmx, lemonscript and Oxygen Engine code elsewhere under a different license than GPL if I don't have to ask a larger group of contributors whether they are okay with it. Because there are some plans for possible future projects using these codes as a foundation, maybe even commercial ones where GPL could make things complicated.

-- Euka
