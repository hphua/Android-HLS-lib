package co.theengine.kaltura;

import java.io.File;
import java.io.IOException;

import junit.framework.TestCase;

import com.robotium.solo.Solo;

import android.app.Activity;
import android.test.ActivityInstrumentationTestCase2;
import android.test.InstrumentationTestCase;

@SuppressWarnings("unchecked")
public class TestPlayer extends ActivityInstrumentationTestCase2<Activity> {
	private static final String LAUNCHER_ACTIVITY_FULL_CLASSNAME = "com.example.videoplayer.VideoPlayerActivity";
	private static Class launcherActivityClass;
	static {
		try {
			System.out.println("WAY? " + LAUNCHER_ACTIVITY_FULL_CLASSNAME);
			launcherActivityClass = Class.forName(LAUNCHER_ACTIVITY_FULL_CLASSNAME);
			System.out.println("YAY");
		} catch (ClassNotFoundException e) {
			System.err.println("" + e);
			throw new RuntimeException(e);
		}
	}

	public TestPlayer() throws ClassNotFoundException {
		super(launcherActivityClass);
	}

	private Solo solo;
	
	/**
	 * Incremental counter for screenshot files.
	 */
	private int screenshotCounter = 0;

	@Override
	protected void setUp() throws Exception {
		solo = new Solo(getInstrumentation(), getActivity());
	}

	/**
	 * Take a screenshot and save it with a sequential file name.
	 */
	private void screenshot() {
		/*
		File f;
		try {
			File dir = new File("/sdcard/Robotium-Screenshots/");
			dir.mkdirs();
			
			String id = String.format("%04d", screenshotCounter++);
			f = new File(dir, "screenshot-"+id+".png");
			System.out.println("Screenshot path " + f);
			// takeScreenshot doesn't seem to be that reliable across devices/emulators
			//Boolean success = getUiDevice().takeScreenshot(f);
			//assertTrue("Unable to take a screenshot", success);
			Process process = Runtime.getRuntime().exec("screencap -p " + f.getAbsolutePath());
			try {
				process.waitFor();
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		//*/
		solo.takeScreenshot();
	}

	private void sleep(int time) {
		solo.sleep(time);
	}
	
	private void streamTest(String itemStream) {
		String itemPause = "Play/Pause";
		
		screenshot();
		
		sleep(2000);
		
		solo.clickOnMenuItem(itemStream);
		
		for (int i = 0; i < 5; i++) {
			sleep(5000);
			screenshot();
		}
		
		solo.clickOnMenuItem(itemPause);

		screenshot();
		sleep(2500);

		screenshot();
		sleep(2500);

		screenshot();
		
		solo.clickOnMenuItem(itemPause);

		screenshot();
		sleep(2500);
		
		screenshot();
		sleep(2500);
		
		screenshot();
		sleep(5000);
		
		screenshot();
	}
	
	public void testABC() {
		streamTest("ABC DVR");
	}
	
	public void testKalturaVOD() {
		streamTest("Kaltura VoD");
	}
	
	@Override
	public void tearDown() throws Exception {
		solo.finishOpenedActivities();
	}
}