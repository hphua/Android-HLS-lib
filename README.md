# Android-HLS-lib
Kaltura Android HLS Library

General Notes
-------------
This project is part of Kaltura's Android Player SDK which allows the playback and integration of Kaltura's video player into native Android apps.
The purpose of this project is to support native HLS streams playback on Android devices.
For more information regarding the Kaltura Android Player SDK go to: https://github.com/kaltura/player-sdk-native-android

Installation Notes
------------------
In order to add a new drop of the HLS library into the Android Player SDK one should take the following steps:

1. clone the repository
2. run the following command from within the HLSPlayerSDK folder:
  [local Android SDK path]/android-ndk-r10c/ndk-build
3. navigate to HLSPlayerSDK/libs
4. generate a new folder consisting of all the folders in there and name it "lib"
5. zip that folder into and archive and rename the archive to "lib.jar"
6. copy the jar file into the Android player SDK project: player-sdk-native-android/hLSPlayerSDK/libs/
7. copy the "com" folder inside the HLSPlayerSDK
8. past the "com" folder into the Android Player SDK project: player-sdk-native-android/hLSPlayerSDK/src/main/java
9. overwrite if needed
