Changelog
---------

#### Version 1.2.0 (Beta 3)

##### Bugfixes

    - Fix error in incoming message handling that caused segmentation faults
    - Fix failing audio and video transfers
    - Mark messages read on other devices as 'deferred' to avoid needless notifications
    - Improved buddy list handling (should avoid "unknown" contacts popping up)
    - Many small issues (see commit history)
    - Fix issues that caused audio and video downloads to not have a proper file ending
    - Fix issue that caused very old unused chats to not be available

###### Features

    - Allow creation of new group chats
    - Allow joining chats by invite link
    - Allow exporting chats by invite link
    - Support displaying and sending GIFs 
    - Support joining chats by invite link

##### Version 1.1.0 (Beta 2)

    - update to libtgl 2
    - support two-factor authentication
    - improve sticker support
    - add option to control message read recipes
    - many bugfixes


##### Version 1.0.0 (Beta 1)

    - Move state files into .purple/telegram-purple directory

    - Add extended account settings for controlling history and read notifications

    - Improved support for Adium
        * fix chat bookmarking
        * add custom views for account settings and chat joining


##### Version 0.7

    - Support uploads and downloads (with some caveats though):
         * Uploads wont contain the current file name but a generic one
         * Every file is uploaded as a document or picture, so unfortunately no embedded videos and audio (right now)
         * Group chats only support picture uploads since nothing else is supported by the UI

    - Fix HTML escaping issues

##### Version 0.6.1

    - Fix many stability issues
    - Do not readd left chats on login

##### Version 0.6

    - Support for secret chats 
    - Receiving geo messages

##### Version 0.5

    - Display incoming photos
    - Respect received user and chat property updates
    - Support changing own profile picture
    - Support adding new contacts
    - Display service messages
    - Works with libpurple proxy settings

##### TODO:

    - Picture Uploads
    - Audio, Video and File Transfers

