#pragma once

struct SDL_Renderer;
class OamTileBank;

// Loads the eight native-size canonical Orbital PNGs into their OAM tile IDs.
// Invalid or missing frames remain unmapped and render as explicit fallbacks.
int LoadOrbitalTiles(SDL_Renderer* renderer, OamTileBank& tiles);
