package org.ddnet.client;

import android.app.NativeActivity;
import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.content.Intent;
import android.content.pm.ActivityInfo;

public class NativeMain extends SDLActivity {

	private static final int COMMAND_RESTART_APP = SDLActivity.COMMAND_USER + 1;

	private String[] launchArguments = new String[0];

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"DDNet",
		};
	}

	@Override
	public void onCreate(Bundle SavedInstanceState) {
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

		Intent intent = getIntent();
		if(intent != null) {
			String gfxBackend = intent.getStringExtra("gfx-backend");
			if(gfxBackend != null) {
				if(gfxBackend.equals("Vulkan")) {
					launchArguments = new String[] {"gfx_backend Vulkan"};
				} else if(gfxBackend.equals("GLES")) {
					launchArguments = new String[] {"gfx_backend GLES"};
				}
			}
		}

		super.onCreate(SavedInstanceState);
	}

	@Override
	protected String[] getArguments() {
		return launchArguments;
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
