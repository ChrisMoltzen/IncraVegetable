# IncraVegetable art list

Put your images in the `assets/` folder using these names (PNG recommended; JPG, BMP and TGA also work).
Any image you leave out uses the built-in art, so you can replace things one at a time.
Sizes are what the game was designed around; other sizes are scaled to fit.
**9-slice** images keep their corners and stretch their middle, so one image fits any size box.
Variants ending in `_hover`, `_pressed` or `_disabled` are optional: without them the normal image
is brightened or darkened automatically.

| File | Size | 9-slice | What it is |
|---|---|---|---|
| `farm/background.png` | 1280 x 720 |  | Everything behind the vegetable patch while farming |
| `farm/hud_bar.png` | 1280 x 72 |  | Strip along the top of the farm screen (title, timer, coins sit on it) |
| `farm/bed.png` | 640 x 400 |  | The rectangular garden bed the vegetables grow in. Stretched to fit, always 1.6x wider than tall. Plants sit inside the middle ~90%, so leave a border of soil or edging around them |
| `farm/soil.png` | 128 x 128 |  | Mound of soil under each vegetable (sits in the lower part of the plant's square) |
| `farm/soil_hover.png` | 128 x 128 |  | Mound under the plant the pointer is on (optional) |
| `crops/lettuce.png` | 128 x 128 |  | Ripe Lettuce, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/carrot.png` | 128 x 128 |  | Ripe Carrot, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/pumpkin.png` | 128 x 128 |  | Ripe Pumpkin, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/sprout.png` | 128 x 128 |  | Growing plant, drawn small when just planted and full size just before it ripens. Keep the stem's base about 2/3 of the way down |
| `crops/lettuce_sprout.png` | 128 x 128 |  | Growing Lettuce (optional - otherwise crops/sprout is used) |
| `crops/carrot_sprout.png` | 128 x 128 |  | Growing Carrot (optional - otherwise crops/sprout is used) |
| `crops/pumpkin_sprout.png` | 128 x 128 |  | Growing Pumpkin (optional - otherwise crops/sprout is used) |
| `farm/farmer.png` | 128 x 128 |  | Hired farmer, standing. Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_walk1.png` | 128 x 128 |  | Farmer walking, first step (optional - else farm/farmer bobs). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_walk2.png` | 128 x 128 |  | Farmer walking, second step (optional). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_pick.png` | 128 x 128 |  | Farmer bending down to pick a crop (optional). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `fx/particle.png` | 32 x 32 |  | Burst particle when a vegetable is picked. Draw it WHITE: the game tints it to the crop's colour |
| `fx/reach_circle.png` | 256 x 256 |  | Picking area around the pointer (Wide Reach upgrade). Usually see-through |
| `ui/hud_logo.png` | 336 x 30 |  | Game name in the top-left of the farm screen |
| `ui/coin.png` | 64 x 64 |  | Coin icon |
| `ui/timer_back.png` | 406 x 36 |  | Behind the day timer bar |
| `ui/timer_fill.png` | 400 x 30 |  | Day timer bar when full. It's cropped from the right as the day runs out |
| `ui/pick_bar_back.png` | 128 x 16 |  | Behind the picking progress bar under a vegetable |
| `ui/pick_bar_fill.png` | 128 x 16 |  | Picking progress bar when full (cropped while picking) |
| `ui/pause_button.png` | 96 x 96 |  | Pause button, top-right while playing |
| `ui/pause_button_hover.png` | 96 x 96 |  | Pause button with the mouse over it (optional) |
| `ui/debug_button.png` | 80 x 80 |  | Debug screen button, bottom-left |
| `ui/debug_button_hover.png` | 80 x 80 |  | Debug button with the mouse over it (optional) |
| `ui/button_primary.png` | 256 x 80 | yes | Main orange button (New Game, Start Day...) |
| `ui/button_primary_hover.png` | 256 x 80 | yes | Main orange button (New Game, Start Day...) - mouse over it (optional) |
| `ui/button_primary_pressed.png` | 256 x 80 | yes | Main orange button (New Game, Start Day...) - being pressed (optional) |
| `ui/button_primary_disabled.png` | 256 x 80 | yes | Main orange button (New Game, Start Day...) - can't be used right now (optional) |
| `ui/button_secondary.png` | 256 x 80 | yes | Green button (Continue, Resume, Tech Tree...) |
| `ui/button_secondary_hover.png` | 256 x 80 | yes | Green button (Continue, Resume, Tech Tree...) - mouse over it (optional) |
| `ui/button_secondary_pressed.png` | 256 x 80 | yes | Green button (Continue, Resume, Tech Tree...) - being pressed (optional) |
| `ui/button_secondary_disabled.png` | 256 x 80 | yes | Green button (Continue, Resume, Tech Tree...) - can't be used right now (optional) |
| `ui/button_danger.png` | 256 x 80 | yes | Red button (Overwrite, Quit Game...) |
| `ui/button_danger_hover.png` | 256 x 80 | yes | Red button (Overwrite, Quit Game...) - mouse over it (optional) |
| `ui/button_danger_pressed.png` | 256 x 80 | yes | Red button (Overwrite, Quit Game...) - being pressed (optional) |
| `ui/button_danger_disabled.png` | 256 x 80 | yes | Red button (Overwrite, Quit Game...) - can't be used right now (optional) |
| `ui/button_ghost.png` | 256 x 80 | yes | Grey button (Settings, Back...) |
| `ui/button_ghost_hover.png` | 256 x 80 | yes | Grey button (Settings, Back...) - mouse over it (optional) |
| `ui/button_ghost_pressed.png` | 256 x 80 | yes | Grey button (Settings, Back...) - being pressed (optional) |
| `ui/button_ghost_disabled.png` | 256 x 80 | yes | Grey button (Settings, Back...) - can't be used right now (optional) |
| `ui/panel.png` | 256 x 256 | yes | Pop-up box behind menus (pause, settings, day summary, confirm) |
| `ui/panel_header.png` | 560 x 70 |  | Coloured title strip at the top of the day summary |
| `ui/tooltip.png` | 128 x 128 | yes | Box behind tech tree upgrade descriptions |
| `ui/slider_track.png` | 300 x 14 |  | Empty volume slider |
| `ui/slider_fill.png` | 300 x 14 |  | Full volume slider (cropped to the volume) |
| `ui/slider_knob.png` | 64 x 64 |  | Handle you drag on a volume slider |
| `ui/font.png` | 256 x 96 |  | Optional font: 16 x 6 grid of equal-sized characters, ASCII 32 (space) to 127 in order, drawn WHITE (the game colours them). Leave it out to keep the built-in font |
| `menu/background.png` | 1280 x 720 |  | Main menu background (vegetable rows drift over it; turn them off in art.txt) |
| `menu/logo.png` | 900 x 112 |  | Game title on the main menu |
| `menu/slots_background.png` | 1280 x 720 |  | Background of the New Game / Load Game slot screen |
| `tree/background.png` | 1280 x 720 |  | Tech tree background |
| `tree/header_bar.png` | 1280 x 64 |  | Strip along the top of the tech tree |
| `tree/node.png` | 236 x 78 | yes | Upgrade you can't afford yet |
| `tree/node_affordable.png` | 236 x 78 | yes | Upgrade you can buy now |
| `tree/node_locked.png` | 236 x 78 | yes | Upgrade whose requirements aren't met |
| `tree/node_maxed.png` | 236 x 78 | yes | Fully upgraded |
| `tree/icons/patch.png` | 64 x 64 |  | Icon for the 'Bigger Patch' upgrade (optional - shown on the left of its box) |
| `tree/icons/seeds.png` | 64 x 64 |  | Icon for the 'More Seeds' upgrade (optional - shown on the left of its box) |
| `tree/icons/daylength.png` | 64 x 64 |  | Icon for the 'Longer Days' upgrade (optional - shown on the left of its box) |
| `tree/icons/pickspeed.png` | 64 x 64 |  | Icon for the 'Quick Hands' upgrade (optional - shown on the left of its box) |
| `tree/icons/headstart.png` | 64 x 64 |  | Icon for the 'Head Start' upgrade (optional - shown on the left of its box) |
| `tree/icons/growspeed.png` | 64 x 64 |  | Icon for the 'Fertile Soil' upgrade (optional - shown on the left of its box) |
| `tree/icons/value.png` | 64 x 64 |  | Icon for the 'Prize Produce' upgrade (optional - shown on the left of its box) |
| `tree/icons/reach.png` | 64 x 64 |  | Icon for the 'Wide Reach' upgrade (optional - shown on the left of its box) |
| `tree/icons/carrots.png` | 64 x 64 |  | Icon for the 'Carrots' upgrade (optional - shown on the left of its box) |
| `tree/icons/pumpkins.png` | 64 x 64 |  | Icon for the 'Pumpkins' upgrade (optional - shown on the left of its box) |
| `tree/icons/autopick.png` | 64 x 64 |  | Icon for the 'Helping Hand' upgrade (optional - shown on the left of its box) |
| `tree/icons/autopickchance.png` | 64 x 64 |  | Icon for the 'Lucky Streak' upgrade (optional - shown on the left of its box) |
| `tree/icons/autopickcount.png` | 64 x 64 |  | Icon for the 'Bumper Bunch' upgrade (optional - shown on the left of its box) |
| `tree/icons/autopickradius.png` | 64 x 64 |  | Icon for the 'Spreading Roots' upgrade (optional - shown on the left of its box) |
| `tree/icons/farmhand.png` | 64 x 64 |  | Icon for the 'Farmhand' upgrade (optional - shown on the left of its box) |
| `tree/icons/farmcrew.png` | 64 x 64 |  | Icon for the 'Farm Crew' upgrade (optional - shown on the left of its box) |
| `tree/icons/farmerspeed.png` | 64 x 64 |  | Icon for the 'Comfy Boots' upgrade (optional - shown on the left of its box) |
| `tree/icons/farmerpick.png` | 64 x 64 |  | Icon for the 'Sharp Shears' upgrade (optional - shown on the left of its box) |
