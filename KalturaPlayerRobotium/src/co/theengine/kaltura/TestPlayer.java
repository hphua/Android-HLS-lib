package co.theengine.kaltura;

import android.app.Activity;
import android.test.ActivityInstrumentationTestCase2;

import com.robotium.solo.Solo;

@SuppressWarnings("unchecked")
public class TestPlayer extends ActivityInstrumentationTestCase2<Activity> {
	private static final String LAUNCHER_ACTIVITY_FULL_CLASSNAME = "com.example.videoplayer.VideoPlayerActivity";
	private static Class launcherActivityClass;
	
	/**
	 * Retrieve the tested application class.
	 */
	static {
		try {
			launcherActivityClass = Class.forName(LAUNCHER_ACTIVITY_FULL_CLASSNAME);
		} catch (ClassNotFoundException e) {
			throw new RuntimeException(e);
		}
	}

	public TestPlayer() throws ClassNotFoundException {
		super(launcherActivityClass);
	}

	private Solo solo;
	
	/**
	 * Incremental counter for screenshot files.
	 * Used with the alternative
	 */
	//private int screenshotCounter = 0;

	/**
	 * Initialize the test.
	 */
	@Override
	protected void setUp() throws Exception {
		solo = new Solo(getInstrumentation(), getActivity());
	}

	/**
	 * Take a screenshot and save it with a sequential file name.
	 */
	private void screenshot() {
		solo.takeScreenshot();
		// Alternative way to screenshot (doesn't work for Robotium apparently)
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
			
			System.out.println("SCREENSHOTTING!!!! " + "screencap -p " + f.getAbsolutePath());
			
			Process process = Runtime.getRuntime().exec("screencap -p " + f.getAbsolutePath());
			try {
				System.out.println("Waiting...");

				System.out.println("OUTPUT: "+IOUtils.toString(process.getInputStream()));
				System.out.println("ERRORS: "+IOUtils.toString(process.getErrorStream()));
				
				process.waitFor();
				
				System.out.println("EXIT: "+process.exitValue());
				
				System.out.println("Done!");
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				System.out.println("Interrupted! " + e);
				e.printStackTrace();
			}
		} catch (IOException e) {
			System.out.println("IOException! " + e);
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		//*/
	}
	
	/**
	 * Sleep for the provided amount of milliseconds.
	 */
	private void sleep(int time) {
		solo.sleep(time);
	}
	
	/**
	 * Test the provided stream menu option with screenshots and pause testing.
	 * @param itemStream	Menu option name to test.
	 */
	private void streamTest(String itemStream) {
		String itemPause = "Play/Pause";
		
		screenshot();
		
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
	
	/**
	 * Test the ABC live stream via the menu option.
	 */
	public void testABC() {
		streamTest("ABC DVR");
	}
	
	/**
	 * Test the Kaltura video on demand via the menu option.
	 */
	public void testKalturaVOD() {
		streamTest("Kaltura VoD");
	}

	/**
	 * Test switching between ABC and Kaltura streams.
	 */
	public void testSwitch() {
		solo.clickOnMenuItem("ABC DVR");

		screenshot();
		solo.sleep(5000);
		screenshot();
		
		solo.clickOnMenuItem("Kaltura VoD");

		screenshot();
		solo.sleep(5000);
		screenshot();
		
		solo.clickOnMenuItem("ABC DVR");
		
		screenshot();
		solo.sleep(5000);
		screenshot();
		
		solo.clickOnMenuItem("Kaltura VoD");
		
		screenshot();
		solo.sleep(5000);
		screenshot();
	}
	
	/**
	 * Test seeking forward and backward on the Kaltura stream.
	 */
	public void testKalturaVODSeek() {
		solo.clickOnMenuItem("Kaltura VoD");

		screenshot();
		solo.sleep(3000);
		screenshot();

		solo.clickOnMenuItem("Seek Fwd");

		screenshot();
		solo.sleep(2000);
		screenshot();
		
		solo.clickOnMenuItem("Seek Fwd");

		screenshot();
		solo.sleep(2000);
		screenshot();
		
		solo.clickOnMenuItem("Seek Backward");

		screenshot();
		solo.sleep(4000);
		screenshot();
		
		solo.clickOnMenuItem("Seek Backward");

		screenshot();
		solo.sleep(4000);
		screenshot();
		
		solo.clickOnMenuItem("Seek Forward");

		screenshot();
		solo.sleep(2000);
		screenshot();
	}
	
	/**
	 * Finish the test.
	 */
	@Override
	public void tearDown() throws Exception {
		solo.finishOpenedActivities();
	}
}