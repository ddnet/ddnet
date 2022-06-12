package tw.DDNet;
import android.app.NativeActivity;
import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.content.pm.ActivityInfo;

public class NativeMain extends SDLActivity {
	static {
		System.loadLibrary("DDNet");
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			// disable hid API for now 
			// "hidapi",
			// "SDL2",
			"DDNet",
		};
	}

	@Override
	public void onCreate(Bundle SavedInstanceState) {
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		super.onCreate(SavedInstanceState);
	}
}
