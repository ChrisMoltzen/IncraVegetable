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
| `farm/fence_h.png` | 64 x 64 |  | Fence tile along the top and bottom of the bed (joins left and right) |
| `farm/fence_v.png` | 64 x 64 |  | Fence tile down the left and right sides of the bed (joins top and bottom) |
| `farm/fence_corner.png` | 64 x 64 |  | Fence corner post (all four corners of the fence) |
| `farm/fence_gate.png` | 64 x 64 |  | Gate in the middle of the bottom fence (joins left and right like fence_h) |
| `farm/soil.png` | 128 x 128 |  | Mound of soil under each vegetable (sits in the lower part of the plant's square) |
| `farm/soil_hover.png` | 128 x 128 |  | Mound under the plant the pointer is on (optional) |
| `crops/lettuce.png` | 128 x 128 |  | Ripe Lettuce, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/carrot.png` | 128 x 128 |  | Ripe Carrot, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/pumpkin.png` | 128 x 128 |  | Ripe Pumpkin, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/turnip.png` | 128 x 128 |  | Ripe Turnip, filling the square one plant takes up (neighbours may overlap it by up to 10%) |
| `crops/sprout.png` | 128 x 128 |  | Growing plant, drawn small when just planted and full size just before it ripens. Keep the stem's base about 2/3 of the way down |
| `crops/lettuce_sprout.png` | 128 x 128 |  | Growing Lettuce (optional - otherwise crops/sprout is used) |
| `crops/carrot_sprout.png` | 128 x 128 |  | Growing Carrot (optional - otherwise crops/sprout is used) |
| `crops/pumpkin_sprout.png` | 128 x 128 |  | Growing Pumpkin (optional - otherwise crops/sprout is used) |
| `crops/turnip_sprout.png` | 128 x 128 |  | Growing Turnip (optional - otherwise crops/sprout is used) |
| `farm/farmer.png` | 128 x 128 |  | Hired farmer, standing. Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_walk1.png` | 128 x 128 |  | Farmer walking, first step (optional - else farm/farmer bobs). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_walk2.png` | 128 x 128 |  | Farmer walking, second step (optional). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `farm/farmer_pick.png` | 128 x 128 |  | Farmer bending down to pick a crop (optional). Feet at the bottom middle, about 8% up from the bottom edge. Draw it facing RIGHT: it's mirrored when the farmer walks left. |
| `fx/particle.png` | 32 x 32 |  | Burst particle when a vegetable is picked. Draw it WHITE: the game tints it to the crop's colour |
| `fx/reach_circle.png` | 256 x 256 |  | Picking area around the pointer (Wide Reach upgrade). Usually see-through |
| `ui/hud_logo.png` | 336 x 30 |  | Game name in the top-left of the farm screen |
| `ui/coin.png` | 64 x 64 |  | Coin icon |
| `ui/dial_sky.png` | 256 x 256 |  | Day/night dial: the WHOLE sky disc. Day half on top with the sun at the top middle, night half below with the moon at the bottom middle. The game turns it so the sun rises on the left and sets on the right; only the top half shows |
| `ui/dial_frame.png` | 280 x 150 |  | Day/night dial: frame drawn over the sky disc. Leave the half-circle window see-through (it fills the frame's width minus about 6% each side, with the horizon about 79% of the way down); the bottom strip hides the sun as it sets |
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
| `ui/button_secondary.png` | 256 x 80 | yes | Green button (Continue, Resume, The Barn...) |
| `ui/button_secondary_hover.png` | 256 x 80 | yes | Green button (Continue, Resume, The Barn...) - mouse over it (optional) |
| `ui/button_secondary_pressed.png` | 256 x 80 | yes | Green button (Continue, Resume, The Barn...) - being pressed (optional) |
| `ui/button_secondary_disabled.png` | 256 x 80 | yes | Green button (Continue, Resume, The Barn...) - can't be used right now (optional) |
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
| `ui/tooltip.png` | 128 x 128 | yes | Box behind upgrade descriptions in The Barn |
| `ui/slider_track.png` | 300 x 14 |  | Empty volume slider |
| `ui/slider_fill.png` | 300 x 14 |  | Full volume slider (cropped to the volume) |
| `ui/slider_knob.png` | 64 x 64 |  | Handle you drag on a volume slider |
| `ui/font.png` | 256 x 96 |  | Optional font: 16 x 6 grid of equal-sized characters, ASCII 32 (space) to 127 in order, drawn WHITE (the game colours them). Leave it out to keep the built-in font |
| `menu/background.png` | 1280 x 720 |  | Main menu background (vegetable rows drift over it; turn them off in art.txt) |
| `menu/logo.png` | 900 x 112 |  | Game title on the main menu |
| `menu/slots_background.png` | 1280 x 720 |  | Background of the New Game / Load Game slot screen |
| `tree/background.png` | 1280 x 720 |  | Background of The Barn (tech tree) |
| `tree/header_bar.png` | 1280 x 64 |  | Strip along the top of The Barn (tech tree) |
| `tree/node.png` | 96 x 96 | yes | Upgrade you can't afford yet |
| `tree/node_affordable.png` | 96 x 96 | yes | Upgrade you can buy now |
| `tree/node_locked.png` | 96 x 96 | yes | Upgrade whose requirements aren't met |
| `tree/node_maxed.png` | 96 x 96 | yes | Fully upgraded |
| `tree/icons/patch.png` | 64 x 64 |  | Tech icon 'patch' (Garden plot) - used by Bigger Patch 1, Bigger Patch 2, Bigger Patch 3, Bigger Patch 4, Bigger Patch 5, Bigger Patch 6, Bigger Patch 7, Bigger Patch 8, Bigger Patch 9, Bigger Patch 10, Bigger Patch 11, Bigger Patch 12, Bigger Patch 13, Bigger Patch 14, Bigger Patch 15, Bigger Patch 16, Bigger Patch 17, Bigger Patch 18, Bigger Patch 19, Bigger Patch 20, Bigger Patch 21, Bigger Patch 22, Bigger Patch 23, Bigger Patch 24, Bigger Patch 25, Bigger Patch 26, Bigger Patch 27, Bigger Patch 28 |
| `tree/icons/seeds.png` | 64 x 64 |  | Tech icon 'seeds' (Seed packet) - used by More Seeds 1, More Seeds 2, More Seeds 3, More Seeds 4, More Seeds 5, More Seeds 6, More Seeds 7, More Seeds 8, More Seeds 9, More Seeds 10, More Seeds 11, More Seeds 12, More Seeds 13, More Seeds 14, More Seeds 15, More Seeds 16, More Seeds 17, More Seeds 18, More Seeds 19, More Seeds 20, More Seeds 21, More Seeds 22, More Seeds 23, More Seeds 24, More Seeds 25 |
| `tree/icons/daylength.png` | 64 x 64 |  | Tech icon 'daylength' (Sun) - used by Longer Days 1, Longer Days 2, Longer Days 3, Longer Days 4, Longer Days 5, Longer Days 6, Longer Days 7, Longer Days 8, Longer Days 9, Longer Days 10, Longer Days 11, Longer Days 12, Longer Days 13, Longer Days 14, Longer Days 15, Longer Days 16, Longer Days 17, Longer Days 18, Longer Days 19, Longer Days 20, Longer Days 21, Longer Days 22, Longer Days 23, Longer Days 24, Longer Days 25, Longer Days 26, Longer Days 27, Longer Days 28, Longer Days 29, Longer Days 30 |
| `tree/icons/headstart.png` | 64 x 64 |  | Tech icon 'headstart' (Sunrise) - used by Head Start 1, Head Start 2, Head Start 3, Head Start 4, Head Start 5 |
| `tree/icons/pickspeed.png` | 64 x 64 |  | Tech icon 'pickspeed' (Quick lettuce) - used by Quick Hands 1, Quick Hands 2, Quick Hands 3, Quick Hands 4, Quick Hands 5, Quick Hands 6, Quick Hands 7, Quick Hands 8, Quick Hands 9, Quick Hands 10 |
| `tree/icons/growspeed.png` | 64 x 64 |  | Tech icon 'growspeed' (Sprout) - used by Fertile Soil 1, Fertile Soil 2, Fertile Soil 3, Fertile Soil 4, Fertile Soil 5, Fertile Soil 6, Fertile Soil 7, Fertile Soil 8 |
| `tree/icons/value.png` | 64 x 64 |  | Tech icon 'value' (Coins) - used by Prize Produce 1, Prize Produce 2, Prize Produce 3, Prize Produce 4, Prize Produce 5, Prize Produce 6, Prize Produce 7, Prize Produce 8, Prize Produce 9, Prize Produce 10 |
| `tree/icons/reach.png` | 64 x 64 |  | Tech icon 'reach' (Reach ring) - used by Wide Reach 1, Wide Reach 2, Wide Reach 3 |
| `tree/icons/carrots.png` | 64 x 64 |  | Tech icon 'carrots' (Carrot) - used by Carrots |
| `tree/icons/pumpkins.png` | 64 x 64 |  | Tech icon 'pumpkins' (Pumpkin) - used by Pumpkins |
| `tree/icons/autopick.png` | 64 x 64 |  | Tech icon 'autopick' (Sparkle) - used by Helping Hand |
| `tree/icons/autopickchance.png` | 64 x 64 |  | Tech icon 'autopickchance' (Clover) - used by Lucky Streak 1, Lucky Streak 2, Lucky Streak 3, Lucky Streak 4, Lucky Streak 5, Lucky Streak 6, Lucky Streak 7, Lucky Streak 8, Lucky Streak 9, Lucky Streak 10 |
| `tree/icons/autopickcount.png` | 64 x 64 |  | Tech icon 'autopickcount' (Bunch) - used by Bumper Bunch 1, Bumper Bunch 2, Bumper Bunch 3, Bumper Bunch 4, Bumper Bunch 5 |
| `tree/icons/autopickradius.png` | 64 x 64 |  | Tech icon 'autopickradius' (Ring) - used by Spreading Roots 1, Spreading Roots 2, Spreading Roots 3, Spreading Roots 4, Spreading Roots 5, Spreading Roots 6 |
| `tree/icons/farmhand.png` | 64 x 64 |  | Tech icon 'farmhand' (Farmer) - used by Farmhand |
| `tree/icons/farmcrew.png` | 64 x 64 |  | Tech icon 'farmcrew' (Two farmers) - used by Farm Crew 1, Farm Crew 2, Farm Crew 3, Farm Crew 4 |
| `tree/icons/farmerspeed.png` | 64 x 64 |  | Tech icon 'farmerspeed' (Boot) - used by Comfy Boots 1, Comfy Boots 2, Comfy Boots 3, Comfy Boots 4, Comfy Boots 5, Comfy Boots 6, Comfy Boots 7, Comfy Boots 8 |
| `tree/icons/farmerpick.png` | 64 x 64 |  | Tech icon 'farmerpick' (Shears) - used by Sharp Shears 1, Sharp Shears 2, Sharp Shears 3, Sharp Shears 4, Sharp Shears 5, Sharp Shears 6, Sharp Shears 7, Sharp Shears 8 |
| `tree/icons/barn.png` | 64 x 64 |  | Tech icon 'barn' (Barn) - used by The Barn |
| `tree/icons/crop_lettuce.png` | 64 x 64 |  | Tech icon 'crop_lettuce' (the Lettuce crop) |
| `tree/icons/crop_carrot.png` | 64 x 64 |  | Tech icon 'crop_carrot' (the Carrot crop) |
| `tree/icons/crop_pumpkin.png` | 64 x 64 |  | Tech icon 'crop_pumpkin' (the Pumpkin crop) |
| `tree/icons/crop_turnip.png` | 64 x 64 |  | Tech icon 'crop_turnip' (the Turnip crop) |
