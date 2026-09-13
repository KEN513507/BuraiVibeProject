# Enemy assets

The crawler is drawn directly on a 32x32 pixel grid in `tools/generate_crawler.py`; the attached large image is a visual reference. This keeps the NES sprite grid and three opaque colors plus transparency.

Run `python -m pip install -r tools/requirements.txt`, then `python tools/generate_crawler.py` and `python tools/process_enemies.py`. The processor reads every PNG in `assets/raw_enemies/`, removes green or white backgrounds, maps each 16x16 or 32x32 frame to at most three opaque NES-style colors, and writes a PNG and matching JSON to `assets/sprites/`. Use `--frame-size 16` for 16x16 sheets. Source images must already be on the chosen pixel grid; the processor never resizes them.

`EnemyManager` stores type definitions and frame-based spawn events. Positions and patrol bounds are map coordinates. The crawler's first event is at frame 1 on the platform from x=160 to x=272, with its feet at y=160. It patrols horizontally and jumps when the player's center is within 64 pixels. The terrain and enemy use the same map coordinates, so scrolling keeps them aligned.
