package org.ddnet.client;

import android.app.NativeActivity;
import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.content.Intent;
import android.content.pm.ActivityInfo;

public class NativeMain extends SDLActivity {

	private static final int COMMAND_RESTART_APP = SDLActivity.COMMAND_USER + 1;

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"DDNet",
		};
	}

	@Override
	public void onCreate(Bundle SavedInstanceState) {
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		super.onCreate(SavedInstanceState);
	}

	@Override
	protected boolean onUnhandledMessage(int command, Object param) {
		if(command == COMMAND_RESTART_APP) {
			restartApp();
			return true;
		}
        return false;
    }

	private void restartApp() {
		Intent restartIntent =
			Intent.makeRestartActivityTask(
				getPackageManager().getLaunchIntentForPackage(
					getPackageName()
				).getComponent()
			);
		restartIntent.setPackage(getPackageName());
		startActivity(restartIntent);
	}
}
