[33mcommit 4e703a206e57c2a22c0bad658af154f201ee43ad[m[33m ([m[1;36mHEAD -> [m[1;32mmaster[m[33m, [m[1;31morigin/master[m[33m)[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Aug 2 23:48:37 2020 +0800

    Added keyword disabling, and removed databases completely, also removed some includes and more platform boundary more clear

[33mcommit 47648e6e42182e8f5158d77e283bd96d76224780[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Aug 1 00:32:58 2020 +0800

    Changed so imgui and icons are loaded and unloaded as window is hidden shown, and removed Database because it was unnecessary

[33mcommit e59ea33d2d573b955ec7b3935e9770f0b8b4176e[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jul 28 23:11:04 2020 +0800

    Got timing/sleep working, so app can change between low update background polling and 60fps ui

[33mcommit e75f7c793191e38a90ce4702c9feea44c9750cf9[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jul 28 21:05:19 2020 +0800

    Worked on timing code

[33mcommit 9888f2b43f8e23442e7843b9d79889dd519acff8[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jul 28 00:19:29 2020 +0800

    Added better sleeping code, and added stuff to icon asset system

[33mcommit 74d2edfc22877f80f68484151de3b7d5d894bddc[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Mon Jul 27 00:09:58 2020 +0800

    Added timing to limit poll rate and sleep, except when in ui mode

[33mcommit fc9a94214ddc30308c048fb1c4a2a08b2faf3410[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Jul 24 22:40:18 2020 +0800

    Got tray icon stuff working, and barplot layout

[33mcommit 3384db85bf4f3f6104e839c0bd40acef4590aac3[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jul 19 00:09:31 2020 +0800

    worked on styling, and added custom website default icon that is converted from a png to a cpp fileand compiled in

[33mcommit 4839e73fbcbf248cd3b1360925d5a63a6312a5de[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Jul 18 00:09:01 2020 +0800

    changed to using arrays for id -> names translation, and added compressed font to source code support so font files don't have to be opened and distributed

[33mcommit 6a06980eb1dfda205f0272489a1fcaa1dfa88080[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Jul 16 19:40:15 2020 +0800

    Going to work on memory arenas

[33mcommit ce58fc754160e79baed062d4d4c5cf4034833e40[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jul 12 01:03:42 2020 +0800

    extract domain bugfix

[33mcommit 766b0a993dd98b04b60cefa3b468d48291dec206[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jul 12 00:08:38 2020 +0800

    changes keywords to only accept domain names, and to make a hash table for domains just like local programs'

[33mcommit db403a618c62ec126a649f8b7efc334c40c7be2a[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Jul 11 20:18:58 2020 +0800

    before changing up keyword system

[33mcommit 00ccee1457d75940d70e5c0965ce50a0c39d18f8[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Jul 9 22:52:35 2020 +0800

    Finished data picker and calendar

[33mcommit 631fddfc2f750e09e6bbc5fbd18c375dc02dbc42[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Jul 9 00:25:34 2020 +0800

    mostly finished date picker backwards and forwards, displaying, custom date selection

[33mcommit 3c2ed0a3438065d4210684525f9d17cb8ab6edc0[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Jul 4 23:42:28 2020 +0800

    Fixed getting urls from fullscreen firefox

[33mcommit e938d3ce2e6de7442a4cfdb8ec280377c9116dc9[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Jul 4 00:27:19 2020 +0800

    Got day_view ranges working, and setup a debug window to show all internal state

[33mcommit 922a99e953f52ddae28b32cb7fd98b510d68c87e[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jun 30 00:12:12 2020 +0800

    cleaned up calendar and added a second calendar button. Only add font glyphs that are used

[33mcommit c947f3f4de8e32d97fafe6cf7446245b56e07918[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Mon Jun 29 00:27:58 2020 +0800

    fixed bugs in calendar and finished mostly, switched to using localtime instead of utc

[33mcommit 44daa57fc16d86d6aa4488536c58b881977db32d[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jun 28 00:59:23 2020 +0800

    mostly finished calendar selector functionality

[33mcommit 537bb2cee073ffc81c02a380803d95f7d63cd6cb[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jun 28 00:22:22 2020 +0800

    Working on ui

[33mcommit e65004531a7dab96cdd15bb19c31a0ca783a5741[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Jun 26 23:54:28 2020 +0800

    experimented with timing precision

[33mcommit 71ae9a3a0c0fec8a919ee0242c6f2bc80184a651[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Jun 25 20:53:41 2020 +0800

    Settings mostly done, starting record/day storage structural change

[33mcommit f7d8407ddb77f00b0b2a6d53d38f3e0c269b4630[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jun 23 22:54:28 2020 +0800

    Got uploading icon texture to gpu working, so on demand loading of icons works

[33mcommit 617a461b464e156f6e58a9e920fa8c86f3a95c8a[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Mon Jun 22 00:17:05 2020 +0800

    Tested and renamed some stuff

[33mcommit 43be4026f3998dd0e0c0ec71edee9f5151625d51[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jun 21 22:57:03 2020 +0800

    Got settings menu in place, all seems to work well ATM

[33mcommit d052c1f0ee5eba0027bff0d0cb5d9f52b7f910ed[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Jun 19 23:43:28 2020 +0800

    Reached parity with previous custom version

[33mcommit 1e240059f80e4935d6e779f31f50105002513b0c[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Jun 18 22:11:29 2020 +0800

    Got SDL with imgui and freetype up and running, still need to integrate with actual monitor code now

[33mcommit 4a75fb449347a9c5148fa3ed8efbef7fe05cd587[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Wed Jun 17 18:17:35 2020 +0800

    Before change to SDL imgui

[33mcommit b0e8731614dcd764b6ff203e530842c838f236fc[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Wed Apr 22 00:28:24 2020 +0800

    fixed up some problems in win32 gui code

[33mcommit 42499ef796509a8dd8bc4aad6f51c3bb59c4edf6[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Apr 21 23:53:31 2020 +0800

    added win32 options menu to add user keywords

[33mcommit 2849c1d17dca13e75f6e49cb754b9dfec06b8cc3[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Apr 18 00:42:56 2020 +0800

    implemented scrolling and chaned ui code to mostly use rects, to allow easier clipping

[33mcommit 7a082012345cbe1668acb411e038590ba27ac277[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Apr 17 21:56:30 2020 +0800

    Going to start a rework to use Rects instead of xywh in ui code

[33mcommit d1b8d70efa27628bf2344aee12128dd6b8b7e567[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Apr 12 22:36:26 2020 +0800

    Cleaned up win32 icon from executable code

[33mcommit f805ad25db54d0668c4f2e589c195e7d97f40ade[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Apr 10 23:52:59 2020 +0800

    stopped trying to get favicons for now, will come back to later. Unified websites and programs so both are treated the same. Now renders program/website names and icons (firefox for websites) in graph

[33mcommit 44a2c8b1a5b4f7abd2adf8708822034074324677[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Apr 4 16:55:53 2020 +0800

    Fixed crashing when memory freed in firefox automation code, due to strcpy bug

[33mcommit 6457a10b96d950c1707d682a6f23c36d74729b72[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Apr 2 23:54:31 2020 +0800

    updated uiautomation code

[33mcommit d126a1086cb85ee5d62e0af55132b10bc15ee9da[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Wed Apr 1 22:46:13 2020 +0800

    Rendering of buttons and graph with icons in imgui system

[33mcommit 6d0fe16f0d62df20b771397332495b8c33c00726[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Wed Apr 1 16:41:19 2020 +0800

    Got immediate mode ui buttons working

[33mcommit 5884b2949e49fa9e4e94c6c8b076ff1478d83647[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Mon Mar 30 23:18:46 2020 +0800

    Made some major changes, using hash tables for websites and programs, and cleaning up poll windows code

[33mcommit 05a7988ccc83ea58c3f86d2ebf1d60e6d0fce81b[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Mon Mar 30 00:08:10 2020 +0800

    Started transitioning to a immediate mode ui system, and started making the platform app api boundary more clear, doesn't compile

[33mcommit f8a425152b18c961a86faa863bdf701d2b78e876[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Mar 26 00:06:12 2020 +0800

    Removed winhttp code and replaced with curl, added string library code mainly to parse html

[33mcommit 8d68dddde369173a36722ec26a7bde2fbf4feb9a[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Mar 19 22:51:44 2020 +0800

    Added system to request favicon location from website prior to getting icon

[33mcommit e884fe9d805b219176d5f6d798cfbc41c0a140b0[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Mar 14 00:29:45 2020 +0800

    researched ico format, and started to parse icon file data

[33mcommit 7a89c8e43f77c4d1cc87ddc1ffb4085c917c4950[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Mar 13 00:08:00 2020 +0800

    Started downloading icons from the web. The basics seem to work but have to parse icon file still.

[33mcommit 595fa3d102ccc61cf45ba5f16efa436085ad9bb4[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Mar 3 23:13:51 2020 +0800

    formalised the concept of website records, they now are stored separtely, are given the firefox icon, and their own ids. Currently not rendering all website records.

[33mcommit 914a70d163928a093e8e379fecaddb178db45a52[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Mar 1 23:11:05 2020 +0800

    Made API sections more clear for platform dependent/independent/rendering/core code. Solidified what parts of the code are being updated each polling period. Put on demand loading of icons in.

[33mcommit ce0058143844fcca427a0e2d9302740532268cf7[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Thu Feb 27 19:45:41 2020 +0800

    added bar graphs, with times and icons sorted  for a days records, updated when user closes 'hides' program. Text rendered with stb

[33mcommit 5e60569dc04ba96a7efa58b16a62020d66316855[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Feb 21 22:57:23 2020 +0800

    started getting icons from other programs and drawing them to screen, along with a simple bar plot tracking todays program times

[33mcommit 6285bbfbee4962566dbd5ba58dfbf9de79caa62b[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Feb 18 20:12:54 2020 +0800

    Reworked file system and allow for printing of periods of records

[33mcommit 6849fe372edcf3f8cf7a36d42e7486bd451b5bae[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Feb 16 18:28:05 2020 +0800

    Added data.h library to project instead of my date.cpp code, will start replacing Date things with sys_days and year_month_day

[33mcommit f688fee6218ae47926e8b8d4123e4493294e4aa1[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Wed Feb 12 22:03:33 2020 +0800

    Implemented concept of days, and got basic hide/show from a tray icon working. Added resource #defines

[33mcommit 0b61e839de7a5245e29c9c4dc106dd54c8ec58b9[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Feb 9 23:07:54 2020 +0800

    added savefile writing/reading, saving of different days, hash table for names

[33mcommit 658cd88e93d8c3927649cea94ce3d04ab2043e3a[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sun Jan 26 16:18:39 2020 +0800

    Starting huge resimplification, and structure change

[33mcommit 24de552a5a96f92e9d7d30607d7b1ec28c178056[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Tue Jun 25 20:08:29 2019 +0800

    Wrote code to do loop and update file when needed etc (not compiling). Going to restructure to nnot retain records in memory

[33mcommit 523f4e3abdb8a8489370e3cb3e464822ce56568c[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Sat Jun 22 17:24:30 2019 +0800

    Now writes to database file, and creates a win32 gui

[33mcommit 13ae320897a9847895b5c742a1526736fb5daae0[m
Author: Mac Twomey <bdlcmt@hotmail.com>
Date:   Fri Jun 14 01:29:53 2019 +0800

    Initial commit
