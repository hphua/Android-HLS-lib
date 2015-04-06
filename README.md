# Android-HLS-lib
Kaltura Android HLS Library

General Notes
-------------
This project is part of Kaltura's Android Player SDK which allows the playback and integration of Kaltura's video player into native Android apps.
The purpose of this project is to support native HLS streams playback on Android devices.
For more information regarding the Kaltura Android Player SDK go to: https://github.com/kaltura/player-sdk-native-android

Technical Overview
------------------
For a technical overview of the project go to: https://github.com/kaltura/Android-HLS-lib/wiki/Technical-Overview

Installation Notes
------------------
In order to add a new drop of the HLS library into the Android Player SDK one should take the following steps:

1. Clone the project repository
2. Run the following command from within the HLSPlayerSDK folder:
  [local Android SDK path]/android-ndk-r10c/ndk-build
3. Navigate to HLSPlayerSDK/libs
4. Create a new folder consisting of all the folders in the libs folder and name it "lib"
5. Zip that folder into an archive and rename the archive to "lib.jar"
6. Copy the jar file into the Android player SDK project: player-sdk-native-android/hLSPlayerSDK/libs/
7. Copy the "com" folder inside the HLSPlayerSDK
8. Paste the "com" folder into the Android Player SDK project: player-sdk-native-android/hLSPlayerSDK/src/main/java
9. Overwrite if needed
