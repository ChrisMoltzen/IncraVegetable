package com.incravegetable.game;

import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.PlayGamesSdk;

import org.libsdl.app.SDLActivity;

/**
 * The game's Android activity. SDLActivity does the work: it loads libSDL3.so and
 * libmain.so (the game, built from the project's CMakeLists.txt) and runs it.
 *
 * It also passes achievements to Google Play Games: src/PlatformServices.cpp calls
 * hasPlayGames() and unlockAchievement() through JNI. Play Games is only used once
 * res/values/games-ids.xml has your game's ids from the Play Console.
 */
public class IncraVegetableActivity extends SDLActivity {
    private static final String TAG = "IncraVegetable";
    private boolean playGames = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String projectId = getString(R.string.game_services_project_id).trim();
        if (!projectId.isEmpty() && !projectId.matches("0+")) {
            // Signs the player in automatically (Play Games v2 needs no sign-in button).
            PlayGamesSdk.initialize(this);
            playGames = true;
        } else {
            Log.i(TAG, "Play Games not set up (games-ids.xml): achievements stay in the game");
        }
    }

    /** Called from C++: is Play Games on? */
    public boolean hasPlayGames() {
        return playGames;
    }

    /**
     * Called from C++ with the game's achievement id, e.g. "green_thumb". Play Games'
     * own id for it is the string resource achievement_green_thumb in games-ids.xml.
     */
    public void unlockAchievement(String id) {
        if (!playGames) return;
        int res = getResources().getIdentifier("achievement_" + id, "string", getPackageName());
        if (res == 0) {
            Log.w(TAG, "No achievement_" + id + " in games-ids.xml (add it in the Play Console)");
            return;
        }
        final String playId = getString(res);
        runOnUiThread(() -> PlayGames.getAchievementsClient(this).unlock(playId));
    }
}
