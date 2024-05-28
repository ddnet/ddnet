package org.ddnet.client;

import android.app.NativeActivity;
import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.content.pm.ActivityInfo;

public class NativeMain extends SDLActivity {

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
}
